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

TEST(cli, two_files)
{
    auto r = run_command(compiler + " a.alg b.alg");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("too many input files"), std::string::npos);
}

// Without --list only the errors print, as with *NO LIST.
TEST(cli, list)
{
    const std::string out = " " TEST_DIR "/algol/hello.alg";
    auto r                = run_command(compiler + out);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.output.find("A L G O L"), std::string::npos) << r.output;

    r = run_command(compiler + " --list" + out);
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
    std::ofstream(path) << "'BEGIN'\n'END'\xff\n'EOP'\n";
    auto r = run_command(compiler + " " + path);
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find(":2: invalid UTF-8"), std::string::npos);
}

TEST(cli, errors)
{
    auto r = run_command(compiler + " " TEST_DIR "/algol/err_type.alg");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("TYPE ERROR"), std::string::npos) << r.output;
}

TEST(cli, stop_after)
{
    auto r = run_command(compiler + " --stop-after=2 " TEST_DIR "/algol/hello.alg");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.output, file_contents(TEST_DIR "/algol/hello.s2"));
}

TEST(cli, bad_stop_after)
{
    auto r = run_command(compiler + " --stop-after=3 " TEST_DIR "/algol/hello.alg");
    EXPECT_NE(r.exit_code, 0);
    EXPECT_NE(r.output.find("--stop-after"), std::string::npos);
}

TEST(cli, dump_ast)
{
    auto r = run_command("(" + compiler + " --dump-ast " TEST_DIR "/algol/proc.alg 2>/dev/null)");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.output, file_contents(TEST_DIR "/algol/proc.ast"));
}
