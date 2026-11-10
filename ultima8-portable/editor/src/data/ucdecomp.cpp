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
	int size;	// bytes; -1 for a search's finder, which a later add_sp clears whole
	E e;
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
};

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
		while (ri > 0 && ins[ri - 1].name.startsWith(QStringLiteral("free_")))
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

	void push(int size, const E &e) { stack_.push_back({size, e}); }
	Ent pop(int size = 0)
	{
		if (stack_.empty())
			fail(QStringLiteral("stack empty"));
		Ent x = stack_.back();
		stack_.pop_back();
		if (size && x.size != size)
			fail(QStringLiteral("size %1 != %2").arg(x.size).arg(size));
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
			got += stack_[i].size;
		}
		if (got != nbytes)
			fail(QStringLiteral("args misaligned %1/%2").arg(got).arg(nbytes));
		return out;
	}
	void addStmt(const S &s) { stmts_.push_back(s); }
	bool onlyFinders() const
	{
		for (const Ent &x : stack_)
			if (x.size != -1)
				return false;
		return true;
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

	E tempValue()
	{
		if (temp_.kind == Temp::None)
			fail(QStringLiteral("no temp"));
		if (temp_.kind == Temp::CallT)
		{
			S st = temp_.call;
			E e;
			if (!st->consumed && !stmts_.empty() && stmts_.back() == st)
			{
				stmts_.pop_back();
				st->consumed = true;
				e = st->expr;
			}
			else
			{
				if (st->var.isEmpty())
					st->var = QStringLiteral("tmp%1").arg(tmpn_++);
				e = var(st->var);
			}
			temp_.kind = Temp::Val;
			temp_.val = e;
			return e;
		}
		return temp_.val;
	}

	E binary(const QString &op, const E &a, const E &b) { return make(Expr::Bin, op, {a, b}); }

	void callStmt(const E &call)
	{
		S st = stmt(Stmt::Call);
		st->expr = call;
		addStmt(st);
		temp_.kind = Temp::CallT;
		temp_.call = st;
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
			push(2, konst(a[0]));
		else if (is("push_long_imm"))
			push(4, konst(a[0]));
		else if (is("push_local") || is("push_byte_local"))
			push(2, var(varName(a[0])));
		else if (is("push_long_local"))
			push(4, var(varName(a[0])));
		else if (is("push_address"))
			push(4, make(Expr::Addr, QString(), {var(varName(a[0]))}));
		else if (is("push_string"))
		{
			auto e = std::make_shared<Expr>();
			e->kind = Expr::Str;
			e->bytes = i.text;
			push(2, e);
		}
		else if (is("string_to_ptr"))
			push(4, pop(2).e);
		else if (is("load_ptr"))
		{
			Ent p = pop(4);
			push(int(a[0]), make(Expr::Deref, QString(), {p.e}));
		}
		else if (is("store") || is("store_byte") || is("store_long"))
		{
			Ent x = pop(is("store_long") ? 4 : 2);
			S s = stmt(Stmt::Assign);
			s->lhs = varName(a[0]);
			s->expr = x.e;
			addStmt(s);
		}
		else if (is("store_ptr"))
		{
			Ent p = pop(4);
			Ent x = pop(int(a[0]));
			S s = stmt(Stmt::Assign);
			s->lhs = fmt(make(Expr::Deref, QString(), {p.e}));
			s->expr = x.e;
			addStmt(s);
		}
		else if (is("push_flag"))
		{
			auto e = std::make_shared<Expr>();
			e->kind = Expr::Flag;
			e->v = a[0];
			e->w = int(a[1]);
			push(2, e);
		}
		else if (is("store_flag"))
		{
			Ent x = pop(2);
			auto f = std::make_shared<Expr>();
			f->kind = Expr::Flag;
			f->v = a[0];
			f->w = int(a[1]);
			S s = stmt(Stmt::Assign);
			s->lhs = fmt(f);
			s->expr = x.e;
			addStmt(s);
		}
		else if (bins.contains(n))
		{
			Ent b = pop();
			Ent aa = pop();
			push(2, binary(bins[n], aa.e, b.e));
		}
		else if (n.endsWith(QLatin1String("_long")) && bins.contains(n.left(n.size() - 5)))
		{
			const QString base = n.left(n.size() - 5);
			Ent b = pop(4);
			Ent aa = pop(4);
			const bool test = base == QLatin1String("eq") || base == QLatin1String("ne") || base == QLatin1String("lt")
				|| base == QLatin1String("le") || base == QLatin1String("gt") || base == QLatin1String("ge")
				|| base == QLatin1String("and") || base == QLatin1String("or");
			push(test ? 2 : 4, binary(bins[base], aa.e, b.e));
		}
		else if (is("eq_strings"))
		{
			Ent b = pop(2);
			Ent aa = pop(2);
			push(2, binary(QStringLiteral("=="), aa.e, b.e));
		}
		else if (is("not"))
			push(2, make(Expr::Un, QStringLiteral("!"), {pop(2).e}));
		else if (is("not_long"))
			push(2, make(Expr::Un, QStringLiteral("!"), {pop(4).e}));
		else if (is("bit_not"))
			push(2, make(Expr::Un, QStringLiteral("~"), {pop(2).e}));
		else if (is("shift_left") || is("shift_right"))
		{
			Ent top = pop(2);
			Ent under = pop(2);
			push(2, binary(is("shift_left") ? QStringLiteral("<<") : QStringLiteral(">>"), top.e, under.e));
		}
		else if (is("word_to_long"))
			push(4, make(Expr::Cast, QStringLiteral("long"), {pop(2).e}));
		else if (is("long_to_word"))
			push(2, make(Expr::Cast, QStringLiteral("int"), {pop(4).e}));
		else if (is("push_pid"))
			push(2, make(Expr::Pid));
		else if (is("push_token"))
			push(1, konst(a[0]));
		else if (is("push_bytes_local"))
			push(int(a[1]), var(varName(a[0])));
		else if (is("push_string_local") || is("push_list_local") || is("push_string_list_local"))
			push(2, var(varName(a[0])));
		else if (is("push_string_address"))
			push(4, make(Expr::Addr, QString(), {var(varName(a[0]))}));
		else if (is("give_heap"))
		{
		}
		else if (is("push_result"))
			push(4, var(QStringLiteral("result")));
		else if (is("pop_result"))
		{
			Ent x = pop(4);
			S s = stmt(Stmt::Assign);
			s->lhs = QStringLiteral("result");
			s->expr = x.e;
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
				got += x.size;
				els.push_back(x.e);
			}
			if (got != size * count)
				fail(QStringLiteral("make_list misaligned"));
			push(2, make(Expr::Call, QStringLiteral("list"), els));
		}
		else if (is("in_list"))
		{
			Ent lst = pop(2);
			Ent el = pop(int(a[0]));
			push(2, binary(QStringLiteral("in"), el.e, lst.e));
		}
		else if (is("list_xor") || is("string_list_xor") || is("list_subtract") || is("string_list_subtract"))
		{
			Ent right = pop(2);
			Ent left = pop(2);
			push(2, binary(n.endsWith(QLatin1String("xor")) ? QStringLiteral("^") : QStringLiteral("-"), left.e, right.e));
		}
		else if (is("list_element"))
		{
			Ent index = pop(2);
			Ent list = pop(2);
			push(a[0] == 1 ? 2 : int(a[0]), make(Expr::Call, QStringLiteral("element"), {list.e, index.e}));
		}
		else if (is("store_bytes"))
		{
			const std::vector<E> v = topArgs(int(a[1]));
			if (v.size() != 1)
				fail(QStringLiteral("store_bytes of several values"));
			S st = stmt(Stmt::Assign);
			st->lhs = varName(a[0]);
			st->expr = v[0];
			addStmt(st);
		}
		else if (is("set_process_info"))
		{
			Ent r = pop(2);
			Ent t = pop(2);
			addStmt(plain(QStringLiteral("set_process_info(%1, %2)").arg(fmt(t.e), fmt(r.e))));
		}
		else if (is("exclusive"))
			addStmt(plain(QStringLiteral("exclusive()")));
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
				got += it->size;
				if (got > off)
					break;
			}
			if (!tgt)
				fail(QStringLiteral("sp address misaligned"));
			push(4, make(Expr::Ref, QString(), {tgt}));
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
				got += x.size;
				toks.push_back(x.e);
			}
			if (got != nbytes)
				fail(QStringLiteral("search tokens misaligned"));
			std::reverse(toks.begin(), toks.end());
			args.insert(args.end(), toks.begin(), toks.end());
			static const char *const kinds[] = {"", "", "area", "area_deep", "container", "container_deep", "on_top_of"};
			push(-1, make(Expr::Finder));
			push(2, make(Expr::Call, QStringLiteral("search_") + (kind >= 2 && kind <= 6 ? QString::fromLatin1(kinds[kind]) : QString::number(kind)), args, local));
		}
		else if (is("search_next"))
			push(2, make(Expr::Call, QStringLiteral("search_next")));
		else if (is("call_intrinsic"))
		{
			const int nbytes = int(a[0]);
			const Ucode::Intrinsic *in = Ucode::intrinsic(int(a[1]));
			QString name = in ? QString::fromLatin1(in->name) : QStringLiteral("intrinsic_%1").arg(a[1]);
			name = name.section(QChar('('), 0, 0);
			callStmt(make(Expr::Call, name, topArgs(nbytes)));
		}
		else if (is("call") || is("call_near"))
		{
			int nb = 0;
			if (cur_ + 1 < ins_.size() && ins_[cur_ + 1].name == QLatin1String("add_sp") && ins_[cur_ + 1].args[0] < 0)
				nb = int(-ins_[cur_ + 1].args[0]);
			const QString name = is("call") ? QStringLiteral("%1::%2").arg(a[0]).arg(a[1], 4, 16, QLatin1Char('0'))
				: QStringLiteral("near_%1").arg((i.at + i.size + a[0]) & 0xffff, 4, 16, QLatin1Char('0'));
			callStmt(make(Expr::Call, name, topArgs(nb)));
		}
		else if (is("spawn"))
		{
			Ent ptr = pop(4);
			std::vector<E> args{ptr.e};
			for (const E &x : topArgs(int(a[0])))
				args.push_back(x);
			callStmt(make(Expr::Call, QStringLiteral("spawn %1::%2").arg(a[2]).arg(a[3], 4, 16, QLatin1Char('0')), args));
		}
		else if (is("add_sp"))
		{
			const int k = int(a[0]);
			if (k < 0)
			{
				int got = 0;
				while (got < -k)
				{
					Ent x = pop();
					if (x.size == -1)
					{
						got = -k;
						break;
					}
					got += x.size;
				}
				if (got != -k)
					fail(QStringLiteral("add_sp misaligned"));
			}
			else if (k > 0)
				fail(QStringLiteral("add_sp +"));
		}
		else if (is("push_temp") || is("push_temp_byte"))
			push(2, tempValue());
		else if (is("push_temp_long"))
			push(4, tempValue());
		else if (is("pop_temp"))
		{
			temp_.kind = Temp::Val;
			temp_.val = pop(2).e;
		}
		else if (is("pop_temp_long"))
		{
			temp_.kind = Temp::Val;
			temp_.val = pop(4).e;
		}
		else if (is("implies"))
		{
			if (a[0] != 1 || a[1] != 1)
				fail(QStringLiteral("implies %1,%2").arg(a[0]).arg(a[1]));
			Ent x = pop(2);
			Ent own = pop(2);
			if (own.e->kind != Expr::Pid)
			{
				// A started earlier waits for B: a process chain, not this process waiting.
				S w = stmt(Stmt::Implies);
				w->cond = own.e;
				w->expr = x.e;
				addStmt(w);
				push(2, x.e);
			}
			else
			{
				S w = stmt(Stmt::Wait);
				w->expr = x.e;
				addStmt(w);
				push(2, x.e);
			}
		}
		else if (is("suspend"))
		{
			if (!(!stmts_.empty() && stmts_.back()->kind == Stmt::Wait && temp_.kind == Temp::Val))
				addStmt(plain(QStringLiteral("suspend()")));
		}
		else if (n.startsWith(QLatin1String("free_")) || is("return") || is("locals") || is("end"))
		{
		}
		else
			fail(QStringLiteral("op %1").arg(n));
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
				if (before && before->name == QLatin1String("branch"))
				{
					const int x = target(*before);
					if (x > t && retStart_ <= x && x <= retEnd_)
					{
						S s = stmt(Stmt::If);
						s->cond = cond;
						s->a = sub(i + 1, t - 1, loop);
						s->a.push_back(plain(QStringLiteral("return")));
						addStmt(s);
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
						s->a = sub(i + 1, t - 1, loop);
						s->b = sub(t, x, loop);
						s->hasElse = true;
						addStmt(s);
						i = x;
						continue;
					}
				}
				S s = stmt(Stmt::If);
				s->cond = cond;
				s->a = sub(i + 1, t, loop);
				addStmt(s);
				i = t;
				continue;
			}
			if (ins.name == QLatin1String("branch"))
			{
				const int t = target(ins);
				if (t == i + 1)
				{
					i++;
					continue;
				}
				if (loop.in && t == loop.exit - 1)
					addStmt(plain(QStringLiteral("continue")));
				else if (loop.in && t == loop.exit)
					addStmt(plain(QStringLiteral("break")));
				else if (loop.in && t == loop.start)
					addStmt(plain(QStringLiteral("continue")));
				else if (retStart_ <= t && t <= retEnd_)
					addStmt(plain(QStringLiteral("return")));
				else
					fail(QStringLiteral("goto"));
				i++;
				continue;
			}
			if (ins.name == QLatin1String("return"))
			{
				addStmt(plain(QStringLiteral("return")));
				i++;
				continue;
			}
			if (ins.name == QLatin1String("for_each") || ins.name == QLatin1String("for_each_string"))
				fail(QStringLiteral("for_each"));
			step(ins);
			i++;
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
		pop(2);
		E list = pop(2).e;
		Loop inner;
		inner.in = true;
		inner.start = start;
		inner.exit = T;
		S w = stmt(Stmt::While);
		w->each = true;
		w->lhs = varName(fe.args[0]);
		w->cond = list;
		w->a = sub(start + 1, b, inner);
		addStmt(w);
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
			stmts_ = std::move(saved);
			S w = stmt(Stmt::While);
			w->endless = true;
			w->a = sub(start, b, inner);
			addStmt(w);
			return T;
		}
		Block pre = std::move(stmts_);
		stmts_ = std::move(saved);
		S w = stmt(Stmt::While);
		w->cond = cond;
		w->pre = std::move(pre);
		w->a = sub(j + 1, b, inner);
		addStmt(w);
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
			const int lo = body.first().name == QLatin1String("locals") ? 1 : 0;
			const int hi = body.last().name == QLatin1String("end") ? body.size() - 1 : body.size();
			QStringList lines;
			render(d.run(lo, hi), 1, &lines);
			r.lifted = true;
			r.text = lines.join(QChar('\n'));
		}
		catch (const Fail &f)
		{
			r.why = f.why;
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
