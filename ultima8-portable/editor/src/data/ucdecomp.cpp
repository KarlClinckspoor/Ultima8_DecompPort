#include "data/ucdecomp.h"

#include <QHash>
#include <QObject>

#include <algorithm>
#include <memory>
#include <vector>

#include "data/ucasm.h"
#include "data/ucast.h"
#include "data/ucode.h"
#include "data/usecode.h"

using namespace Ucast;

namespace Ucdecomp
{

namespace
{

struct Fail
{
	QString why;
};

[[noreturn]] void fail(const QString &why)
{
	throw Fail{why};
}

// ---- The decompiler, one routine ----

struct Ent
{
	E e;
	size_t pos = 0;		// how many statements there were when it was pushed
	bool implied = false;	// the process id an implies left on the stack
	int size() const { return e->size; }
};

struct Loop
{
	bool in = false;
	int start = -1;
	int exit = -1;
};

struct Temp
{
	enum Kind { None, CallT, Val } kind = None;
	S call;
	E val;
	bool adjusted = false;	// the call's stack adjustment has run
};

using MutE = std::shared_ptr<Expr>;

MutE mk(Expr::Kind kind, uint8_t op, int size, char type)
{
	MutE e = newExpr(kind);
	e->op = op;
	e->size = size;
	e->type = type;
	return e;
}

E mkConst(int64_t v, uint8_t op, int size)
{
	MutE e = mk(Expr::Const, op, size, size == 4 ? 'l' : 'i');
	e->v = v;
	return e;
}

E mkVar(int64_t off, uint8_t op, int size, char type, std::vector<int64_t> args = {})
{
	MutE e = mk(Expr::Var, op, size, type);
	e->s = varNameOf(off);
	e->v = off;
	e->opArgs = std::move(args);
	return e;
}

E mkNamedVar(const QString &name, uint8_t op, int size)
{
	MutE e = mk(Expr::Var, op, size, size == 4 ? 'l' : 'i');
	e->s = name;
	return e;
}

E mkBin(const QString &op, uint8_t opcode, int size, char type, const E &a, const E &b)
{
	MutE e = mk(Expr::Bin, opcode, size, type);
	e->s = op;
	e->k = {a, b};
	return e;
}

E mkUn(const QString &op, uint8_t opcode, const E &a)
{
	MutE e = mk(Expr::Un, opcode, 2, 'i');
	e->s = op;
	e->k = {a};
	return e;
}

E mkCast(const QString &type, uint8_t opcode, int size, char t, const E &a)
{
	MutE e = mk(Expr::Cast, opcode, size, t);
	e->s = type;
	e->k = {a};
	return e;
}

MutE mkCall(const QString &name, uint8_t opcode, std::vector<int64_t> args, std::vector<E> k, char type = 'i')
{
	MutE e = mk(Expr::Call, opcode, 2, type);
	e->s = name;
	e->opArgs = std::move(args);
	e->k = std::move(k);
	return e;
}

class Dec
{
public:
	Dec(const QVector<Ucasm::Insn> &ins) : ins_(ins)
	{
		for (int k = 0; k < ins.size(); k++)
			at_.insert(ins[k].at, k);
		int ri = ins.size() - 1;
		while (ri > 0 && ins[ri].name != QStringLiteral("return"))
			ri--;
		while (ri > 0 && (ins[ri - 1].name.startsWith(QStringLiteral("free_")) || ins[ri - 1].name == QStringLiteral("give_heap")))
			ri--;
		retStart_ = ri;
		retEnd_ = ins.size() - 2;
		for (int k = 0; k < ins.size(); k++)
			if (ins[k].name == QStringLiteral("branch"))
			{
				const int t = (ins[k].at + ins[k].size + int(ins[k].args.last())) & 0xffff;
				auto it = at_.constFind(t);
				if (it != at_.constEnd() && *it <= k)
					backward_[*it].append(k);
			}
	}

	Block run(int lo, int hi)
	{
		Block b = parse(lo, hi, Loop());
		if (!stack_.empty())
			fail(QStringLiteral("leftover stack"));
		return b;
	}

private:
	const QVector<Ucasm::Insn> &ins_;
	QHash<int, int> at_;
	QHash<int, QVector<int>> backward_;
	int retStart_ = 0, retEnd_ = 0;
	int cur_ = 0;
	std::vector<Ent> stack_;
	Block stmts_;
	Temp temp_;
	int tmpn_ = 0;
	std::vector<RawOp> pending_;	// instructions the text does not show, for the next statement
	size_t mark_ = 0;		// how many of them came before the current statement's first push

	void push(const E &e, bool implied = false)
	{
		if (onlyFinders())
			mark_ = pending_.size();
		Ent x;
		x.e = e;
		x.pos = stmts_.size();
		x.implied = implied;
		stack_.push_back(x);
	}
	Ent pop(int size = 0)
	{
		if (stack_.empty())
			fail(QStringLiteral("stack empty"));
		Ent x = stack_.back();
		stack_.pop_back();
		if (size && x.size() != size)
			fail(QStringLiteral("size %1 != %2").arg(x.size()).arg(size));
		return x;
	}
	std::vector<E> topArgs(int nbytes)
	{
		std::vector<E> out;
		if (nbytes == 0)
			return out;
		int got = 0;
		int i = int(stack_.size());
		while (got < nbytes)
		{
			if (--i < 0)
				fail(QStringLiteral("args underflow"));
			out.push_back(stack_[i].e);
			got += stack_[i].size();
		}
		if (got != nbytes)
			fail(QStringLiteral("args misaligned %1/%2").arg(got).arg(nbytes));
		return out;
	}
	// The entries under the top `skip` bytes that total `bytes`, deepest first.
	std::vector<E> beneath(int skip, int bytes)
	{
		std::vector<E> out;
		if (bytes == 0)
			return out;
		int i = int(stack_.size());
		int got = 0;
		while (got < skip)
		{
			if (--i < 0)
				fail(QStringLiteral("hoist underflow"));
			got += stack_[i].size();
		}
		int taken = 0;
		while (taken < bytes)
		{
			if (--i < 0)
				fail(QStringLiteral("hoist underflow"));
			out.insert(out.begin(), stack_[i].e);
			taken += stack_[i].size();
		}
		if (taken != bytes)
			fail(QStringLiteral("hoist misaligned"));
		return out;
	}
	void addStmt(const S &s)
	{
		// What came before the statement's first push goes before it; what came after (a free of the
		// old value, between a value and its store) is its middle.
		const size_t m = std::min(mark_, pending_.size());
		s->raw.assign(pending_.begin(), pending_.begin() + m);
		s->mid.assign(pending_.begin() + m, pending_.end());
		pending_.clear();
		mark_ = 0;
		stmts_.push_back(s);
	}
	bool onlyFinders() const
	{
		for (const Ent &x : stack_)
			if (x.size() != -1)
				return false;
		return true;
	}
	void rawOp(const Ucasm::Insn &i)
	{
		RawOp r;
		r.op = i.op;
		r.args.assign(i.args.begin(), i.args.end());
		// A call's frees come before its stack adjustment; anything else goes with the next statement.
		if (temp_.kind == Temp::CallT && !temp_.adjusted && !stmts_.empty() && stmts_.back() == temp_.call
			&& !temp_.call->consumed && i.name.endsWith(QLatin1String("_sp")))
			temp_.call->expr->post.push_back(r);
		else
			pending_.push_back(r);
	}

	int target(const Ucasm::Insn &i) const
	{
		const int t = (i.at + i.size + int(i.args.last())) & 0xffff;
		auto it = at_.constFind(t);
		if (it != at_.constEnd())
			return *it;
		if (t == ins_.last().at + ins_.last().size)
			return ins_.size();
		fail(QStringLiteral("bad target"));
	}

	static QString varName(int64_t off) { return Ucast::varNameOf(off); }

	E tempValue(uint8_t op)
	{
		if (temp_.kind == Temp::None)
			fail(QStringLiteral("no temp"));
		const int size = op == 0x5f ? 4 : 2;
		if (temp_.kind == Temp::CallT)
		{
			S st = temp_.call;
			E e;
			if (!st->consumed && !stmts_.empty() && stmts_.back() == st)
			{
				stmts_.pop_back();
				pending_.insert(pending_.begin(), st->raw.begin(), st->raw.end());
				st->raw.clear();
				st->consumed = true;
				auto c = std::make_shared<Expr>(*st->expr);
				c->tempOp = op;
				c->size = size;
				c->type = size == 4 ? 'l' : 'i';
				st->expr = c;
				e = c;
			}
			else
			{
				if (st->var.isEmpty())
					st->var = QStringLiteral("tmp%1").arg(tmpn_++);
				e = mkNamedVar(st->var, op, size);
			}
			temp_.kind = Temp::Val;
			temp_.val = e;
			return e;
		}
		if (temp_.val->kind == Expr::Var && (temp_.val->s == QLatin1String("temp") || temp_.val->s.startsWith(QLatin1String("tmp"))))
			return mkNamedVar(temp_.val->s, op, size);
		return temp_.val;
	}

	void callStmt(const E &call)
	{
		S st = stmt(Stmt::Call);
		st->expr = call;
		addStmt(st);
		temp_.kind = Temp::CallT;
		temp_.call = st;
		temp_.adjusted = false;
	}

	S simpleStmt(const QString &text, uint8_t op)
	{
		S s = plain(text);
		s->op = op;
		return s;
	}

	// The stack adjustment that follows the call at cur_ (past the frees of its arguments).
	bool nextAdjusts(int *bytes) const
	{
		int j = cur_ + 1;
		while (j < ins_.size() && ins_[j].name.startsWith(QLatin1String("free_")))
			j++;
		if (j < ins_.size() && ins_[j].name == QLatin1String("add_sp") && ins_[j].args[0] < 0)
		{
			*bytes = int(-ins_[j].args[0]);
			return true;
		}
		return false;
	}

	void step(const Ucasm::Insn &i)
	{
		static const QHash<QString, QString> bins{{"add", "+"}, {"sub", "-"}, {"mul", "*"}, {"div", "/"}, {"mod", "%"},
			{"eq", "=="}, {"ne", "!="}, {"lt", "<"}, {"le", "<="}, {"gt", ">"}, {"ge", ">="}, {"and", "&&"},
			{"or", "||"}, {"bit_and", "&"}, {"bit_or", "|"}, {"concat_strings", "+"}, {"concat_lists", "+"}};
		const QString &n = i.name;
		const auto &a = i.args;
		auto is = [&](const char *name) { return n == QLatin1String(name); };
		if (is("push_imm") || is("push_byte_imm"))
			push(mkConst(a[0], i.op, 2));
		else if (is("push_long_imm"))
			push(mkConst(a[0], i.op, 4));
		else if (is("push_token"))
			push(mkConst(a[0], i.op, 1));
		else if (is("push_local") || is("push_byte_local"))
			push(mkVar(a[0], i.op, 2, 'i'));
		else if (is("push_long_local"))
			push(mkVar(a[0], i.op, 4, 'l'));
		else if (is("push_string_local"))
			push(mkVar(a[0], i.op, 2, 's'));
		else if (is("push_list_local"))
			push(mkVar(a[0], i.op, 2, 'L', {a[0], a[1]}));
		else if (is("push_string_list_local"))
			push(mkVar(a[0], i.op, 2, 'S'));
		else if (is("push_bytes_local"))
			push(mkVar(a[0], i.op, int(a[1]), 'b', {a[0], a[1]}));
		else if (is("push_address") || is("push_string_address"))
		{
			MutE e = mk(Expr::Addr, i.op, 4, 'p');
			e->v = a[0];
			e->k = {mkVar(a[0], 0, 2, 'i')};
			push(e);
		}
		else if (is("push_string"))
		{
			MutE e = mk(Expr::Str, i.op, 2, 's');
			e->bytes = i.text;
			push(e);
		}
		else if (is("string_to_ptr"))
			push(mkCast(QStringLiteral("ptr"), i.op, 4, 'p', pop(2).e));
		else if (is("load_ptr"))
		{
			Ent p = pop(4);
			MutE e = mk(Expr::Deref, i.op, int(a[0]), 'b');
			e->k = {p.e};
			e->opArgs = {a[0]};
			push(e);
		}
		else if (is("store") || is("store_byte") || is("store_long"))
		{
			Ent x = pop(is("store_long") ? 4 : 2);
			S s = stmt(Stmt::Assign);
			s->lhs = varName(a[0]);
			s->expr = x.e;
			s->op = i.op;
			s->opArgs = {a[0]};
			addStmt(s);
		}
		else if (is("store_bytes"))
		{
			const std::vector<E> v = topArgs(int(a[1]));
			if (v.size() != 1)
				fail(QStringLiteral("store_bytes of several values"));
			S s = stmt(Stmt::Assign);
			s->lhs = varName(a[0]);
			s->expr = v[0];
			s->op = i.op;
			s->opArgs = {a[0], a[1]};
			addStmt(s);
		}
		else if (is("store_ptr"))
		{
			Ent p = pop(4);
			Ent x = pop(int(a[0]));
			MutE d = mk(Expr::Deref, 0, int(a[0]), 'b');
			d->k = {p.e};
			S s = stmt(Stmt::Assign);
			s->lhs = fmt(d);
			s->lhsE = p.e;
			s->expr = x.e;
			s->op = i.op;
			s->opArgs = {a[0]};
			addStmt(s);
		}
		else if (is("push_flag"))
		{
			MutE e = mk(Expr::Flag, i.op, 2, 'i');
			e->v = a[0];
			e->w = int(a[1]);
			e->opArgs = {a[0], a[1]};
			push(e);
		}
		else if (is("store_flag"))
		{
			Ent x = pop(2);
			E f = Ucast::flag(a[0], int(a[1]));
			S s = stmt(Stmt::Assign);
			s->lhs = fmt(f);
			s->expr = x.e;
			s->op = i.op;
			s->opArgs = {a[0], a[1]};
			addStmt(s);
		}
		else if (bins.contains(n))
		{
			Ent b = pop();
			Ent aa = pop();
			const bool strings = n == QLatin1String("concat_strings");
			const bool lists = n == QLatin1String("concat_lists");
			push(mkBin(bins[n], i.op, 2, strings ? 's' : lists ? 'L' : 'i', aa.e, b.e));
		}
		else if (n.endsWith(QLatin1String("_long")) && bins.contains(n.left(n.size() - 5)))
		{
			const QString base = n.left(n.size() - 5);
			Ent b = pop(4);
			Ent aa = pop(4);
			const bool test = base == QLatin1String("eq") || base == QLatin1String("ne") || base == QLatin1String("lt")
				|| base == QLatin1String("le") || base == QLatin1String("gt") || base == QLatin1String("ge")
				|| base == QLatin1String("and") || base == QLatin1String("or");
			push(mkBin(bins[base], i.op, test ? 2 : 4, test ? 'i' : 'l', aa.e, b.e));
		}
		else if (is("eq_strings"))
		{
			Ent b = pop(2);
			Ent aa = pop(2);
			push(mkBin(QStringLiteral("=="), i.op, 2, 'i', aa.e, b.e));
		}
		else if (is("not"))
			push(mkUn(QStringLiteral("!"), i.op, pop(2).e));
		else if (is("not_long"))
			push(mkUn(QStringLiteral("!"), i.op, pop(4).e));
		else if (is("bit_not"))
			push(mkUn(QStringLiteral("~"), i.op, pop(2).e));
		else if (is("shift_left") || is("shift_right"))
		{
			Ent top = pop(2);
			Ent under = pop(2);
			push(mkBin(is("shift_left") ? QStringLiteral("<<") : QStringLiteral(">>"), i.op, 2, 'i', top.e, under.e));
		}
		else if (is("word_to_long"))
			push(mkCast(QStringLiteral("long"), i.op, 4, 'l', pop(2).e));
		else if (is("long_to_word"))
			push(mkCast(QStringLiteral("int"), i.op, 2, 'i', pop(4).e));
		else if (is("push_pid"))
			push(mk(Expr::Pid, i.op, 2, 'i'));
		else if (is("give_heap"))
			rawOp(i);
		else if (is("push_result"))
			push(mkNamedVar(QStringLiteral("result"), i.op, 4));
		else if (is("pop_result"))
		{
			Ent x = pop(4);
			S s = stmt(Stmt::Assign);
			s->lhs = QStringLiteral("result");
			s->expr = x.e;
			s->op = i.op;
			addStmt(s);
		}
		else if (is("make_list"))
		{
			const int size = int(a[0]), count = int(a[1]);
			std::vector<E> els;
			int got = 0;
			while (got < size * count)
			{
				Ent x = pop();
				got += x.size();
				els.push_back(x.e);
			}
			if (got != size * count)
				fail(QStringLiteral("make_list misaligned"));
			push(mkCall(QStringLiteral("list"), i.op, {a[0], a[1]}, els, 'L'));
		}
		else if (is("in_list"))
		{
			Ent lst = pop(2);
			Ent el = pop(int(a[0]));
			MutE e = mk(Expr::Bin, i.op, 2, 'i');
			e->s = QStringLiteral("in");
			e->k = {el.e, lst.e};
			e->opArgs = {a[0], a[1]};
			push(e);
		}
		else if (is("list_xor") || is("string_list_xor") || is("list_subtract") || is("string_list_subtract"))
		{
			Ent right = pop(2);
			Ent left = pop(2);
			MutE e = mk(Expr::Bin, i.op, 2, 'L');
			e->s = n.endsWith(QLatin1String("xor")) ? QStringLiteral("^") : QStringLiteral("-");
			e->k = {left.e, right.e};
			e->opArgs = {a[0]};
			push(e);
		}
		else if (is("list_element"))
		{
			Ent index = pop(2);
			Ent list = pop(2);
			MutE e = mk(Expr::Call, i.op, a[0] == 1 ? 2 : int(a[0]), 'i');
			e->s = QStringLiteral("element");
			e->k = {list.e, index.e};
			e->opArgs = {a[0], a[1]};
			push(e);
		}
		else if (is("set_process_info"))
		{
			Ent r = pop(2);
			Ent t = pop(2);
			S s = plain(QStringLiteral("set_process_info(%1, %2)").arg(fmt(t.e), fmt(r.e)));
			s->op = i.op;
			s->cond = t.e;
			s->expr = r.e;
			addStmt(s);
		}
		else if (is("exclusive"))
			addStmt(simpleStmt(QStringLiteral("exclusive()"), i.op));
		else if (is("push_sp_address"))
		{
			const int off = int(-a[0]);
			int got = 0;
			E tgt;
			for (auto it = stack_.rbegin(); it != stack_.rend(); ++it)
			{
				if (got == off)
				{
					tgt = it->e;
					break;
				}
				got += it->size();
				if (got > off)
					break;
			}
			if (!tgt)
				fail(QStringLiteral("sp address misaligned"));
			MutE e = mk(Expr::Ref, i.op, 4, 'p');
			e->k = {tgt};
			e->opArgs = {a[0]};
			push(e);
		}
		else if (is("search"))
		{
			const int local = int(a[0]), nbytes = int(a[1]), kind = int(a[2]);
			Ent r = pop(2);
			Ent q = pop(2);
			std::vector<E> args{r.e, q.e};
			std::vector<E> toks;
			int got = 0;
			while (got < nbytes)
			{
				Ent x = pop();
				got += x.size();
				toks.push_back(x.e);
			}
			if (got != nbytes)
				fail(QStringLiteral("search tokens misaligned"));
			std::reverse(toks.begin(), toks.end());
			args.insert(args.end(), toks.begin(), toks.end());
			static const char *const kinds[] = {"", "", "area", "area_deep", "container", "container_deep", "on_top_of"};
			push(mk(Expr::Finder, 0, -1, 'b'));
			MutE c = mk(Expr::Call, i.op, 2, 'i');
			c->s = QStringLiteral("search_") + (kind >= 2 && kind <= 6 ? QString::fromLatin1(kinds[kind]) : QString::number(kind));
			c->k = args;
			c->v = local;
			c->opArgs = {a[0], a[1], a[2]};
			push(c);
		}
		else if (is("search_next"))
		{
			MutE c = mk(Expr::Call, i.op, 2, 'i');
			c->s = QStringLiteral("search_next");
			push(c);
		}
		else if (is("call_intrinsic"))
		{
			const int nbytes = int(a[0]);
			const Ucode::Intrinsic *in = Ucode::intrinsic(int(a[1]));
			QString name = in ? QString::fromLatin1(in->name) : QStringLiteral("intrinsic_%1").arg(a[1]);
			name = name.section(QChar('('), 0, 0);
			std::vector<E> args = topArgs(nbytes);
			int total = nbytes;
			nextAdjusts(&total);
			MutE c = mkCall(name, i.op, {a[0], a[1]}, args);
			if (total > nbytes)
				c->hoist = beneath(nbytes, total - nbytes);
			callStmt(c);
		}
		else if (is("call") || is("call_near"))
		{
			int nb = 0;
			nextAdjusts(&nb);
			const QString name = is("call") ? QStringLiteral("%1::%2").arg(a[0]).arg(a[1], 4, 16, QLatin1Char('0'))
				: QStringLiteral("near_%1").arg((i.at + i.size + a[0]) & 0xffff, 4, 16, QLatin1Char('0'));
			callStmt(mkCall(name, i.op, std::vector<int64_t>(a.begin(), a.end()), topArgs(nb)));
		}
		else if (is("spawn"))
		{
			Ent ptr = pop(4);
			std::vector<E> args{ptr.e};
			for (const E &x : topArgs(int(a[0])))
				args.push_back(x);
			int total = int(a[0]);
			nextAdjusts(&total);
			MutE c = mkCall(QStringLiteral("spawn %1::%2").arg(a[2]).arg(a[3], 4, 16, QLatin1Char('0')), i.op,
				std::vector<int64_t>(a.begin(), a.end()), args);
			if (total > int(a[0]))
				c->hoist = beneath(int(a[0]), total - int(a[0]));
			callStmt(c);
		}
		else if (is("add_sp"))
		{
			const int k = int(a[0]);
			temp_.adjusted = true;
			if (k < 0)
			{
				int got = 0;
				bool finder = false;
				while (got < -k)
				{
					Ent x = pop();
					if (x.size() == -1)
					{
						got = -k;
						finder = true;
						break;
					}
					got += x.size();
				}
				if (got != -k)
					fail(QStringLiteral("add_sp misaligned"));
				if (finder)
				{
					// The search loop's cleanup belongs to the loop, not to what follows it.
					if (!stmts_.empty() && stmts_.back()->kind == Stmt::While)
					{
						RawOp r;
						r.op = i.op;
						r.args.assign(i.args.begin(), i.args.end());
						stmts_.back()->tail.push_back(r);
					}
					else
						rawOp(i);
				}
			}
			else if (k > 0)
				fail(QStringLiteral("add_sp +"));
		}
		else if (is("push_temp") || is("push_temp_byte") || is("push_temp_long"))
			push(tempValue(i.op));
		else if (is("pop_temp") || is("pop_temp_long"))
		{
			Ent x = pop(is("pop_temp") ? 2 : 4);
			if (x.implied)
			{
				temp_.kind = Temp::Val;
				temp_.val = x.e;
			}
			else
			{
				S s = stmt(Stmt::Assign);
				s->lhs = QStringLiteral("temp");
				s->expr = x.e;
				s->op = i.op;
				addStmt(s);
				temp_.kind = Temp::Val;
				temp_.val = mkNamedVar(QStringLiteral("temp"), 0, 2);
			}
		}
		else if (is("implies"))
		{
			if (a[0] != 1 || a[1] != 1)
				fail(QStringLiteral("implies %1,%2").arg(a[0]).arg(a[1]));
			Ent x = pop(2);
			Ent own = pop(2);
			if (own.e->kind != Expr::Pid)
			{
				// A pid pushed just under a temporary stays on the stack: keep its push.
				if (!stack_.empty() && stack_.back().e->kind == Expr::Pid)
				{
					stack_.pop_back();
					S lone = plain(QString());
					lone->hidden = true;
					lone->op = 0x59;
					addStmt(lone);
				}
				// A started earlier waits for B: a process chain, not this process waiting.
				S w = stmt(Stmt::Implies);
				w->cond = own.e;
				w->expr = x.e;
				addStmt(w);
			}
			else
			{
				S w = stmt(Stmt::Wait);
				w->expr = x.e;
				addStmt(w);
			}
			push(x.e, true);
		}
		else if (is("suspend"))
		{
			if (!(!stmts_.empty() && stmts_.back()->kind == Stmt::Wait && temp_.kind == Temp::Val))
				addStmt(simpleStmt(QStringLiteral("suspend()"), i.op));
		}
		else if (n.startsWith(QLatin1String("free_")))
			rawOp(i);
		else if (is("return") || is("locals") || is("end"))
		{
		}
		else
			fail(QStringLiteral("op %1").arg(n));
	}

	void jumpStmt(const QString &text, bool toBack)
	{
		S s = plain(text);
		s->op = 0x52;
		if (toBack)
			s->opArgs = {1};
		addStmt(s);
	}

	Block parse(int lo, int hi, const Loop &loop)
	{
		Block saved = std::move(stmts_);
		stmts_.clear();
		int i = lo;
		while (i < hi)
		{
			const Ucasm::Insn &ins = ins_[i];
			cur_ = i;
			if (backward_.contains(i) && i != (loop.in ? loop.start : -1))
			{
				int b = -1;
				for (int x : backward_[i])
					if (x < hi)
						b = std::max(b, x);
				if (b >= 0)
				{
					i = loopAt(i, b);
					continue;
				}
			}
			if (ins.name == QLatin1String("branch_false"))
			{
				E cond = pop(2).e;
				if (!onlyFinders())
					fail(QStringLiteral("stack not empty at branch"));
				const int t = target(ins);
				const Ucasm::Insn *before = t - 1 > i ? &ins_[t - 1] : nullptr;
				// A branch just before the end may belong to a nested if that ends at the same place.
				bool nested = false;
				for (int q = i + 1; q < t - 1 && !nested; q++)
					if (ins_[q].name == QLatin1String("branch_false") && target(ins_[q]) == t)
						nested = true;
				if (before && before->name == QLatin1String("branch") && !nested)
				{
					const int x = target(*before);
					if (x > t && retStart_ <= x && x <= retEnd_)
					{
						S s = stmt(Stmt::If);
						s->cond = cond;
						addStmt(s);
						s->a = sub(i + 1, t - 1, loop);
						S r = plain(QStringLiteral("return"));
						r->op = 0x52;
						r->opArgs = {x - retStart_};
						s->a.push_back(r);
						i = t;
						continue;
					}
					if (loop.in && (x == loop.exit || x == loop.start))
					{
					}
					else if (x > t)
					{
						S s = stmt(Stmt::If);
						s->cond = cond;
						addStmt(s);
						s->a = sub(i + 1, t - 1, loop);
						s->b = sub(t, x, loop);
						s->hasElse = true;
						i = x;
						continue;
					}
				}
				S s = stmt(Stmt::If);
				s->cond = cond;
				addStmt(s);
				s->a = sub(i + 1, t, loop);
				i = t;
				continue;
			}
			if (ins.name == QLatin1String("branch"))
			{
				const int t = target(ins);
				if (t == i + 1)
				{
					S s = plain(QString());
					s->hidden = true;
					s->op = 0x52;
					addStmt(s);
					i++;
					continue;
				}
				if (loop.in && t == loop.exit - 1)
					jumpStmt(QStringLiteral("continue"), true);
				else if (loop.in && t == loop.exit)
					jumpStmt(QStringLiteral("break"), false);
				else if (loop.in && t == loop.start)
					jumpStmt(QStringLiteral("continue"), false);
				else if (retStart_ <= t && t <= retEnd_)
				{
					jumpStmt(QStringLiteral("return"), false);
					stmts_.back()->opArgs = {t - retStart_};	// how many cleanup instructions it skips
				}
				else
					fail(QStringLiteral("goto"));
				i++;
				continue;
			}
			if (ins.name == QLatin1String("return"))
			{
				S r = plain(QStringLiteral("return"));
				r->op = 0x50;
				addStmt(r);
				i++;
				continue;
			}
			if (ins.name == QLatin1String("for_each") || ins.name == QLatin1String("for_each_string"))
				fail(QStringLiteral("for_each"));
			step(ins);
			i++;
		}
		// What was pushed and never used stays pushed: keep each push where it was made.
		{
			size_t shift = 0;
			for (const Ent &x : stack_)
			{
				if (x.size() == -1 || (x.e->kind == Expr::Call && x.e->s == QLatin1String("search_next")))
					continue;
				S p = plain(QString());
				p->hidden = true;
				p->op = 0xff;
				p->expr = x.e;
				const size_t at = std::min(x.pos + shift, stmts_.size());
				stmts_.insert(stmts_.begin() + at, p);
				shift++;
			}
			stack_.clear();
		}
		// Cleanup left at the end of the range ran inside it: it goes after the last statement.
		if (!pending_.empty() && (lo < hi))
		{
			if (stmts_.empty())
			{
				S nothing = plain(QString());
				nothing->hidden = true;
				nothing->op = 0;
				stmts_.push_back(nothing);
			}
			stmts_.back()->tail.insert(stmts_.back()->tail.end(), pending_.begin(), pending_.end());
			pending_.clear();
		}
		Block r = std::move(stmts_);
		stmts_ = std::move(saved);
		return r;
	}

	// for_each: [list, cursor] on the stack, the loop's top, a branch back to it.
	int forEachAt(int start, int b)
	{
		const Ucasm::Insn &fe = ins_[start];
		const int T = b + 1;
		if (target(fe) != T)
			fail(QStringLiteral("for_each exit"));
		cur_ = start;
		Ent cursor = pop(2);
		E list = pop(2).e;
		Loop inner;
		inner.in = true;
		inner.start = start;
		inner.exit = T;
		S w = stmt(Stmt::While);
		w->each = true;
		w->lhs = varName(fe.args[0]);
		w->cond = list;
		w->expr = cursor.e;
		w->op = fe.op;
		w->opArgs = {fe.args[0], fe.args[1]};
		addStmt(w);
		w->a = sub(start + 1, b, inner);
		return T;
	}

	int loopAt(int start, int b)
	{
		if (ins_[start].name == QLatin1String("for_each") || ins_[start].name == QLatin1String("for_each_string"))
			return forEachAt(start, b);
		const int T = b + 1;
		Loop inner;
		inner.in = true;
		inner.start = start;
		inner.exit = T;
		Block saved = std::move(stmts_);
		stmts_.clear();
		const std::vector<Ent> stack0 = stack_;
		const Temp temp0 = temp_;
		const int tmpn0 = tmpn_;
		const std::vector<RawOp> pending0 = pending_;
		int j = start;
		E cond;
		try
		{
			while (j < b)
			{
				const Ucasm::Insn &ins = ins_[j];
				cur_ = j;
				if (ins.name == QLatin1String("branch_false") && target(ins) == T)
				{
					cond = pop(2).e;
					if (!onlyFinders())
						fail(QStringLiteral("stack not empty at loop test"));
					break;
				}
				if (ins.name == QLatin1String("branch") || ins.name == QLatin1String("branch_false")
					|| ins.name == QLatin1String("for_each") || ins.name == QLatin1String("for_each_string")
					|| ins.name == QLatin1String("return"))
					fail(QStringLiteral("loop head"));
				step(ins);
				j++;
			}
			if (!cond)
				fail(QStringLiteral("loop without test"));
		}
		catch (const Fail &)
		{
			stack_ = stack0;
			temp_ = temp0;
			tmpn_ = tmpn0;
			pending_ = pending0;
			stmts_ = std::move(saved);
			S w = stmt(Stmt::While);
			w->endless = true;
			addStmt(w);
			w->a = sub(start, b, inner);
			return T;
		}
		Block pre = std::move(stmts_);
		stmts_ = std::move(saved);
		S w = stmt(Stmt::While);
		w->cond = cond;
		w->pre = std::move(pre);
		addStmt(w);
		w->a = sub(j + 1, b, inner);
		return T;
	}

	Block sub(int lo, int hi, const Loop &loop)
	{
		std::vector<Ent> savedStack = std::move(stack_);
		const Temp savedTemp = temp_;
		stack_.clear();
		Block r = parse(lo, hi, loop);
		stack_ = std::move(savedStack);
		temp_ = savedTemp;
		return r;
	}
};

}

bool decompile(const QByteArray &record, int classId, QVector<Routine> *out, QString *error)
{
	out->clear();
	if (record.size() < 12 + 0x80)
	{
		if (error)
			*error = QObject::tr("the record is too short");
		return false;
	}
	const QByteArray code = record.mid(12);
	QVector<Ucasm::Insn> all;
	if (!Ucasm::decodeInsns(code, &all, error))
		return false;
	auto word = [&](int at) { return uint32_t(uint8_t(code[at])) | uint32_t(uint8_t(code[at + 1])) << 8
		| uint32_t(uint8_t(code[at + 2])) << 16 | uint32_t(uint8_t(code[at + 3])) << 24; };
	QHash<int, QString> eventAt;
	for (int e = 31; e >= 0; e--)
		if (const uint32_t off = word(e * 4))
			eventAt.insert(int(off), Ucode::eventName(e));
	int first = 0;
	while (first < all.size())
	{
		int last = first;
		while (last < all.size() && all[last].op != 0x79)
			last++;
		if (last < all.size())
			last++;
		Routine r;
		r.start = all[first].at;
		r.end = last < all.size() ? all[last].at : code.size();
		const QString ev = eventAt.value(r.start);
		r.name = ev.isEmpty() ? QStringLiteral("func_%1").arg(r.start, 4, 16, QLatin1Char('0')) : ev;
		const QVector<Ucasm::Insn> body = all.mid(first, last - first);
		try
		{
			Dec d(body);
			const bool locals = body.first().name == QLatin1String("locals");
			const int lo = locals ? 1 : 0;
			const int hi = body.last().name == QLatin1String("end") ? body.size() - 1 : body.size();
			r.tree = d.run(lo, hi);
			r.hasFrame = locals;
			r.frame = locals ? int(body.first().args[0]) : 0;
			QStringList lines;
			render(r.tree, 1, &lines);
			r.lifted = true;
			r.text = lines.join(QChar('\n'));
		}
		catch (const Fail &f)
		{
			r.why = f.why;
			r.tree.clear();
			r.text = Ucasm::printRange(record, classId, r.start, r.end);
		}
		out->append(r);
		first = last;
	}
	return true;
}

QString classText(const QByteArray &record, int classId, const Usecode *names, QString *error)
{
	QVector<Routine> routines;
	if (!decompile(record, classId, &routines, error))
		return QString();
	QString out = QStringLiteral("class %1").arg(classId);
	if (names && !names->className(classId).isEmpty())
		out += QStringLiteral(" %1").arg(names->className(classId));
	out += QStringLiteral("\n");
	for (const Routine &r : routines)
	{
		if (r.lifted)
			out += QStringLiteral("\nroutine %1 {    // %2\n%3\n}\n").arg(r.name).arg(r.start, 4, 16, QLatin1Char('0')).arg(r.text);
		else
			out += QStringLiteral("\nasm routine %1 {    // %2, not lifted: %3\n%4}\n").arg(r.name).arg(r.start, 4, 16, QLatin1Char('0'))
				.arg(r.why, r.text);
	}
	return out;
}

}
