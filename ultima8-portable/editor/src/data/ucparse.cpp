#include "data/ucparse.h"

#include <QObject>
#include <QStringList>

#include <stdexcept>

using namespace Ucast;

namespace Ucparse
{

namespace
{

struct Bad
{
	QString why;
};

[[noreturn]] void bad(const QString &why)
{
	throw Bad{why};
}

// ---- Expressions: a recursive descent parser over one line's text ----

class ExprParser
{
public:
	ExprParser(const QString &text) : t_(text) {}

	E parse()
	{
		E e = binary(0);
		skip();
		return e;
	}
	bool atEnd()
	{
		skip();
		return i_ >= t_.size();
	}
	// Consumes the text if it comes next.
	bool take(const QString &s)
	{
		skip();
		if (t_.mid(i_, s.size()) == s)
		{
			i_ += s.size();
			return true;
		}
		return false;
	}
	void expect(const QString &s)
	{
		if (!take(s))
			bad(QObject::tr("expected %1 at '%2'").arg(s, t_.mid(i_, 12)));
	}
	int pos() const { return i_; }
	QString rest() const { return t_.mid(i_); }

private:
	const QString t_;
	int i_ = 0;

	void skip()
	{
		while (i_ < t_.size() && t_[i_].isSpace())
			i_++;
	}

	// The binary operator at the cursor and its length, or "".
	QString peekOp()
	{
		skip();
		static const char *const ops[] = {"||", "&&", "==", "!=", "<=", ">=", "<<", ">>", "<", ">", "+", "-", "*", "/",
			"%", "|", "&", "^"};
		for (const char *op : ops)
		{
			const QString o = QString::fromLatin1(op);
			if (t_.mid(i_, o.size()) == o)
			{
				// "&&" before "&", "<<" before "<": the list is in that order. A '&' that starts
				// "&&" is taken above; one here is bitwise.
				return o;
			}
		}
		// "in" as a word.
		if (t_.mid(i_, 2) == QLatin1String("in") && (i_ + 2 >= t_.size() || t_[i_ + 2].isSpace()))
			return QStringLiteral("in");
		return QString();
	}

	E binary(int minPrec)
	{
		E left = unary();
		for (;;)
		{
			const QString op = peekOp();
			if (op.isEmpty())
				break;
			const int p = prec(op);
			if (p == 0 || p < minPrec)
				break;
			i_ += op.size();
			E right = binary(p + 1);
			left = make(Expr::Bin, op, {left, right});
		}
		return left;
	}

	QString word()
	{
		skip();
		int j = i_;
		while (j < t_.size() && (t_[j].isLetterOrNumber() || t_[j] == '_' || t_[j] == ':'))
			j++;
		const QString w = t_.mid(i_, j - i_);
		i_ = j;
		return w;
	}

	E unary()
	{
		skip();
		if (i_ >= t_.size())
			bad(QObject::tr("an expression is expected"));
		const QChar c = t_[i_];
		if (c == '!' && t_.mid(i_, 2) != QLatin1String("!="))
		{
			i_++;
			return make(Expr::Un, QStringLiteral("!"), {unary()});
		}
		if (c == '~')
		{
			i_++;
			return make(Expr::Un, QStringLiteral("~"), {unary()});
		}
		if (c == '*')
		{
			i_++;
			return make(Expr::Deref, QString(), {unary()});
		}
		if (c == '&')
		{
			i_++;
			skip();
			if (i_ < t_.size() && t_[i_] == '(')
			{
				i_++;
				E inner = binary(0);
				expect(QStringLiteral(")"));
				return make(Expr::Ref, QString(), {inner});
			}
			return make(Expr::Addr, QString(), {unary()});
		}
		if (c == '-' && i_ + 1 < t_.size() && t_[i_ + 1].isDigit())
		{
			i_++;
			return konst(-number());
		}
		if (c == '(')
		{
			// A cast, "(long)x", or a parenthesised expression.
			const int save = i_;
			i_++;
			const QString w = word();
			if ((w == QLatin1String("long") || w == QLatin1String("int") || w == QLatin1String("ptr")) && take(QStringLiteral(")")))
				return make(Expr::Cast, w, {unary()});
			i_ = save + 1;
			E inner = binary(0);
			expect(QStringLiteral(")"));
			return inner;
		}
		if (c == '"')
			return string();
		if (c.isDigit())
		{
			// A number, or a class::routine name ("124::10c3").
			int j = i_;
			while (j < t_.size() && t_[j].isLetterOrNumber())
				j++;
			if (t_.mid(j, 2) == QLatin1String("::"))
				return named();
			return konst(number());
		}
		return named();
	}

	int64_t number()
	{
		skip();
		int j = i_;
		bool hex = t_.mid(i_, 2) == QLatin1String("0x");
		if (hex)
			j += 2;
		while (j < t_.size() && (hex ? t_[j].isLetterOrNumber() : t_[j].isDigit()))
			j++;
		bool ok;
		const int64_t v = t_.mid(i_, j - i_).toLongLong(&ok, 0);
		if (!ok)
			bad(QObject::tr("bad number '%1'").arg(t_.mid(i_, j - i_)));
		i_ = j;
		return v;
	}

	E string()
	{
		i_++;	// the quote
		QByteArray bytes;
		while (i_ < t_.size() && t_[i_] != '"')
		{
			if (t_[i_] == '\\' && i_ + 3 < t_.size() && t_[i_ + 1] == 'x')
			{
				bool ok;
				bytes.append(char(t_.mid(i_ + 2, 2).toInt(&ok, 16)));
				if (!ok)
					bad(QObject::tr("bad escape in a string"));
				i_ += 4;
			}
			else
				bytes.append(char(t_[i_++].unicode()));
		}
		if (i_ >= t_.size())
			bad(QObject::tr("a string is not closed"));
		i_++;
		return str(bytes);
	}

	E named()
	{
		QString w = word();
		if (w.isEmpty())
			bad(QObject::tr("unexpected '%1'").arg(t_.mid(i_, 12)));
		if (w == QLatin1String("flag") && take(QStringLiteral("[")))
		{
			const int64_t bit = number();
			int width = 1;
			if (take(QStringLiteral(":")))
				width = int(number());
			expect(QStringLiteral("]"));
			return flag(bit, width);
		}
		if (w == QLatin1String("spawn"))
			w += QStringLiteral(" ") + word();
		if (take(QStringLiteral("(")))
		{
			std::vector<E> args;
			if (!take(QStringLiteral(")")))
			{
				do
					args.push_back(binary(0));
				while (take(QStringLiteral(",")));
				expect(QStringLiteral(")"));
			}
			int64_t local = 0;
			if (w.startsWith(QLatin1String("search_")) && w != QLatin1String("search_next"))
				local = 0;	// set by the foreach that holds it
			return make(Expr::Call, w, args, local);
		}
		if (w == QLatin1String("pid"))
			return make(Expr::Pid);
		if (w == QLatin1String("finder"))
			return make(Expr::Finder);
		return var(w);
	}
};

E parseExpr(const QString &text)
{
	ExprParser p(text);
	E e = p.parse();
	if (!p.atEnd())
		bad(QObject::tr("unexpected '%1'").arg(p.rest().left(16)));
	return e;
}

// The offset a local_N / arg_N name stands for.
int64_t offsetOf(const QString &name)
{
	if (name.startsWith(QLatin1String("local_")))
		return -name.mid(6).toLongLong();
	if (name.startsWith(QLatin1String("arg_")))
		return name.mid(4).toLongLong();
	bad(QObject::tr("%1 is not a local or an argument").arg(name));
}

// The position of the first " = " outside strings and brackets, or -1.
int assignAt(const QString &line)
{
	int depth = 0;
	bool inString = false;
	for (int i = 0; i < line.size(); i++)
	{
		const QChar c = line[i];
		if (inString)
		{
			if (c == '\\')
				i++;
			else if (c == '"')
				inString = false;
			continue;
		}
		if (c == '"')
			inString = true;
		else if (c == '(' || c == '[')
			depth++;
		else if (c == ')' || c == ']')
			depth--;
		else if (depth == 0 && line.mid(i, 3) == QLatin1String(" = "))
			return i;
	}
	return -1;
}

// ---- Statements: a line at a time ----

class BlockParser
{
public:
	BlockParser(const QStringList &lines) : lines_(lines) {}

	Block block()
	{
		Block out;
		while (i_ < lines_.size())
		{
			const QString line = lines_[i_].trimmed();
			if (line == QLatin1String("}") || line == QLatin1String("} else {"))
				break;
			out.push_back(statement());
		}
		return out;
	}
	bool done() const { return i_ >= lines_.size(); }
	QString closing() { return lines_[i_].trimmed(); }
	void advance() { i_++; }

private:
	const QStringList &lines_;
	int i_ = 0;

	[[noreturn]] void failHere(const QString &why) { bad(QObject::tr("%1 in '%2'").arg(why, lines_[i_].trimmed())); }

	S statement()
	{
		const QString line = lines_[i_].trimmed();
		if (line.startsWith(QLatin1String("if (")) && line.endsWith(QLatin1String(") {")))
			return ifStatement(line);
		if (line.startsWith(QLatin1String("while (")) && line.endsWith(QLatin1String(") {")))
			return whileStatement(line);
		if (line.startsWith(QLatin1String("foreach (")) && line.endsWith(QLatin1String(") {")))
			return foreachStatement(line);
		if (!line.endsWith(QChar(';')))
			failHere(QObject::tr("a statement is expected"));
		i_++;
		return simple(line.left(line.size() - 1));
	}

	S simple(const QString &text)
	{
		if (text.startsWith(QLatin1String("if (!")) && text.endsWith(QLatin1String(") break")))
			return plain(text);
		static const char *const plains[] = {"set_process_info(", "exclusive()", "suspend()", "return", "break", "continue"};
		for (const char *p : plains)
			if (text.startsWith(QLatin1String(p)))
			{
				if (text.startsWith(QLatin1String("set_process_info(")))
				{
					// Both arguments must parse, as expressions.
					ExprParser ep(text.mid(17));
					ep.parse();
					ep.expect(QStringLiteral(","));
					ep.parse();
					ep.expect(QStringLiteral(")"));
				}
				return plain(text);
			}
		if (text.startsWith(QLatin1String("await ")))
		{
			S s = Ucast::stmt(Stmt::Wait);
			s->expr = parseExpr(text.mid(6));
			return s;
		}
		if (text.startsWith(QLatin1String("implies(")) && text.endsWith(QChar(')')))
		{
			ExprParser ep(text.mid(8));
			S s = Ucast::stmt(Stmt::Implies);
			s->cond = ep.parse();
			ep.expect(QStringLiteral(","));
			s->expr = ep.parse();
			ep.expect(QStringLiteral(")"));
			return s;
		}
		const int eq = assignAt(text);
		if (eq < 0)
		{
			S s = Ucast::stmt(Stmt::Call);
			s->expr = parseExpr(text);
			if (s->expr->kind != Expr::Call)
				bad(QObject::tr("'%1' is not a statement").arg(text));
			return s;
		}
		const QString lhs = text.left(eq);
		E value = parseExpr(text.mid(eq + 3));
		if (lhs.startsWith(QLatin1String("tmp")) && value->kind == Expr::Call)
		{
			S s = Ucast::stmt(Stmt::Call);
			s->var = lhs;
			s->expr = value;
			return s;
		}
		S s = Ucast::stmt(Stmt::Assign);
		s->lhs = lhs;
		s->expr = value;
		return s;
	}

	void close(const QString &what)
	{
		if (done() || closing() != QLatin1String("}"))
			bad(QObject::tr("%1 is not closed").arg(what));
		i_++;
	}

	S ifStatement(const QString &line)
	{
		S s = Ucast::stmt(Stmt::If);
		s->cond = parseExpr(line.mid(4, line.size() - 4 - 3));
		i_++;
		s->a = block();
		if (!done() && closing() == QLatin1String("} else {"))
		{
			i_++;
			s->hasElse = true;
			s->b = block();
		}
		close(QStringLiteral("if"));
		return s;
	}

	S whileStatement(const QString &line)
	{
		S s = Ucast::stmt(Stmt::While);
		const QString cond = line.mid(7, line.size() - 7 - 3);
		i_++;
		Block body = block();
		close(QStringLiteral("while"));
		if (cond == QLatin1String("true"))
		{
			// "while (true)" is endless, unless a line "if (!test) break;" splits a test from what
			// it needs first (the printer's form of a loop whose test follows statements).
			for (size_t k = 0; k < body.size(); k++)
				if (body[k]->kind == Stmt::Plain && body[k]->text.startsWith(QLatin1String("if (!")))
				{
					s->cond = parseExpr(body[k]->text.mid(5, body[k]->text.size() - 5 - 7));
					s->pre.assign(body.begin(), body.begin() + k);
					s->a.assign(body.begin() + k + 1, body.end());
					return s;
				}
			s->endless = true;
			s->a = std::move(body);
			return s;
		}
		s->cond = parseExpr(cond);
		s->a = std::move(body);
		return s;
	}

	S foreachStatement(const QString &line)
	{
		S s = Ucast::stmt(Stmt::While);
		const QString inner = line.mid(9, line.size() - 9 - 3);
		const int in = inner.indexOf(QLatin1String(" in "));
		if (in < 0)
			failHere(QObject::tr("foreach needs 'in'"));
		const QString v = inner.left(in);
		s->cond = parseExpr(inner.mid(in + 4));
		if (s->cond->kind == Expr::Call && s->cond->s.startsWith(QLatin1String("search_")) && s->cond->s != QLatin1String("search_next"))
		{
			auto e = std::make_shared<Expr>(*s->cond);
			e->v = offsetOf(v);
			s->cond = e;
		}
		else
		{
			s->each = true;
			s->lhs = v;
		}
		i_++;
		s->a = block();
		close(QStringLiteral("foreach"));
		return s;
	}
};

}

bool parseBlock(const QString &text, Block *out, QString *error)
{
	QStringList lines = text.split(QChar('\n'), Qt::SkipEmptyParts);
	try
	{
		// "if (!test) break;" is one line that is not a block: it parses as a plain statement.
		BlockParser p(lines);
		*out = p.block();
		if (!p.done())
			bad(QObject::tr("an unexpected '%1'").arg(p.closing()));
		return true;
	}
	catch (const Bad &b)
	{
		if (error)
			*error = b.why;
		return false;
	}
}

}
