// The tree usecode is lifted to: expressions and statements, printed as C-like text. Shared by the
// decompiler (which builds it from stack code), the parser (from text) and the code generator.
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <memory>
#include <vector>

namespace Ucast
{

struct Expr;
using E = std::shared_ptr<const Expr>;

struct Expr
{
	enum Kind { Const, Var, Str, Bin, Un, Deref, Addr, Flag, Call, Pid, Cast, Ref, Finder };
	Kind kind = Const;
	int64_t v = 0;		// a constant; a flag's bit; a search's local
	int w = 0;		// a flag's width
	QString s;		// a variable's name; an operator; a call's name; a cast's type
	QByteArray bytes;	// a string
	std::vector<E> k;	// operands, arguments
};

E make(Expr::Kind kind, const QString &s = QString(), std::vector<E> k = {}, int64_t v = 0);
E konst(int64_t v);
E var(const QString &name);
E flag(int64_t bit, int width);
E str(const QByteArray &bytes);

// The name a local (negative offset) or an argument has in the text.
QString varNameOf(int64_t offset);
int prec(const QString &op);
QString fmt(const E &e, int outer = 0);

struct Stmt;
using S = std::shared_ptr<Stmt>;
using Block = std::vector<S>;

struct Stmt
{
	enum Kind { Assign, Plain, Wait, Implies, Call, If, While };
	Kind kind = Plain;
	QString lhs;		// Assign; a foreach's variable
	QString text;		// Plain
	QString var;		// Call: the temporary its result is kept in, if any
	E expr;			// Assign's value; Call; Wait; Implies' second
	E cond;			// If, While; Implies' first
	Block a;		// If: then; While: body
	Block b;		// If: else
	Block pre;		// While: statements the test needs first
	bool hasElse = false;
	bool endless = false;
	bool each = false;	// While: foreach (lhs in cond)
	bool consumed = false;	// Call: its result was taken by a later instruction
};

S stmt(Stmt::Kind kind);
S plain(const QString &text);
void render(const Block &block, int indent, QStringList *out);

}
