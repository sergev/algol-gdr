//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "ir.h"

#include <cstring>
#include <sstream>

namespace ir {

Value i1(bool b)
{
    return { "i1", b ? "true" : "false" };
}

Value i8(int v)
{
    return { "i8", std::to_string(v) };
}

Value i64(int64_t v)
{
    return { "i64", std::to_string(v) };
}

Value real(double v)
{
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%016llX", (unsigned long long)bits);
    return { "double", buf };
}

Value null()
{
    return { "ptr", "null" };
}

std::string global_name(const std::string &name)
{
    for (char c : name) {
        bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                     c == '_' || c == '.' || c == '$';
        if (!plain) {
            std::string q = "@\"";
            for (unsigned char u : name) {
                if (u < ' ' || u == '"' || u == '\\' || u >= 0x7f) {
                    char buf[4];
                    std::snprintf(buf, sizeof(buf), "\\%02X", u);
                    q += buf;
                } else {
                    q += char(u);
                }
            }
            return q + "\"";
        }
    }
    return "@" + name;
}

//
// Function.
//
Function::Function(std::string name, std::string ret, std::vector<std::string> param_types,
                   std::string attrs, std::string linkage)
    : name_(std::move(name)), ret_(std::move(ret)), attrs_(std::move(attrs)),
      linkage_(std::move(linkage)), params_(std::move(param_types))
{
    set_block(new_block("entry"));
}

Value Function::param(size_t i) const
{
    return { params_.at(i), "%p" + std::to_string(i) };
}

Block *Function::new_block(const std::string &hint)
{
    int n = block_names_[hint]++;
    blocks_.push_back(std::make_unique<Block>(n ? hint + "." + std::to_string(n) : hint));
    return blocks_.back().get();
}

void Function::set_block(Block *b)
{
    cur_ = b;
}

std::string Function::tmp()
{
    return "%t" + std::to_string(next_tmp_++);
}

void Function::emit(const std::string &line)
{
    if (!open())
        set_block(new_block("dead"));
    cur_->lines_.push_back(line);
}

void Function::terminate(const std::string &line)
{
    emit(line);
    cur_->terminated_ = true;
}

void Function::raw(const std::string &line)
{
    emit(line);
}

Value Function::binop(const std::string &op, const Value &a, const Value &b)
{
    std::string t = tmp();
    emit(t + " = " + op + " " + a.str() + ", " + b.ref);
    return { a.type, t };
}

Value Function::icmp(const std::string &cond, const Value &a, const Value &b)
{
    std::string t = tmp();
    emit(t + " = icmp " + cond + " " + a.str() + ", " + b.ref);
    return { "i1", t };
}

Value Function::fcmp(const std::string &cond, const Value &a, const Value &b)
{
    std::string t = tmp();
    emit(t + " = fcmp " + cond + " " + a.str() + ", " + b.ref);
    return { "i1", t };
}

Value Function::cast(const std::string &op, const Value &v, const std::string &to)
{
    std::string t = tmp();
    emit(t + " = " + op + " " + v.str() + " to " + to);
    return { to, t };
}

Value Function::select(const Value &c, const Value &a, const Value &b)
{
    std::string t = tmp();
    emit(t + " = select " + c.str() + ", " + a.str() + ", " + b.str());
    return { a.type, t };
}

Value Function::stack_alloc(const std::string &type, int64_t count, int align,
                            const std::string &label)
{
    // Allocas go to the entry block, so that loops do not grow the stack.
    std::string t    = label.empty() ? tmp() : "%" + label;
    std::string line = t + " = alloca " + type + ", i64 " + std::to_string(count) + ", align " +
                       std::to_string(align);
    auto &entry = blocks_.front()->lines_;
    size_t pos  = 0;
    while (pos < entry.size() && entry[pos].find(" = alloca ") != std::string::npos)
        pos++;
    entry.insert(entry.begin() + pos, line);
    return { "ptr", t };
}

Value Function::load(const std::string &type, const Value &ptr)
{
    std::string t = tmp();
    emit(t + " = load " + type + ", " + ptr.str());
    return { type, t };
}

void Function::store(const Value &v, const Value &ptr)
{
    emit("store " + v.str() + ", " + ptr.str());
}

Value Function::gep(const std::string &elem, const Value &ptr, const Value &index)
{
    std::string t = tmp();
    emit(t + " = getelementptr " + elem + ", " + ptr.str() + ", " + index.str());
    return { "ptr", t };
}

Value Function::call(const std::string &type, const std::string &callee,
                     const std::vector<Value> &args, const std::string &attrs)
{
    std::string list;
    for (const auto &a : args)
        list += (list.empty() ? "" : ", ") + a.str();
    std::string line = "call " + type + " " + callee + "(" + list + ")" + attrs;
    if (type == "void") {
        emit(line);
        return {};
    }
    std::string t = tmp();
    emit(t + " = " + line);
    return { type, t };
}

Value Function::phi(const std::string &type, const std::vector<std::pair<Value, const Block *>> &in)
{
    std::string t    = tmp();
    std::string line = t + " = phi " + type + " ";
    for (size_t i = 0; i < in.size(); i++)
        line += (i ? ", [ " : "[ ") + in[i].first.ref + ", %" + in[i].second->name() + " ]";
    emit(line);
    return { type, t };
}

void Function::br(const Block *to)
{
    if (open())
        terminate("br label %" + to->name());
}

void Function::cond_br(const Value &c, const Block *t, const Block *f)
{
    terminate("br " + c.str() + ", label %" + t->name() + ", label %" + f->name());
}

void Function::switch_(const Value &v, const Block *otherwise,
                       const std::vector<std::pair<int64_t, const Block *>> &cases)
{
    std::string line = "switch " + v.str() + ", label %" + otherwise->name() + " [";
    for (const auto &[k, b] : cases)
        line += " " + v.type + " " + std::to_string(k) + ", label %" + b->name();
    terminate(line + " ]");
}

void Function::ret(const Value &v)
{
    terminate("ret " + v.str());
}

void Function::ret_void()
{
    terminate("ret void");
}

void Function::unreachable()
{
    terminate("unreachable");
}

std::string Function::text() const
{
    std::ostringstream out;
    out << "define " << (linkage_.empty() ? "" : linkage_ + " ") << ret_ << " "
        << global_name(name_) << "(";
    for (size_t i = 0; i < params_.size(); i++)
        out << (i ? ", " : "") << params_[i] << " %p" << i;
    out << ")" << (attrs_.empty() ? "" : " " + attrs_) << " {\n";
    for (const auto &b : blocks_) {
        out << b->name() << ":\n";
        for (const auto &l : b->lines_)
            out << "  " << l << "\n";
        if (!b->terminated_)
            out << "  unreachable\n";
    }
    out << "}\n";
    return out.str();
}

//
// Module.
//
Value Module::string(const std::string &bytes)
{
    auto it = strings_.find(bytes);
    if (it != strings_.end())
        return { "ptr", it->second };
    std::string name = "@.str." + std::to_string(strings_.size());
    std::string chars;
    for (unsigned char c : bytes) {
        if (c < ' ' || c == '"' || c == '\\' || c >= 0x7f) {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "\\%02X", c);
            chars += buf;
        } else {
            chars += char(c);
        }
    }
    globals_.push_back(name + " = private unnamed_addr constant [" +
                       std::to_string(bytes.size() + 1) + " x i8] c\"" + chars + "\\00\"");
    strings_[bytes] = name;
    return { "ptr", name };
}

void Module::declare(const std::string &ret, const std::string &name, const std::string &params,
                     const std::string &attrs)
{
    declares_.emplace(name, "declare " + ret + " " + global_name(name) + "(" + params + ")" +
                                (attrs.empty() ? "" : " " + attrs));
}

void Module::global(const std::string &line)
{
    globals_.push_back(line);
}

Function &Module::function(const std::string &name, const std::string &ret,
                           const std::vector<std::string> &param_types, const std::string &attrs,
                           const std::string &linkage)
{
    functions_.push_back(std::make_unique<Function>(name, ret, param_types, attrs, linkage));
    return *functions_.back();
}

std::string Module::text() const
{
    std::ostringstream out;
    out << "; ModuleID = '" << name_ << "'\n";
    out << "source_filename = \"" << name_ << "\"\n";
    if (!triple_.empty())
        out << "target triple = \"" << triple_ << "\"\n";
    out << "\n";
    for (const auto &g : globals_)
        out << g << "\n";
    if (!globals_.empty())
        out << "\n";
    for (const auto &f : functions_)
        out << f->text() << "\n";
    for (const auto &[name, d] : declares_)
        out << d << "\n";
    return out.str();
}

} // namespace ir
