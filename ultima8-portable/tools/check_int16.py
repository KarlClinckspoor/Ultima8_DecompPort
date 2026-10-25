#!/usr/bin/env python3
"""Finds expressions whose integer semantics changed between Borland C++ 3.1 (int = 16 bits)
and the host (int = 32 bits).

Under Borland, int and unsigned have the same rank, so int16 op uint16 is evaluated as
unsigned 16-bit; on the host both promote to (signed) 32-bit int.  The checker computes,
for every operand, the type Borland gave it ("btype": width + signedness, with int = 16)
and whether the host value can be negative, and reports

  a     relational (< > <= >=), / % >> (and /= %=) where Borland's common type is
        unsigned 16 bits and an operand is signed (int16/char, negative constant, ...)
  a-eq  the same for == and !=
  b     comparisons, compound assignments and other uses of ~x or -x where x is uint16
        or uint8 and the host result can differ (the value is not truncated back to
        16 bits before it matters)
  c     LOW: comparisons with an operand Borland computed in 16 bits and could wrap
        (a + b > c, a - b < 0 with uint16, ...).  c-uns: Borland computed it unsigned
        and the host value can be negative; c-wrap: only overflow differs.

Uses of ~x / -x that are provably harmless (truncated to <= 16 bits, masked with a
16-bit constant, modular compound assignment into a 16-bit lvalue) are counted as
"b-safe" and listed only with -v.

usage: python3 tools/check_int16.py [-v] [-j N] [files...]   (from ultima8-portable/)
"""
import ctypes
import os
import re
import sys
from multiprocessing import Pool

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIBCLANG = "/usr/lib/llvm-18/lib/libclang-18.so.1"

import clang.cindex as ci  # noqa: E402

if not ci.Config.loaded:
    if os.path.exists(LIBCLANG):
        ci.Config.set_library_file(LIBCLANG)
CK = ci.CursorKind
TK = ci.TypeKind

_lib = None


def lib():
    global _lib
    if _lib is None:
        _lib = ci.conf.lib
        _lib.clang_getCursorBinaryOperatorKind.argtypes = [ci.Cursor]
        _lib.clang_getCursorBinaryOperatorKind.restype = ctypes.c_int
        _lib.clang_getCursorUnaryOperatorKind.argtypes = [ci.Cursor]
        _lib.clang_getCursorUnaryOperatorKind.restype = ctypes.c_int
        _lib.clang_Cursor_Evaluate.argtypes = [ci.Cursor]
        _lib.clang_Cursor_Evaluate.restype = ctypes.c_void_p
        _lib.clang_EvalResult_getKind.argtypes = [ctypes.c_void_p]
        _lib.clang_EvalResult_getKind.restype = ctypes.c_int
        _lib.clang_EvalResult_getAsLongLong.argtypes = [ctypes.c_void_p]
        _lib.clang_EvalResult_getAsLongLong.restype = ctypes.c_longlong
        _lib.clang_EvalResult_dispose.argtypes = [ctypes.c_void_p]
        _lib.clang_EvalResult_dispose.restype = None
    return _lib


# CXBinaryOperatorKind / CXUnaryOperatorKind (clang-c/Index.h, LLVM 17+)
B_MUL, B_DIV, B_REM, B_ADD, B_SUB, B_SHL, B_SHR = 3, 4, 5, 6, 7, 8, 9
B_LT, B_GT, B_LE, B_GE, B_EQ, B_NE = 11, 12, 13, 14, 15, 16
B_AND, B_XOR, B_OR, B_LAND, B_LOR, B_ASSIGN = 17, 18, 19, 20, 21, 22
B_MULA, B_DIVA, B_REMA, B_ADDA, B_SUBA, B_SHLA, B_SHRA, B_ANDA, B_XORA, B_ORA = range(23, 33)
B_COMMA = 33
U_PLUS, U_MINUS, U_NOT, U_LNOT = 7, 8, 9, 10
REL = {B_LT, B_GT, B_LE, B_GE}
EQ = {B_EQ, B_NE}
ARITH = {B_MUL, B_DIV, B_REM, B_ADD, B_SUB, B_AND, B_XOR, B_OR}
WRAPPING = {B_ADD, B_SUB, B_MUL, B_SHL}
MODULAR_CA = {B_MULA, B_ADDA, B_SUBA, B_SHLA, B_ANDA, B_XORA, B_ORA}
OPNAME = {B_MUL: "*", B_DIV: "/", B_REM: "%", B_ADD: "+", B_SUB: "-", B_SHL: "<<", B_SHR: ">>",
          B_LT: "<", B_GT: ">", B_LE: "<=", B_GE: ">=", B_EQ: "==", B_NE: "!=", B_AND: "&",
          B_XOR: "^", B_OR: "|", B_MULA: "*=", B_DIVA: "/=", B_REMA: "%=", B_ADDA: "+=",
          B_SUBA: "-=", B_SHLA: "<<=", B_SHRA: ">>=", B_ANDA: "&=", B_XORA: "^=", B_ORA: "|="}

S16, U16, S32, U32, S64, U64 = (16, True), (16, False), (32, True), (32, False), (64, True), (64, False)
CASTS = {CK.CSTYLE_CAST_EXPR, CK.CXX_FUNCTIONAL_CAST_EXPR, CK.CXX_STATIC_CAST_EXPR,
         CK.CXX_REINTERPRET_CAST_EXPR, CK.CXX_CONST_CAST_EXPR}


def binop(c):
    return lib().clang_getCursorBinaryOperatorKind(c)


def unop(c):
    return lib().clang_getCursorUnaryOperatorKind(c)


def evaluate(c):
    """Integer value of a constant expression, or None."""
    if c.kind not in (CK.INTEGER_LITERAL, CK.UNARY_OPERATOR, CK.BINARY_OPERATOR, CK.PAREN_EXPR,
                      CK.DECL_REF_EXPR, CK.UNEXPOSED_EXPR, CK.CSTYLE_CAST_EXPR,
                      CK.CHARACTER_LITERAL):
        return None
    t = c.type.get_canonical().kind
    if t not in INT_KINDS and t != TK.ENUM:
        return None
    r = lib().clang_Cursor_Evaluate(c)
    if not r:
        return None
    try:
        if lib().clang_EvalResult_getKind(r) != 1:  # CXEval_Int
            return None
        return lib().clang_EvalResult_getAsLongLong(r)
    finally:
        lib().clang_EvalResult_dispose(r)


INT_KINDS = {TK.SHORT: S16, TK.USHORT: U16, TK.CHAR_S: (8, True), TK.SCHAR: (8, True),
             TK.UCHAR: (8, False), TK.CHAR_U: (8, False), TK.BOOL: (8, False),
             TK.INT: S32, TK.UINT: U32, TK.LONG: S64, TK.ULONG: U64,
             TK.LONGLONG: S64, TK.ULONGLONG: U64, TK.WCHAR: S32, TK.CHAR16: U16,
             TK.CHAR32: U32}


def host_bt(t):
    """Borland view of a host type (int32_t was long, so host int stays 32 bits)."""
    t = t.get_canonical()
    if t.kind == TK.ENUM:
        d = t.get_declaration()
        try:
            u = d.enum_type.get_canonical().kind
        except Exception:
            return S16
        # an enum with a fixed 16/32-bit underlying type keeps it; a plain enum was int
        if u in (TK.USHORT, TK.SHORT, TK.UCHAR, TK.SCHAR, TK.CHAR_S, TK.ULONG, TK.LONG) or (
                u in (TK.INT, TK.UINT) and is_fixed_enum(d)):
            return INT_KINDS.get(u, S16)
        return S16
    return INT_KINDS.get(t.kind)


_fixed_enum = {}


def is_fixed_enum(d):
    key = d.hash
    if key not in _fixed_enum:
        toks = [x.spelling for x in d.get_tokens()][:6]
        _fixed_enum[key] = ":" in toks[:toks.index("{")] if "{" in toks else ":" in toks
    return _fixed_enum[key]


def promote(bt):
    if bt is None:
        return None
    if bt[0] < 16:
        return S16
    return bt


def uac(a, b):
    a, b = promote(a), promote(b)
    if a is None or b is None:
        return None
    w = max(a[0], b[0])
    if w == 16:
        return U16 if (not a[1] or not b[1]) else S16
    # wider: unsigned wins only if the unsigned operand has the full width
    for x in (a, b):
        if x[0] == w and not x[1]:
            return (w, False)
    return (w, True)


def width(t):
    bt = host_bt(t)
    return bt[0] if bt else None


def peel(c):
    """Strip parentheses and implicit casts."""
    while True:
        if c.kind == CK.PAREN_EXPR or c.kind == CK.UNEXPOSED_EXPR:
            ch = list(c.get_children())
            if len(ch) != 1:
                return c
            c = ch[0]
        else:
            return c


LIT_RE = re.compile(r"^(0[xX][0-9a-fA-F]+|0[0-7]*|[1-9][0-9]*)([uUlL]*)$")


class TU:
    def __init__(self, tu):
        self.tu = tu
        self.macros = {}
        for c in tu.cursor.get_children():
            if c.kind == CK.MACRO_DEFINITION:
                toks = [t.spelling for t in c.get_tokens()][1:]
                while len(toks) >= 3 and toks[0] == "(" and toks[-1] == ")":
                    toks = toks[1:-1]
                if len(toks) == 1:
                    self.macros[c.spelling] = toks[0]
        self.btc = {}
        self.ngc = {}

    def lit_bt(self, c):
        toks = [t.spelling for t in c.get_tokens()]
        # for a literal spelled in a macro the extent starts at its spelling, so the first
        # token is the literal itself (or the macro name)
        txt = toks[0] if toks else None
        seen = 0
        while txt in self.macros and seen < 8:
            txt = self.macros[txt]
            seen += 1
        m = LIT_RE.match(txt or "")
        if not m:
            v = evaluate(c)
            if v is None:
                return S16
            return S16 if -32768 <= v <= 32767 else S32
        body, suf = m.group(1), m.group(2).lower()
        v = int(body, 16) if body[:2] in ("0x", "0X") else (int(body, 8) if body.startswith("0") else int(body))
        dec = body[0] != "0" or body == "0"
        if "u" in suf and "l" in suf:
            return U32
        if "u" in suf:
            return U16 if v <= 0xFFFF else U32
        if "l" in suf:
            return S32 if v <= 0x7FFFFFFF else U32
        if v <= 0x7FFF:
            return S16
        if not dec and v <= 0xFFFF:
            return U16
        return S32 if v <= 0x7FFFFFFF else U32

    def bt(self, c):
        """Type Borland gave the (peeled) expression."""
        c = peel(c)
        k = c.hash
        if k in self.btc:
            return self.btc[k]
        r = self._bt(c)
        self.btc[k] = r
        return r

    def _bt(self, c):
        if c.kind == CK.INTEGER_LITERAL:
            return self.lit_bt(c)
        if c.kind == CK.CHARACTER_LITERAL:
            return (8, True)
        if c.kind == CK.CXX_UNARY_EXPR:  # sizeof
            return U16
        if c.kind == CK.UNARY_OPERATOR:
            op = unop(c)
            if op in (U_PLUS, U_MINUS, U_NOT):
                ch = list(c.get_children())
                return promote(self.bt(ch[0])) if ch else None
            if op == U_LNOT:
                return S16
            return host_bt(c.type)
        if c.kind == CK.BINARY_OPERATOR:
            op = binop(c)
            ch = list(c.get_children())
            if len(ch) != 2:
                return host_bt(c.type)
            if op in ARITH:
                return uac(self.bt(ch[0]), self.bt(ch[1]))
            if op in (B_SHL, B_SHR):
                return promote(self.bt(ch[0]))
            if op in REL or op in EQ or op in (B_LAND, B_LOR):
                return S16
            if op == B_COMMA:
                return self.bt(ch[1])
            return host_bt(c.type)
        if c.kind == CK.CONDITIONAL_OPERATOR:
            ch = list(c.get_children())
            if len(ch) == 3:
                return uac(self.bt(ch[1]), self.bt(ch[2]))
        return host_bt(c.type)

    def reasons(self, c):
        """Why the host value of c can be negative: set of 'signed', 'unary', 'arith'.
        Empty set: never negative on the host."""
        c = peel(c)
        k = c.hash
        if k in self.ngc:
            return self.ngc[k]
        self.ngc[k] = frozenset()
        r = frozenset(self._reasons(c))
        self.ngc[k] = r
        return r

    def _reasons(self, c):
        v = evaluate(c)
        if v is not None:
            return {"signed"} if v < 0 else set()
        hb = host_bt(c.type)
        if hb is None:
            return set()
        if c.kind == CK.UNARY_OPERATOR:
            op = unop(c)
            ch = list(c.get_children())
            if op in (U_MINUS, U_NOT) and ch:
                inner = self.bt(ch[0])
                if inner and inner[0] <= 16 and not inner[1]:
                    return {"unary"}
                return {"signed"} | set(self.reasons(ch[0]))
            if op == U_PLUS and ch:
                return set(self.reasons(ch[0]))
            if op == U_LNOT:
                return set()
        if c.kind == CK.BINARY_OPERATOR:
            op = binop(c)
            ch = list(c.get_children())
            if len(ch) == 2:
                a, b = self.reasons(ch[0]), self.reasons(ch[1])
                if op == B_AND:
                    return set(a & b) if (a and b) else set()
                if op in (B_OR, B_XOR, B_ADD, B_MUL, B_DIV, B_REM):
                    r = set(a | b)
                    # 16-bit arithmetic that only differs on the host because of a ~x inside
                    return r
                if op == B_SUB:
                    return set(a | b) | {"arith"}
                if op in (B_SHL, B_SHR):
                    return set(a)
                if op in REL or op in EQ or op in (B_LAND, B_LOR):
                    return set()
                if op == B_COMMA:
                    return set(b)
                if op >= B_ASSIGN:
                    return {"signed"} if hb[1] else set()
        if c.kind == CK.CONDITIONAL_OPERATOR:
            ch = list(c.get_children())
            if len(ch) == 3:
                return set(self.reasons(ch[1]) | self.reasons(ch[2]))
        if c.kind in CASTS:
            ch = list(c.get_children())
            if not hb[1]:
                return set()  # cast to unsigned: never negative
            if not ch:
                return {"signed"}
            src = host_bt(peel(ch[-1]).type)
            if src is None:
                return {"signed"}
            if not src[1]:
                return set() if src[0] < hb[0] else {"signed"}
            r = set(self.reasons(ch[-1]))
            return r | {"signed"} if src[0] > hb[0] else r
        return {"signed"} if hb[1] else set()


def is_wrapping_arith(tu, c):
    """c (peeled) is +, -, *, << computed by Borland in 16 bits."""
    c = peel(c)
    if c.kind != CK.BINARY_OPERATOR or binop(c) not in WRAPPING:
        return False
    if evaluate(c) is not None:
        return False
    b = tu.bt(c)
    return b is not None and b[0] == 16


def classify(rs):
    if "unary" in rs:
        return "b"
    if "arith" in rs:
        return "c"
    return "a"


def walk(path, args, want_files):
    idx = ci.Index.create()
    opts = ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD | ci.TranslationUnit.PARSE_INCOMPLETE
    tu_ = idx.parse(path, args=args, options=opts)
    tu = TU(tu_)
    out = []
    lines_cache = {}

    def srcline(f, n):
        if f not in lines_cache:
            try:
                with open(f, errors="replace") as fh:
                    lines_cache[f] = fh.read().split("\n")
            except OSError:
                lines_cache[f] = []
        L = lines_cache[f]
        return L[n - 1].strip() if 0 < n <= len(L) else ""

    def report(c, cat, why):
        loc = c.location
        if not loc.file:
            return
        f = os.path.realpath(loc.file.name)
        if not want_files(f):
            return
        rel = os.path.relpath(f, ROOT)
        out.append((cat, rel, loc.line, loc.column, why, srcline(f, loc.line)))

    def check_cmp(c, op, l, r):
        bl, br = tu.bt(l), tu.bt(r)
        cb = uac(bl, br)
        if cb is None:
            return
        rel = op in REL
        cats = []
        for x, bx in ((l, bl), (r, br)):
            rs = tu.reasons(x)
            if cb == U16:
                if rs:
                    cat = classify(rs)
                    if cat == "a" and op in EQ:
                        cat = "a-eq"
                    if cat == "c" and op not in REL and op not in EQ:
                        cat = "c"
                    cats.append((cat, "unsigned 16-bit in Borland, operand can be negative on host"))
            elif cb[0] >= 32 and promote(bx) == U16 and rs:
                cats.append((classify(rs), "Borland zero-extends a uint16 operand the host sees as negative"))
            if (rel or op in EQ) and is_wrapping_arith(tu, x) and not ("unary" in rs):
                b = tu.bt(x)
                kind = "c-uns" if (b == U16 and rs) else "c-wrap"
                cats.append((kind, "operand computed in 16 bits by Borland"))
        seen = set()
        for cat, why in cats:
            if cat in ("c",) :
                cat = "c-uns"
            if cat in seen:
                continue
            seen.add(cat)
            report(c, cat, "%s: %s" % (OPNAME.get(op, op), why))

    def check_shr(c, l):
        if promote(tu.bt(l)) == U16:
            rs = tu.reasons(l)
            if rs:
                cat = classify(rs)
                report(c, "c-uns" if cat == "c" else cat, ">>: Borland shifts unsigned, host value can be negative")

    def check_ca(c, op, l, r):
        wl = width(l.type)
        if wl is None:
            return
        rs = tu.reasons(r)
        br = tu.bt(r)
        if op in (B_DIVA, B_REMA):
            check_cmp(c, B_DIV, l, r)
            return
        if op == B_SHRA:
            return
        if op in MODULAR_CA:
            if wl <= 16:
                if "unary" in rs:
                    report(c, "b-safe", "%s into %d-bit lvalue is modular" % (OPNAME[op], wl))
                return
            if promote(br) == U16 and rs:
                cat = classify(rs)
                report(c, "c-uns" if cat == "c" else cat,
                       "%s into %d-bit lvalue: Borland zero-extends the uint16 rhs" % (OPNAME[op], wl))

    # consumer analysis for ~x / -x on 16-bit unsigned operands
    def check_unary_use(c, stack, fn_ret):
        child = c
        for i in range(len(stack) - 1, -1, -1):
            p = stack[i]
            k = p.kind
            if k == CK.PAREN_EXPR:
                child = p
                continue
            if k == CK.UNEXPOSED_EXPR:
                w = width(p.type)
                if p.type.get_canonical().kind == TK.BOOL:
                    return report(c, "b", "~/- of uint16 used as a truth value")
                if w is not None and w <= 16:
                    return report(c, "b-safe", "truncated to %d bits" % w)
                if w is not None and p.type.get_canonical().kind != TK.INT:
                    return report(c, "b", "~/- of uint16 converted to %s" % p.type.spelling)
                child = p
                continue
            if k in CASTS:
                w = width(p.type)
                if w is not None and w <= 16:
                    return report(c, "b-safe", "cast to %d bits" % w)
                return report(c, "b", "~/- of uint16 cast to a %s-bit type" % w)
            if k == CK.UNARY_OPERATOR:
                op = unop(p)
                if op in (U_MINUS, U_NOT, U_PLUS):
                    child = p
                    continue
                if op == U_LNOT:
                    return report(c, "b", "~/- of uint16 used as a truth value (!)")
                return report(c, "b", "~/- of uint16 under unary operator")
            if k == CK.BINARY_OPERATOR:
                op = binop(p)
                ch = list(p.get_children())
                if len(ch) != 2:
                    return
                me = 0 if ch[0].hash == child.hash else 1
                other = ch[1 - me]
                if op in REL or op in EQ or op in (B_DIV, B_REM) or (op == B_SHR and me == 0):
                    return  # reported by the operator checks
                if op == B_AND:
                    bo = tu.bt(other)
                    if bo and promote(bo)[0] == 16 and not tu.reasons(other):
                        return report(c, "b-safe", "masked with a non-negative 16-bit value")
                if op in (B_AND, B_OR, B_XOR, B_ADD, B_SUB, B_MUL) or (op == B_SHL and me == 0):
                    bo = tu.bt(other)
                    if bo and promote(bo)[0] > 16:
                        return report(c, "b", "~/- of uint16 combined (%s) with a 32-bit operand" % OPNAME[op])
                    child = p
                    continue
                if op in (B_LAND, B_LOR):
                    return report(c, "b", "~/- of uint16 used as a truth value (%s)" % ("&&" if op == B_LAND else "||"))
                if op == B_ASSIGN and me == 1:
                    w = width(ch[0].type)
                    if w is not None and w <= 16:
                        return report(c, "b-safe", "assigned to %d-bit lvalue" % w)
                    return report(c, "b", "~/- of uint16 assigned to a %s-bit lvalue" % w)
                if op == B_COMMA and me == 0:
                    return
                return report(c, "b", "~/- of uint16 as operand of %s" % OPNAME.get(op, op))
            if k == CK.COMPOUND_ASSIGNMENT_OPERATOR:
                return  # check_ca
            if k == CK.CONDITIONAL_OPERATOR:
                ch = list(p.get_children())
                if ch and ch[0].hash == child.hash:
                    return report(c, "b", "~/- of uint16 used as a truth value (?:)")
                child = p
                continue
            if k == CK.VAR_DECL:
                w = width(p.type)
                if w is not None and w <= 16:
                    return report(c, "b-safe", "initialises a %d-bit variable" % w)
                return report(c, "b", "~/- of uint16 initialises a %s-bit variable" % w)
            if k == CK.RETURN_STMT:
                w = width(fn_ret) if fn_ret is not None else None
                if w is not None and w <= 16:
                    return report(c, "b-safe", "returned as %d bits" % w)
                return report(c, "b", "~/- of uint16 returned as a %s-bit value" % w)
            if k in (CK.IF_STMT, CK.WHILE_STMT, CK.DO_STMT, CK.FOR_STMT):
                return report(c, "b", "~/- of uint16 used as a condition")
            if k == CK.CALL_EXPR:
                return report(c, "b", "~/- of uint16 passed as a 32-bit argument")
            if k == CK.ARRAY_SUBSCRIPT_EXPR:
                return report(c, "b", "~/- of uint16 used as an array index")
            if k in (CK.COMPOUND_STMT, CK.CASE_STMT, CK.SWITCH_STMT):
                return report(c, "b-safe", "value unused / switch")
            return report(c, "b", "~/- of uint16 used in %s" % k.name)

    def visit(c, stack, fn_ret, in_file):
        k = c.kind
        if k in (CK.FUNCTION_DECL, CK.CXX_METHOD, CK.CONSTRUCTOR, CK.DESTRUCTOR, CK.FUNCTION_TEMPLATE,
                 CK.CONVERSION_FUNCTION):
            try:
                fn_ret = c.result_type
            except Exception:
                fn_ret = None
        if k == CK.BINARY_OPERATOR:
            op = binop(c)
            ch = list(c.get_children())
            if len(ch) == 2:
                l, r = peel(ch[0]), peel(ch[1])
                if op in REL or op in EQ or op in (B_DIV, B_REM):
                    check_cmp(c, op, l, r)
                elif op == B_SHR:
                    check_shr(c, l)
        elif k == CK.COMPOUND_ASSIGNMENT_OPERATOR:
            op = binop(c)
            ch = list(c.get_children())
            if len(ch) == 2:
                check_ca(c, op, peel(ch[0]), peel(ch[1]))
        elif k == CK.UNARY_OPERATOR and unop(c) in (U_MINUS, U_NOT):
            ch = list(c.get_children())
            if ch:
                # also constants: ~0x8000 is 0x7FFF to Borland, 0xFFFF7FFF on the host
                b = tu.bt(ch[0])
                if b == U16:
                    check_unary_use(c, stack, fn_ret)
        elif k == CK.CASE_STMT and switches:
            check_case(c, switches[-1])
        is_switch = k == CK.SWITCH_STMT
        if is_switch:
            ch = [x for x in c.get_children() if x.kind != CK.VAR_DECL]
            switches.append(peel(ch[0]) if len(ch) == 2 else None)
        stack.append(c)
        for x in c.get_children():
            visit(x, stack, fn_ret, in_file)
        stack.pop()
        if is_switch:
            switches.pop()

    def check_case(c, cond):
        """Borland converted a case label to the (promoted, 16-bit) type of the switch."""
        if cond is None:
            return
        ch = list(c.get_children())
        if not ch:
            return
        v = evaluate(peel(ch[0]))
        b = promote(tu.bt(cond))
        if v is None or b is None or b[0] != 16:
            return
        if b == S16 and 0x8000 <= v <= 0xFFFF:
            report(c, "a-eq", "case %#x on a signed 16-bit switch: Borland matched %d" % (v, v - 0x10000))
        elif b == U16 and -0x8000 <= v < 0:
            rs = tu.reasons(cond)
            if not rs:
                report(c, "a-eq", "case %d on an unsigned 16-bit switch: Borland matched %#x" % (v, v & 0xFFFF))
        elif b == U16 and v >= 0x8000 and tu.reasons(cond):
            cat = classify(tu.reasons(cond))
            report(c, "c-uns" if cat == "c" else cat,
                   "case %#x: switch value is unsigned 16-bit in Borland, can be negative on host" % v)

    switches = []
    for top in tu_.cursor.get_children():
        f = top.location.file
        if not f:
            continue
        fp = os.path.realpath(f.name)
        if not want_files(fp):
            continue
        visit(top, [], None, fp)
    return out


def game_file(f):
    g = os.path.join(ROOT, "game") + os.sep
    return f.startswith(g) and "/third_party/" not in f and "/platform/" not in f


def include_args():
    inc = []
    for top in ("game", "platform"):
        for d, _, _ in sorted(os.walk(os.path.join(ROOT, top))):
            inc.append("-I" + d)
    return ["-x", "c++", "-std=gnu++20", "-fsigned-char", "-fno-delete-null-pointer-checks",
            "-Wno-everything"] + inc


def job(path):
    sys.setrecursionlimit(20000)
    try:
        return walk(path, include_args(), game_file)
    except Exception as e:  # keep going on one bad file
        return [("error", os.path.relpath(path, ROOT), 0, 0, repr(e), "")]


ORDER = ["a", "a-eq", "b", "c-uns", "c-wrap", "b-safe", "error"]
TITLE = {"a": "a: relational / div / mod with signed vs uint16 (Borland compared unsigned)",
         "a-eq": "a-eq: == / != with a possibly negative operand vs uint16",
         "b": "b: ~x / -x of uint16 or uint8 where the host result can differ",
         "c-uns": "c (LOW): 16-bit arithmetic Borland computed unsigned, host can be negative",
         "c-wrap": "c (LOW): 16-bit arithmetic that Borland would wrap on overflow",
         "b-safe": "b-safe: ~x / -x of uint16 that is truncated / masked before use (harmless)",
         "error": "errors"}


def main():
    argv = sys.argv[1:]
    verbose = "-v" in argv
    argv = [a for a in argv if a != "-v"]
    jobs = os.cpu_count() or 2
    if "-j" in argv:
        i = argv.index("-j")
        jobs = int(argv[i + 1])
        del argv[i:i + 2]
    if argv:
        files = [os.path.realpath(a) for a in argv]
    else:
        files = []
        for d, _, fs in os.walk(os.path.join(ROOT, "game")):
            files += [os.path.join(d, f) for f in fs if f.endswith((".cpp", ".c"))]
        files = sorted(f for f in files if game_file(f))
    found = {}
    with Pool(jobs) as p:
        for res in p.imap_unordered(job, files):
            for cat, rel, line, col, why, text in res:
                found.setdefault((cat, rel, line, col), (why, text))
    by = {}
    for (cat, rel, line, col), (why, text) in found.items():
        by.setdefault(cat, []).append((rel, line, col, why, text))
    for cat in ORDER:
        items = sorted(by.get(cat, []))
        if not items:
            continue
        print("== %s: %d" % (TITLE[cat], len(items)))
        if cat == "b-safe" and not verbose:
            print("   (use -v to list)")
            continue
        for rel, line, col, why, text in items:
            if cat.startswith("c"):
                print("  %s:%d:%d  %s" % (rel, line, col, text))
            else:
                print("  %s:%d:%d  [%s]  %s" % (rel, line, col, why, text))
        print()
    print("totals: " + ", ".join("%s=%d" % (c, len(by.get(c, []))) for c in ORDER if c != "error"
                                 or by.get(c)))


if __name__ == "__main__":
    main()
