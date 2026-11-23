//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "ast.h"

#include <charconv>
#include <cstdio>
#include <map>
#include <ostream>

namespace ast {

namespace {

template <class... F>
struct overloaded : F... {
    using F::operator()...;
};
template <class... F>
overloaded(F...) -> overloaded<F...>;

const char *const TYPE_NAMES[]     = { "integer", "real", "boolean", "string", "bits", "none" };
const char *const STD_FUNC_NAMES[] = { "abs",  "sign", "sqrt",   "sin",       "cos",  "arctan",
                                       "ln",   "exp",  "entier", "length",    "read", "resym",
                                       "xeen", "even", "random", "timeofday", "sum" };
const char *const STD_PROC_NAMES[] = { "stop",      "load",          "loadgo",    "assign",
                                       "atos",      "stoa",          "ininteger", "inreal",
                                       "insymbol",  "outinteger",    "outreal",   "outstring",
                                       "outsymbol", "outterminator", "fault",     "printtext",
                                       "prsym",     "space",         "tab",       "nlcr",
                                       "absfixt",   "fixt",          "flot",      "setrandom" };
const char *const IO_OP_NAMES[]    = { "print",  "punch",  "read",      "write",  "decode",
                                       "encode", "rewind", "backspace", "endfile" };
const char *const UNARY_NAMES[]    = { "neg", "not" };
const char *const BINARY_NAMES[]   = {
    "add", "sub", "mul", "div", "intdiv", "power", "lt",   "le",
    "eq",  "ne",  "ge",  "gt",  "and",    "or",    "impl", "equiv"
};
const char *const CONVERSION_NAMES[] = { "int_to_real", "real_to_int" };
const char *const SPEC_NAMES[]       = { "simple", "array", "procedure", "label", "switch" };

//
// A YAML node: a scalar, or a mapping or sequence in block or flow style.
//
struct Node {
    enum Kind { SCALAR, MAP, SEQ } kind{ SCALAR };
    bool flow{};
    std::string text;
    std::vector<std::pair<std::string, Node>> map;
    std::vector<Node> seq;

    Node &add(const std::string &key, Node value)
    {
        map.emplace_back(key, std::move(value));
        return *this;
    }
};

Node scalar(const std::string &s)
{
    Node n;
    n.text = s;
    return n;
}

Node quoted(const std::string &s)
{
    std::string q = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            q += '\\';
            q += char(c);
        } else if (c < 040) {
            static const char hex[] = "0123456789abcdef";
            q += "\\x";
            q += hex[c >> 4];
            q += hex[c & 15];
        } else {
            q += char(c);
        }
    }
    return scalar(q + '"');
}

Node mapping(const char *kind, bool flow = false)
{
    Node n;
    n.kind = Node::MAP;
    n.flow = flow;
    n.add("kind", scalar(kind));
    return n;
}

Node sequence()
{
    Node n;
    n.kind = Node::SEQ;
    return n;
}

std::string flow_text(const Node &n)
{
    if (n.kind == Node::SCALAR)
        return n.text;
    std::string s = n.kind == Node::MAP ? "{" : "[";
    bool first    = true;
    if (n.kind == Node::MAP) {
        for (const auto &[k, v] : n.map) {
            s += (first ? "" : ", ") + k + ": " + flow_text(v);
            first = false;
        }
    } else {
        for (const auto &v : n.seq) {
            s += (first ? "" : ", ") + flow_text(v);
            first = false;
        }
    }
    return s + (n.kind == Node::MAP ? "}" : "]");
}

bool inline_node(const Node &n)
{
    return n.kind == Node::SCALAR || n.flow || (n.kind == Node::SEQ && n.seq.empty());
}

void emit_entries(std::ostream &out, const Node &n, int indent, bool first_inline);

// The value after "key:" or "-".
void emit_value(std::ostream &out, const Node &n, int indent)
{
    if (inline_node(n)) {
        out << ' ' << flow_text(n) << '\n';
        return;
    }
    out << '\n';
    emit_entries(out, n, indent, false);
}

void emit_entries(std::ostream &out, const Node &n, int indent, bool first_inline)
{
    std::string pad(indent, ' ');
    if (n.kind == Node::MAP) {
        for (const auto &[k, v] : n.map) {
            out << (first_inline ? "" : pad) << k << ':';
            first_inline = false;
            emit_value(out, v, indent + 2);
        }
        return;
    }
    for (const auto &v : n.seq) {
        out << (first_inline ? "" : pad) << '-';
        first_inline = false;
        if (v.kind == Node::MAP && !v.flow) {
            out << ' ';
            emit_entries(out, v, indent + 2, true);
        } else {
            emit_value(out, v, indent + 2);
        }
    }
}

std::string number(int64_t v)
{
    return std::to_string(v);
}

std::string real_text(double v)
{
    char buf[64];
    auto r = std::to_chars(buf, buf + sizeof(buf), v);
    std::string s(buf, r.ptr);
    if (s.find_first_of(".en") == std::string::npos)
        s += ".0";
    return s;
}

class Printer {
public:
    explicit Printer(const Program &p) { collect(p); }

    Node program(const Program &p)
    {
        Node n;
        n.kind           = Node::MAP;
        Node extern_list = sequence();
        for (const auto &e : p.externals) {
            Node x = mapping(e.kind == External::CODE ? "code" : "algol", true);
            x.add("name", quoted(e.name)).add("id", scalar(number(e.id)));
            if (e.type != Type::NONE)
                x.add("type", scalar(to_string(e.type)));
            extern_list.seq.push_back(std::move(x));
        }
        n.add("externals", std::move(extern_list));
        if (p.unit)
            n.add("unit", scalar("true"));
        n.add("block", stmt(*p.block));
        return n;
    }

private:
    std::map<int, std::string> names_;

    void collect(const Program &p)
    {
        for (const auto &e : p.externals)
            names_[e.id] = e.name;
        collect(*p.block);
    }

    void collect(const Stmt &s)
    {
        std::visit(overloaded{
                       [this](const stmt::Block &b) {
                           for (const auto &d : b.decls)
                               collect(*d);
                           for (const auto &x : b.body)
                               collect(*x);
                       },
                       [this](const stmt::Compound &b) {
                           for (const auto &x : b.body)
                               collect(*x);
                       },
                       [this](const stmt::Labeled &l) { collect(*l.stmt); },
                       [this](const stmt::If &i) {
                           collect(*i.then_stmt);
                           if (i.else_stmt)
                               collect(*i.else_stmt);
                       },
                       [this](const stmt::For &f) { collect(*f.body); },
                       [](const auto &) {},
                   },
                   s.node);
    }

    void collect(const Decl &d)
    {
        std::visit(overloaded{
                       [this](const decl::Var &v) { names_[v.id] = v.name; },
                       [this](const decl::Arrays &a) {
                           for (const auto &x : a.arrays)
                               names_[x.id] = x.name;
                       },
                       [this](const decl::Switch &s) { names_[s.id] = s.name; },
                       [this](const decl::Label &l) { names_[l.id] = l.name; },
                       [this](const decl::Procedure &p) {
                           names_[p.id] = p.name;
                           for (const auto &f : p.formals)
                               names_[f.id] = f.name;
                           collect(*p.body);
                       },
                       [](const decl::Common &) {},
                   },
                   d.node);
    }

    // A reference to a declaration: its id and name.
    void ref(Node &n, const char *key, int id)
    {
        n.add(key, scalar(number(id)));
        auto it = names_.find(id);
        n.add("name", quoted(it == names_.end() ? "?" : it->second));
    }

    static Node loc(SrcLoc l) { return scalar(number(l.line) + "/" + number(l.stmt)); }

    Node exprs(const ExprList &list)
    {
        Node n = sequence();
        for (const auto &e : list)
            n.seq.push_back(expr(*e));
        return n;
    }

    Node expr(const Expr &e)
    {
        Node n;
        auto head = [&e](const char *kind, bool flow = false) {
            Node m = mapping(kind, flow);
            m.add("type", scalar(to_string(e.type)));
            return m;
        };
        std::visit(overloaded{
                       [&](const expr::IntConst &c) {
                           n = head("int_const", true);
                           n.add("value", scalar(number(c.value)));
                       },
                       [&](const expr::RealConst &c) {
                           n = head("real_const", true);
                           n.add("value", scalar(real_text(c.value)));
                       },
                       [&](const expr::BoolConst &c) {
                           n = head("bool_const", true);
                           n.add("value", scalar(c.value ? "true" : "false"));
                       },
                       [&](const expr::BitsConst &c) {
                           char buf[32];
                           std::snprintf(buf, sizeof(buf), "0o%llo", (unsigned long long)c.value);
                           n = head("bits_const", true);
                           n.add("value", scalar(buf));
                       },
                       [&](const expr::StringConst &c) {
                           n = head("string_const", true);
                           n.add("value", quoted(c.value));
                       },
                       [&](const expr::Var &v) {
                           n = head("var", true);
                           ref(n, "decl", v.decl);
                       },
                       [&](const expr::Element &x) {
                           n = head("element");
                           ref(n, "array", x.array);
                           n.add("subscripts", exprs(x.subscripts));
                       },
                       [&](const expr::Call &c) {
                           n = head("call");
                           ref(n, "proc", c.proc);
                           n.add("args", exprs(c.args));
                       },
                       [&](const expr::StdFunc &f) {
                           n = head("std_func");
                           n.add("func", scalar(to_string(f.func)));
                           n.add("args", exprs(f.args));
                       },
                       [&](const expr::Unary &u) {
                           n = head("unary");
                           n.add("op", scalar(to_string(u.op)));
                           n.add("operand", expr(*u.operand));
                       },
                       [&](const expr::Binary &b) {
                           n = head("binary");
                           n.add("op", scalar(to_string(b.op)));
                           n.add("left", expr(*b.left));
                           n.add("right", expr(*b.right));
                       },
                       [&](const expr::Cond &c) {
                           n = head("cond");
                           n.add("condition", expr(*c.condition));
                           n.add("then", expr(*c.then_expr));
                           n.add("else", expr(*c.else_expr));
                       },
                       [&](const expr::Convert &c) {
                           n = head("convert");
                           n.add("conv", scalar(to_string(c.conv)));
                           n.add("expr", expr(*c.expr));
                       },
                       [&](const expr::StringElement &s) {
                           n = head("string_element");
                           n.add("string", expr(*s.string));
                           n.add("index", expr(*s.index));
                       },
                       [&](const expr::Slice &s) {
                           n = head("slice");
                           n.add("string", expr(*s.string));
                           n.add("first", expr(*s.first));
                           n.add("last", expr(*s.last));
                       },
                       [&](const expr::LabelRef &l) {
                           n = head("label_ref", true);
                           ref(n, "label", l.label);
                       },
                       [&](const expr::SwitchElement &s) {
                           n = head("switch_element");
                           ref(n, "switch", s.switch_);
                           n.add("index", expr(*s.index));
                       },
                   },
                   e.node);
        return n;
    }

    Node left_part(const LeftPart &lp)
    {
        Node n;
        std::visit(overloaded{
                       [&](const left::Var &v) {
                           n = mapping("var", true);
                           ref(n, "decl", v.decl);
                       },
                       [&](const left::Element &x) {
                           n = mapping("element");
                           ref(n, "array", x.array);
                           n.add("subscripts", exprs(x.subscripts));
                       },
                   },
                   lp);
        return n;
    }

    Node for_elements(const std::vector<ForElement> &list)
    {
        Node seq = sequence();
        for (const auto &fe : list) {
            Node n;
            std::visit(overloaded{
                           [&](const for_elem::Value &v) {
                               n = mapping("value");
                               n.add("value", expr(*v.value));
                           },
                           [&](const for_elem::StepUntil &s) {
                               n = mapping("step_until");
                               n.add("init", expr(*s.init));
                               n.add("step", expr(*s.step));
                               n.add("until", expr(*s.until));
                           },
                           [&](const for_elem::While &w) {
                               n = mapping("while");
                               n.add("value", expr(*w.value));
                               n.add("condition", expr(*w.condition));
                           },
                       },
                       fe);
            seq.seq.push_back(std::move(n));
        }
        return seq;
    }

    Node io_items(const IoList &list)
    {
        Node seq = sequence();
        for (const auto &item : list) {
            Node n;
            std::visit(overloaded{
                           [&](const io::Item &i) {
                               n = mapping("item");
                               n.add("expr", expr(*i.expr));
                           },
                           [&](const io::For &f) {
                               n = mapping("for");
                               n.add("control", left_part(f.control));
                               n.add("elements", for_elements(f.elements));
                               n.add("body", io_items(f.body));
                           },
                           [&](const io::NewLine &) { n = mapping("new_line", true); },
                           [&](const io::NewPage &) { n = mapping("new_page", true); },
                       },
                       item.node);
            seq.seq.push_back(std::move(n));
        }
        return seq;
    }

    Node stmts(const StmtList &list)
    {
        Node n = sequence();
        for (const auto &s : list)
            n.seq.push_back(stmt(*s));
        return n;
    }

    Node stmt(const Stmt &s)
    {
        Node n;
        auto head = [&s](const char *kind, bool flow = false) {
            Node m = mapping(kind, flow);
            m.add("loc", loc(s.loc));
            return m;
        };
        std::visit(overloaded{
                       [&](const stmt::Block &b) {
                           n              = head("block");
                           Node decl_list = sequence();
                           for (const auto &d : b.decls)
                               decl_list.seq.push_back(decl(*d));
                           n.add("decls", std::move(decl_list));
                           n.add("body", stmts(b.body));
                       },
                       [&](const stmt::Compound &c) {
                           n = head("compound");
                           n.add("body", stmts(c.body));
                       },
                       [&](const stmt::Labeled &l) {
                           n = head("labeled");
                           ref(n, "label", l.label);
                           n.add("stmt", stmt(*l.stmt));
                       },
                       [&](const stmt::Dummy &) { n = head("dummy", true); },
                       [&](const stmt::Assign &a) {
                           n            = head("assign");
                           Node targets = sequence();
                           for (const auto &t : a.targets)
                               targets.seq.push_back(left_part(t));
                           n.add("targets", std::move(targets));
                           n.add("value", expr(*a.value));
                       },
                       [&](const stmt::Goto &g) {
                           n = head("goto");
                           n.add("target", expr(*g.target));
                       },
                       [&](const stmt::If &i) {
                           n = head("if");
                           n.add("condition", expr(*i.condition));
                           n.add("then", stmt(*i.then_stmt));
                           if (i.else_stmt)
                               n.add("else", stmt(*i.else_stmt));
                       },
                       [&](const stmt::For &f) {
                           n = head("for");
                           n.add("control", left_part(f.control));
                           n.add("elements", for_elements(f.elements));
                           n.add("body", stmt(*f.body));
                       },
                       [&](const stmt::Call &c) {
                           n = head("call");
                           ref(n, "proc", c.proc);
                           n.add("args", exprs(c.args));
                       },
                       [&](const stmt::StdProc &p) {
                           n = head("std_proc");
                           n.add("proc", scalar(to_string(p.proc)));
                           n.add("args", exprs(p.args));
                       },
                       [&](const stmt::Io &io) {
                           n = head("io");
                           n.add("op", scalar(to_string(io.op)));
                           n.add("params", exprs(io.params));
                           n.add("items", io_items(io.items));
                       },
                   },
                   s.node);
        return n;
    }

    Node decl(const Decl &d)
    {
        Node n;
        auto head = [&d](const char *kind, bool flow = false) {
            Node m = mapping(kind, flow);
            if (d.loc.line)
                m.add("line", scalar(number(d.loc.line)));
            return m;
        };
        std::visit(overloaded{
                       [&](const decl::Var &v) {
                           n = head("var", true);
                           n.add("name", quoted(v.name)).add("id", scalar(number(v.id)));
                           n.add("type", scalar(to_string(v.type)));
                           if (v.own)
                               n.add("own", scalar("true"));
                       },
                       [&](const decl::Arrays &a) {
                           n = head("arrays");
                           n.add("type", scalar(to_string(a.type)));
                           if (a.own)
                               n.add("own", scalar("true"));
                           Node names = sequence();
                           for (const auto &x : a.arrays) {
                               Node m;
                               m.kind = Node::MAP;
                               m.flow = true;
                               m.add("name", quoted(x.name)).add("id", scalar(number(x.id)));
                               names.seq.push_back(std::move(m));
                           }
                           n.add("arrays", std::move(names));
                           Node bounds = sequence();
                           for (const auto &b : a.bounds) {
                               Node m;
                               m.kind = Node::MAP;
                               m.add("lower", expr(*b.lower)).add("upper", expr(*b.upper));
                               bounds.seq.push_back(std::move(m));
                           }
                           n.add("bounds", std::move(bounds));
                       },
                       [&](const decl::Switch &s) {
                           n = head("switch");
                           n.add("name", quoted(s.name)).add("id", scalar(number(s.id)));
                           n.add("elements", exprs(s.elements));
                       },
                       [&](const decl::Label &l) {
                           n = head("label", true);
                           n.add("name", quoted(l.name)).add("id", scalar(number(l.id)));
                       },
                       [&](const decl::Procedure &p) {
                           n = head("procedure");
                           n.add("name", quoted(p.name)).add("id", scalar(number(p.id)));
                           n.add("type", scalar(to_string(p.type)));
                           Node formals = sequence();
                           for (const auto &f : p.formals) {
                               Node m;
                               m.kind = Node::MAP;
                               m.flow = true;
                               m.add("name", quoted(f.name)).add("id", scalar(number(f.id)));
                               m.add("spec", scalar(SPEC_NAMES[f.spec.kind]));
                               if (f.spec.type != Type::NONE)
                                   m.add("type", scalar(to_string(f.spec.type)));
                               m.add("by_value", scalar(f.by_value ? "true" : "false"));
                               formals.seq.push_back(std::move(m));
                           }
                           n.add("formals", std::move(formals));
                           n.add("body", stmt(*p.body));
                       },
                       [&](const decl::Common &c) {
                           n = head("common");
                           n.add("name", quoted(c.name));
                           Node elements = sequence();
                           for (const auto &e : c.elements) {
                               Node m;
                               m.kind = Node::MAP;
                               m.flow = true;
                               m.add("name", quoted(e.name)).add("decl", scalar(number(e.decl)));
                               m.add("size", scalar(number(e.size)));
                               elements.seq.push_back(std::move(m));
                           }
                           n.add("elements", std::move(elements));
                       },
                   },
                   d.node);
        return n;
    }
};

} // namespace

const char *to_string(Type t)
{
    return TYPE_NAMES[int(t)];
}

const char *to_string(StdFunc f)
{
    return STD_FUNC_NAMES[int(f)];
}

const char *to_string(StdProc p)
{
    return STD_PROC_NAMES[int(p)];
}

const char *to_string(IoOp op)
{
    return IO_OP_NAMES[int(op)];
}

const char *to_string(UnaryOp op)
{
    return UNARY_NAMES[int(op)];
}

const char *to_string(BinaryOp op)
{
    return BINARY_NAMES[int(op)];
}

const char *to_string(Conversion c)
{
    return CONVERSION_NAMES[int(c)];
}

void print(std::ostream &out, const Program &program)
{
    Printer p(program);
    emit_entries(out, p.program(program), 0, false);
}

} // namespace ast
