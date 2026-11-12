#include "data/ucgen.h"

#include <QHash>
#include <QObject>
#include <QVector>

#include <algorithm>

using namespace Ucast;

namespace Ucgen
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

class Gen
{
public:
	QByteArray run(const Block &body, bool hasFrame, int frame)
	{
		if (hasFrame)
		{
			byte(0x5a);
			byte(frame);
		}
		for (size_t k = 0; k < body.size(); k++)
		{
			const S &s = body[k];
			const bool last = k + 1 == body.size();
			if (last && s->kind == Stmt::Plain && s->op == 0x50)
			{
				// The cleanup before the final return: an early return may jump into it part way.
				std::vector<RawOp> clean = s->raw;
				clean.insert(clean.end(), s->mid.begin(), s->mid.end());
				for (size_t c = 0; c <= clean.size(); c++)
				{
					if (lret_.contains(int(c)))
						place(lret_[int(c)]);
					if (c < clean.size())
						rawOps({clean[c]});
				}
				byte(0x50);
				rawOps(s->tail);
			}
			else
				statement(s);
		}
		byte(0x79);
		for (const Fix &f : fixes_)
		{
			if (labels_[f.label] < 0)
				bad(QObject::tr("a label is never placed"));
			const int rel = (labels_[f.label] - f.end) & 0xffff;
			out_[f.pos] = char(rel & 0xff);
			out_[f.pos + 1] = char(rel >> 8);
		}
		return out_;
	}

private:
	struct Fix
	{
		int pos, end, label;
	};
	struct LoopLabels
	{
		int start, back, exit;
	};
	QByteArray out_;
	QVector<Fix> fixes_;
	QVector<int> labels_;
	QVector<LoopLabels> loops_;
	QHash<int, int> lret_;	// label of the return point after k cleanup instructions

	int newLabel()
	{
		labels_.append(-1);
		return labels_.size() - 1;
	}
	void place(int label) { labels_[label] = out_.size(); }
	void byte(int v) { out_.append(char(v & 0xff)); }
	void word(int v)
	{
		byte(v);
		byte(v >> 8);
	}
	void dword(int64_t v)
	{
		word(int(v & 0xffff));
		word(int((v >> 16) & 0xffff));
	}
	void jump(uint8_t op, int label)
	{
		byte(op);
		const int pos = out_.size();
		word(0);
		fixes_.append({pos, out_.size(), label});
	}
	void rawOps(const std::vector<RawOp> &ops)
	{
		for (const RawOp &r : ops)
		{
			byte(r.op);
			for (int64_t a : r.args)
				byte(int(a));
		}
	}

	static int sizeOf(const E &e) { return e->size; }

	void value(const E &e)
	{
		switch (e->kind)
		{
		case Expr::Const:
			byte(e->op);
			if (e->op == 0x0b)
				word(int(e->v));
			else if (e->op == 0x0c)
				dword(e->v);
			else
				byte(int(e->v));
			break;
		case Expr::Var:
			if (e->s == QLatin1String("result") || e->s == QLatin1String("temp") || e->s.startsWith(QLatin1String("tmp")))
			{
				byte(e->op);
				break;
			}
			byte(e->op);
			if (e->op == 0x42 || e->op == 0x45)
			{
				byte(int(e->opArgs[0]));
				byte(int(e->opArgs[1]));
			}
			else
				byte(int(e->v));
			break;
		case Expr::Str:
			byte(0x0d);
			word(e->bytes.size());
			out_.append(e->bytes);
			byte(0);
			break;
		case Expr::Bin:
			if (e->s == QLatin1String("<<") || e->s == QLatin1String(">>"))
			{
				value(e->k[1]);
				value(e->k[0]);
			}
			else
			{
				value(e->k[0]);
				value(e->k[1]);
			}
			byte(e->op);
			for (int64_t a : e->opArgs)
				byte(int(a));
			break;
		case Expr::Un:
		case Expr::Cast:
			value(e->k[0]);
			byte(e->op);
			break;
		case Expr::Deref:
			value(e->k[0]);
			byte(0x4c);
			byte(int(e->opArgs[0]));
			break;
		case Expr::Addr:
			byte(e->op);
			byte(int(e->v));
			break;
		case Expr::Ref:
			byte(0x6f);
			byte(int(e->opArgs[0]));
			break;
		case Expr::Flag:
			byte(0x4e);
			word(int(e->v));
			byte(e->w);
			break;
		case Expr::Pid:
			byte(0x59);
			break;
		case Expr::Call:
			call(e);
			if (e->tempOp)
				byte(e->tempOp);
			break;
		case Expr::Finder:
			bad(QObject::tr("a finder is not a value"));
		}
	}

	void call(const E &e)
	{
		if (e->s == QLatin1String("list"))
		{
			for (size_t k = e->k.size(); k-- > 0;)
				value(e->k[k]);
			byte(0x0e);
			byte(int(e->opArgs[0]));
			byte(int(e->opArgs[1]));
			return;
		}
		if (e->s == QLatin1String("element"))
		{
			value(e->k[0]);
			value(e->k[1]);
			byte(0x44);
			byte(int(e->opArgs[0]));
			byte(int(e->opArgs[1]));
			return;
		}
		if (e->s == QLatin1String("search_next"))
		{
			byte(0x73);
			return;
		}
		for (const E &h : e->hoist)
			value(h);
		int total = 0;
		for (const E &h : e->hoist)
			total += sizeOf(h);
		for (size_t k = e->k.size(); k-- > 0;)
		{
			const E &a = e->k[k];
			// A reference to a temporary pushed before the arguments is only the pointer.
			value(a);
			if (!(e->op == 0x0f || e->op == 0x57) || k > 0 || e->op == 0x0f)
				total += sizeOf(a);
			else
			{
				// spawn's first operand is the pointer the instruction itself pops
			}
		}
		switch (e->op)
		{
		case 0x0f:
			byte(0x0f);
			byte(int(e->opArgs[0]));
			word(int(e->opArgs[1]));
			break;
		case 0x10:
			byte(0x10);
			word(int(e->opArgs[0]));
			word(int(e->opArgs[1]));
			break;
		case 0x11:
			byte(0x11);
			word(int(e->opArgs[0]));
			word(int(e->opArgs[1]));
			break;
		case 0x57:
			byte(0x57);
			byte(int(e->opArgs[0]));
			byte(int(e->opArgs[1]));
			word(int(e->opArgs[2]));
			word(int(e->opArgs[3]));
			break;
		default:
			bad(QObject::tr("cannot make a call of opcode %1").arg(e->op, 2, 16));
		}
		rawOps(e->post);
		if (total > 0)
		{
			byte(0x6e);
			byte(-total);
		}
	}

	void block(const Block &b)
	{
		for (const S &s : b)
			statement(s);
	}

	void statement(const S &s)
	{
		statementCode(s);
		rawOps(s->tail);
	}

	void statementCode(const S &s)
	{
		rawOps(s->raw);
		if (s->kind != Stmt::Assign)
			rawOps(s->mid);
		switch (s->kind)
		{
		case Stmt::Assign:
			if (s->lhs == QLatin1String("result"))
			{
				value(s->expr);
				rawOps(s->mid);
				byte(0x08);
			}
			else if (s->lhs == QLatin1String("temp"))
			{
				value(s->expr);
				rawOps(s->mid);
				byte(s->op);
			}
			else if (s->op == 0x4f)
			{
				value(s->expr);
				rawOps(s->mid);
				byte(0x4f);
				word(int(s->opArgs[0]));
				byte(int(s->opArgs[1]));
			}
			else if (s->op == 0x4d)
			{
				value(s->expr);
				value(s->lhsE);
				rawOps(s->mid);
				byte(0x4d);
				byte(int(s->opArgs[0]));
			}
			else if (s->op == 0x03)
			{
				value(s->expr);
				rawOps(s->mid);
				byte(0x03);
				byte(int(s->opArgs[0]));
				byte(int(s->opArgs[1]));
			}
			else
			{
				value(s->expr);
				rawOps(s->mid);
				byte(s->op);
				byte(int(s->opArgs[0]));
			}
			break;
		case Stmt::Plain:
			if (s->hidden)
			{
				if (s->op == 0x52)
				{
					byte(0x52);
					word(0);
				}
				else if (s->op == 0x59)
					byte(0x59);
				else if (s->op == 0xff)
					value(s->expr);
			}
			else if (s->op == 0x77)
			{
				value(s->cond);
				value(s->expr);
				byte(0x77);
			}
			else if (s->op == 0x78 || s->op == 0x53 || s->op == 0x50)
				byte(s->op);
			else if (s->text == QLatin1String("return"))
			{
				const int skip = s->opArgs.empty() ? 0 : int(s->opArgs[0]);
				if (!lret_.contains(skip))
					lret_.insert(skip, newLabel());
				jump(0x52, lret_[skip]);
			}
			else if (s->text == QLatin1String("break"))
			{
				if (loops_.isEmpty())
					bad(QObject::tr("break outside a loop"));
				jump(0x52, loops_.last().exit);
			}
			else if (s->text == QLatin1String("continue"))
			{
				if (loops_.isEmpty())
					bad(QObject::tr("continue outside a loop"));
				jump(0x52, !s->opArgs.empty() ? loops_.last().back : loops_.last().start);
			}
			else
				bad(QObject::tr("cannot make '%1'").arg(s->text));
			break;
		case Stmt::Wait:
			byte(0x59);
			value(s->expr);
			byte(0x54);
			byte(1);
			byte(1);
			byte(0x12);
			byte(0x53);
			break;
		case Stmt::Implies:
			value(s->cond);
			value(s->expr);
			byte(0x54);
			byte(1);
			byte(1);
			byte(0x12);
			break;
		case Stmt::Call:
			call(s->expr);
			break;
		case Stmt::If:
		{
			value(s->cond);
			const int lelse = newLabel(), lend = newLabel();
			jump(0x51, lelse);
			block(s->a);
			if (s->hasElse)
			{
				jump(0x52, lend);
				place(lelse);
				block(s->b);
				place(lend);
			}
			else
				place(lelse);
			break;
		}
		case Stmt::While:
			loop(s);
			break;
		}
	}

	void loop(const S &s)
	{
		LoopLabels l{newLabel(), newLabel(), newLabel()};
		const bool search = !s->each && s->cond && s->cond->kind == Expr::Call && s->cond->s.startsWith(QLatin1String("search_"));
		if (s->each)
		{
			value(s->cond);
			value(s->expr);
			place(l.start);
			byte(s->op);
			byte(int(s->opArgs[0]));
			byte(int(s->opArgs[1]));
			const int pos = out_.size();
			word(0);
			fixes_.append({pos, out_.size(), l.exit});
			loops_.append(l);
			block(s->a);
			loops_.removeLast();
			place(l.back);
			jump(0x52, l.start);
			place(l.exit);
			return;
		}
		if (search)
		{
			const E &c = s->cond;
			for (size_t k = 2; k < c->k.size(); k++)
				value(c->k[k]);
			value(c->k[1]);
			value(c->k[0]);
			byte(0x70);
			byte(int(c->opArgs[0]));
			byte(int(c->opArgs[1]));
			byte(int(c->opArgs[2]));
			place(l.start);
			jump(0x51, l.exit);
			loops_.append(l);
			block(s->a);
			loops_.removeLast();
			byte(0x73);
			place(l.back);
			jump(0x52, l.start);
			place(l.exit);
			return;
		}
		place(l.start);
		if (!s->endless)
		{
			loops_.append(l);
			block(s->pre);
			loops_.removeLast();
			value(s->cond);
			jump(0x51, l.exit);
		}
		loops_.append(l);
		block(s->a);
		loops_.removeLast();
		place(l.back);
		jump(0x52, l.start);
		place(l.exit);
	}
};

}

QByteArray generate(const Block &body, bool hasFrame, int frame, QString *error)
{
	try
	{
		Gen g;
		return g.run(body, hasFrame, frame);
	}
	catch (const Bad &b)
	{
		if (error)
			*error = b.why;
		return QByteArray();
	}
}

}
