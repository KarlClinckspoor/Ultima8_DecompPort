#include "data/ucast.h"

#include <QHash>

namespace Ucast
{

QString varNameOf(int64_t off)
{
	return off < 0 ? QStringLiteral("local_%1").arg(-off) : QStringLiteral("arg_%1").arg(off);
}

std::shared_ptr<Expr> newExpr(Expr::Kind kind)
{
	auto e = std::make_shared<Expr>();
	e->kind = kind;
	return e;
}

E flag(int64_t bit, int width)
{
	auto e = std::make_shared<Expr>();
	e->kind = Expr::Flag;
	e->v = bit;
	e->w = width;
	return e;
}

E str(const QByteArray &bytes)
{
	auto e = std::make_shared<Expr>();
	e->kind = Expr::Str;
	e->bytes = bytes;
	return e;
}


E make(Expr::Kind kind, const QString &s, std::vector<E> k, int64_t v)
{
	auto e = std::make_shared<Expr>();
	e->kind = kind;
	e->s = s;
	e->k = std::move(k);
	e->v = v;
	return e;
}

E konst(int64_t v) { return make(Expr::Const, QString(), {}, v); }
E var(const QString &name) { return make(Expr::Var, name); }

int prec(const QString &op)
{
	static const QHash<QString, int> table{{"||", 1}, {"&&", 2}, {"|", 3}, {"&", 5}, {"==", 6}, {"!=", 6},
		{"<", 7}, {"<=", 7}, {">", 7}, {">=", 7}, {"in", 7}, {"^", 4}, {"<<", 8}, {">>", 8}, {"+", 9}, {"-", 9},
		{"*", 10}, {"/", 10}, {"%", 10}};
	return table.value(op, 0);
}

QString fmt(const E &e, int outer)
{
	switch (e->kind)
	{
	case Expr::Const:
		return e->v > 99 ? QStringLiteral("0x%1").arg(e->v, 0, 16) : QString::number(e->v);
	case Expr::Var:
		return e->s;
	case Expr::Str:
	{
		QString out = QStringLiteral("\"");
		for (char ch : e->bytes)
		{
			const uint8_t b = uint8_t(ch);
			if (b >= 32 && b < 127 && b != '"' && b != '\\')
				out += QChar(b);
			else
				out += QStringLiteral("\\x%1").arg(b, 2, 16, QLatin1Char('0'));
		}
		return out + QChar('"');
	}
	case Expr::Bin:
	{
		const int p = prec(e->s);
		const QString text = QStringLiteral("%1 %2 %3").arg(fmt(e->k[0], p), e->s, fmt(e->k[1], p + 1));
		return p < outer ? QStringLiteral("(%1)").arg(text) : text;
	}
	case Expr::Un:
		return e->s + fmt(e->k[0], 11);
	case Expr::Deref:
		return QStringLiteral("*") + fmt(e->k[0], 11);
	case Expr::Addr:
		return QStringLiteral("&") + fmt(e->k[0], 11);
	case Expr::Ref:
		return QStringLiteral("&(%1)").arg(fmt(e->k[0]));
	case Expr::Flag:
		return e->w == 1 ? QStringLiteral("flag[%1]").arg(e->v) : QStringLiteral("flag[%1:%2]").arg(e->v).arg(e->w);
	case Expr::Call:
	{
		QStringList args;
		for (const E &a : e->k)
			args << fmt(a);
		return QStringLiteral("%1(%2)").arg(e->s, args.join(QStringLiteral(", ")));
	}
	case Expr::Pid:
		return QStringLiteral("pid");
	case Expr::Cast:
		return QStringLiteral("(%1)%2").arg(e->s, fmt(e->k[0], 11));
	case Expr::Finder:
		return QStringLiteral("finder");
	}
	return QString();
}

// ---- Statements ----

S stmt(Stmt::Kind kind)
{
	auto s = std::make_shared<Stmt>();
	s->kind = kind;
	return s;
}

S plain(const QString &text)
{
	S s = stmt(Stmt::Plain);
	s->text = text;
	return s;
}

void render(const Block &block, int indent, QStringList *out)
{
	const QString p(indent * 4, QChar(' '));
	for (const S &s : block)
	{
		if (s->hidden)
			continue;
		switch (s->kind)
		{
		case Stmt::Assign:
			out->append(QStringLiteral("%1%2 = %3;").arg(p, s->lhs, fmt(s->expr)));
			break;
		case Stmt::Plain:
			out->append(QStringLiteral("%1%2;").arg(p, s->text));
			break;
		case Stmt::Wait:
			out->append(QStringLiteral("%1await %2;").arg(p, fmt(s->expr)));
			break;
		case Stmt::Implies:
			out->append(QStringLiteral("%1implies(%2, %3);").arg(p, fmt(s->cond), fmt(s->expr)));
			break;
		case Stmt::Call:
			out->append(QStringLiteral("%1%2%3;").arg(p, s->var.isEmpty() ? QString() : s->var + QStringLiteral(" = "), fmt(s->expr)));
			break;
		case Stmt::If:
			out->append(QStringLiteral("%1if (%2) {").arg(p, fmt(s->cond)));
			render(s->a, indent + 1, out);
			if (s->hasElse)
			{
				out->append(QStringLiteral("%1} else {").arg(p));
				render(s->b, indent + 1, out);
			}
			out->append(QStringLiteral("%1}").arg(p));
			break;
		case Stmt::While:
			if (s->each)
				out->append(QStringLiteral("%1foreach (%2 in %3) {").arg(p, s->lhs, fmt(s->cond)));
			else if (s->endless && s->pre.empty())
				out->append(QStringLiteral("%1while (true) {").arg(p));
			else if (!s->pre.empty())
			{
				out->append(QStringLiteral("%1while (true) {").arg(p));
				render(s->pre, indent + 1, out);
				out->append(QStringLiteral("%1    if (!%2) break;").arg(p, fmt(s->cond, 11)));
			}
			else if (s->cond->kind == Expr::Call && s->cond->s.startsWith(QStringLiteral("search_")))
				out->append(QStringLiteral("%1foreach (%2 in %3) {").arg(p, varNameOf(s->cond->v), fmt(s->cond)));
			else
				out->append(QStringLiteral("%1while (%2) {").arg(p, fmt(s->cond)));
			render(s->a, indent + 1, out);
			out->append(QStringLiteral("%1}").arg(p));
			break;
		}
	}
}

}
