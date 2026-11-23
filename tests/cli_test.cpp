//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include <gtest/gtest.h>

#include <fstream>

#include "util.h"

static const std::string compiler = BUILD_DIR "/src/algc";

TEST(cli, usage)
{
    auto r = run_command(compiler + " --help");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.output.find("Usage:"), std::string::npos);
    EXPECT_NE(r.output.find("--list"), std::string::npos);
}

TEST(cli, no_arguments)
{
    auto r = run_command(compiler);
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("Usage:"), std::string::npos);
}

TEST(cli, version)
{
    auto r = run_command(compiler + " --version");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.output.find("Version 0."), std::string::npos);
}

TEST(cli, bad_option)
{
    auto r = run_command(compiler + " --bogus x.alg");
    EXPECT_EQ(r.exit_code, 1);
}

TEST(cli, unknown_input)
{
    auto r = run_command(compiler + " a.txt");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("a.txt: unknown kind of file"), std::string::npos);
}

TEST(cli, bad_level)
{
    auto r = run_command(compiler + " -O7 x.alg");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("bad optimization level"), std::string::npos);
}

TEST(cli, output_of_several)
{
    auto r = run_command(compiler + " -c -o x.o a.alg b.alg");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("-o with several inputs"), std::string::npos);
}

// --emit-llvm writes IR; -S and -c take it on to clang.
TEST(cli, outputs)
{
    const std::string base = BUILD_DIR "/cli_hello";
    const std::string src  = " " TEST_DIR "/algol/hello.alg";
    auto r                 = run_command(compiler + " --emit-llvm -o " + base + ".ll" + src);
    ASSERT_EQ(r.exit_code, 0) << r.output;
    EXPECT_NE(file_contents(base + ".ll").find("define"), std::string::npos);

    r = run_command(compiler + " -S -o " + base + ".s " + base + ".ll");
    ASSERT_EQ(r.exit_code, 0) << r.output;
    EXPECT_NE(file_contents(base + ".s").find("algol_main"), std::string::npos);

    r = run_command(compiler + " -c -o " + base + ".o" + src);
    ASSERT_EQ(r.exit_code, 0) << r.output;
    EXPECT_FALSE(file_contents(base + ".o").empty());

    r = run_command(compiler + " -v -c -o " + base + ".o " + base + ".s");
    ASSERT_EQ(r.exit_code, 0) << r.output;
    EXPECT_NE(r.output.find("running"), std::string::npos);
}

// An executable, linked with libalgol.a.
TEST(cli, executable)
{
    const std::string exe = BUILD_DIR "/cli_empty";
    auto r = run_command(compiler + " -o " + exe + " " TEST_DIR "/algol/empty.alg && " + exe);
    EXPECT_EQ(r.exit_code, 0) << r.output;
    EXPECT_EQ(r.output, "");
}

// The intermediate .ll stays with --save-temps.
TEST(cli, save_temps)
{
    const std::string dir = BUILD_DIR "/cli_temps";
    auto r = run_command("rm -rf " + dir + " && mkdir " + dir + " && cd " + dir + " && " +
                         compiler + " --save-temps -c " TEST_DIR "/algol/hello.alg && ls");
    ASSERT_EQ(r.exit_code, 0) << r.output;
    EXPECT_NE(r.output.find("hello.ll"), std::string::npos);
    EXPECT_NE(r.output.find("hello.o"), std::string::npos);

    r = run_command("rm -rf " + dir + " && mkdir " + dir + " && cd " + dir + " && " + compiler +
                    " -c " TEST_DIR "/algol/hello.alg && ls");
    ASSERT_EQ(r.exit_code, 0) << r.output;
    EXPECT_EQ(r.output, "hello.o\n");
}

// Without --list only the errors print, as with *NO LIST.
TEST(cli, list)
{
    const std::string out = " " TEST_DIR "/algol/hello.alg";
    auto r                = run_command(compiler + " --syntax-only" + out);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.output.find("A L G O L"), std::string::npos) << r.output;

    r = run_command(compiler + " --syntax-only --list" + out);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.output.rfind("A L G O L", 0), 0u) << r.output;
}

TEST(cli, missing_file)
{
    auto r = run_command(compiler + " /nonexistent/file.alg");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("/nonexistent/file.alg: No such file"), std::string::npos);
}

TEST(cli, invalid_utf8)
{
    const std::string path = BUILD_DIR "/invalid_utf8.alg";
    std::ofstream(path) << "_begin\n_end\xff\n_eop\n";
    auto r = run_command(compiler + " " + path);
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find(":2: invalid UTF-8"), std::string::npos);
}

TEST(cli, errors)
{
    auto r = run_command(compiler + " --syntax-only " TEST_DIR "/algol/err_type.alg");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("err_type.alg:6: error: Type error"), std::string::npos) << r.output;
}

TEST(cli, dump_ast)
{
    auto r = run_command("(" + compiler + " --dump-ast " TEST_DIR "/algol/proc.alg 2>/dev/null)");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.output, file_contents(TEST_DIR "/algol/proc.ast"));
}
