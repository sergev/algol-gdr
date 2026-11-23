//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The IR builder: functions, blocks, constants.
//
#include "ir.h"

#include <gtest/gtest.h>

TEST(Ir, function)
{
    ir::Module m("x.alg");
    m.declare("void", "f", "ptr");
    auto &fn   = m.function("g", "i64", { "i64" }, "", "internal");
    auto *then = fn.new_block("then");
    auto *done = fn.new_block("then");
    auto c     = fn.icmp("slt", fn.param(0), ir::i64(0));
    fn.cond_br(c, then, done);
    fn.set_block(then);
    fn.call("void", "@f", { m.string("a\"b\n") });
    fn.br(done);
    fn.set_block(done);
    fn.ret(fn.binop("add", fn.param(0), ir::i64(1)));
    EXPECT_EQ(m.text(),
              "; ModuleID = 'x.alg'\n"
              "source_filename = \"x.alg\"\n"
              "\n"
              "@.str.0 = private unnamed_addr constant [5 x i8] c\"a\\22b\\0A\\00\"\n"
              "\n"
              "define internal i64 @g(i64 %p0) {\n"
              "entry:\n"
              "  %t0 = icmp slt i64 %p0, 0\n"
              "  br i1 %t0, label %then, label %then.1\n"
              "then:\n"
              "  call void @f(ptr @.str.0)\n"
              "  br label %then.1\n"
              "then.1:\n"
              "  %t1 = add i64 %p0, 1\n"
              "  ret i64 %t1\n"
              "}\n"
              "\n"
              "declare void @f(ptr)\n");
}

TEST(Ir, constants)
{
    EXPECT_EQ(ir::real(1.5).str(), "double 0x3FF8000000000000");
    EXPECT_EQ(ir::i1(true).str(), "i1 true");
    EXPECT_EQ(ir::global_name("a.b"), "@a.b");
    EXPECT_EQ(ir::global_name("счёт"), "@\"\\D1\\81\\D1\\87\\D1\\91\\D1\\82\"");
}
