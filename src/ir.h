//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_IR_H
#define ALGOL_IR_H

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

//
// A small builder of LLVM IR in text, after the shape of llir/llvm (Go):
// a module has globals, declarations and functions; a function has basic
// blocks; a block has instructions and a terminator. Values are typed
// operands: "i64 %t3", "double 1.5", "ptr @.str.0".
//
namespace ir {

struct Value {
    std::string type; // i1, i8, i64, double, ptr, void
    std::string ref;  // %t1, @name, 42, 0x3FF8000000000000, null

    std::string str() const { return type + " " + ref; }
    bool valid() const { return !type.empty(); }
};

Value i1(bool b);
Value i8(int v);
Value i64(int64_t v);
Value real(double v);
Value null();

class Function;

class Block {
public:
    explicit Block(std::string name) : name_(std::move(name)) {}

    const std::string &name() const { return name_; }
    bool terminated() const { return terminated_; }

private:
    friend class Function;
    std::string name_;
    std::vector<std::string> lines_;
    bool terminated_{};
};

class Function {
public:
    Function(std::string name, std::string ret, std::vector<std::string> param_types,
             std::string attrs = "");

    const std::string &name() const { return name_; }
    const std::string &ret_type() const { return ret_; }
    Value param(size_t i) const;

    // A new block, not yet current; its name made unique from hint.
    Block *new_block(const std::string &hint);
    void set_block(Block *b);
    Block *block() const { return cur_; }
    // The current block can take instructions: not terminated.
    bool open() const { return cur_ && !cur_->terminated(); }

    // Instructions in the current block; after a terminator, a fresh
    // unreachable block takes them.
    Value binop(const std::string &op, const Value &a, const Value &b);
    Value icmp(const std::string &cond, const Value &a, const Value &b);
    Value fcmp(const std::string &cond, const Value &a, const Value &b);
    Value cast(const std::string &op, const Value &v, const std::string &to);
    Value select(const Value &c, const Value &a, const Value &b);
    Value stack_alloc(const std::string &type, int64_t count = 1, int align = 8);
    Value load(const std::string &type, const Value &ptr);
    void store(const Value &v, const Value &ptr);
    Value gep(const std::string &elem, const Value &ptr, const Value &index);
    Value call(const std::string &type, const std::string &callee, const std::vector<Value> &args,
               const std::string &attrs = "");
    Value phi(const std::string &type, const std::vector<std::pair<Value, const Block *>> &in);
    void raw(const std::string &line); // a line as is

    void br(const Block *to);
    void cond_br(const Value &c, const Block *t, const Block *f);
    void switch_(const Value &v, const Block *otherwise,
                 const std::vector<std::pair<int64_t, const Block *>> &cases);
    void ret(const Value &v);
    void ret_void();
    void unreachable();

    std::string text() const;

private:
    std::string name_, ret_, attrs_;
    std::vector<std::string> params_;
    std::vector<std::unique_ptr<Block>> blocks_;
    std::map<std::string, int> block_names_;
    Block *cur_{};
    int next_tmp_{};

    std::string tmp();
    void emit(const std::string &line);
    void terminate(const std::string &line);
};

class Module {
public:
    explicit Module(std::string name) : name_(std::move(name)) {}

    void set_target(const std::string &triple) { triple_ = triple; }

    // A NUL-terminated constant; a ptr to it. Equal strings share one.
    Value string(const std::string &bytes);

    // declare RET @NAME(PARAMS) [ATTRS], once per name.
    void declare(const std::string &ret, const std::string &name, const std::string &params,
                 const std::string &attrs = "");

    // A global definition line, as is: "@g = internal global i64 0".
    void global(const std::string &line);

    Function &function(const std::string &name, const std::string &ret,
                       const std::vector<std::string> &param_types, const std::string &attrs = "");

    std::string text() const;

private:
    std::string name_, triple_;
    std::map<std::string, std::string> strings_; // bytes → @.str.N
    std::vector<std::string> globals_;
    std::map<std::string, std::string> declares_;
    std::vector<std::unique_ptr<Function>> functions_;
};

// "name" as an LLVM identifier: quoted when it has other characters.
std::string global_name(const std::string &name);

} // namespace ir

#endif
