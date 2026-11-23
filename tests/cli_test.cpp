//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "util.h"

static const std::string compiler = BUILD_DIR "/src/algc";

TEST(cli, usage)
{
    auto r = run_command(compiler + " --help");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.output.find("Usage:"), std::string::npos);
    EXPECT_NE(r.output.find("--full"), std::string::npos);
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

// Without --list only the errors and the summary line print, as with *NO LIST.
TEST(cli, list)
{
    const std::string out = " -o " BUILD_DIR "/cli_list.std " TEST_DIR "/algol/hello.alg";
    auto r                = run_command(compiler + out);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.output.find("A L G O L"), std::string::npos);
    EXPECT_EQ(r.output.rfind("PROGRAM   ДЛИHA:", 0), 0u) << r.output;

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

TEST(cli, compile)
{
    auto r = run_command(compiler + " -o /dev/null " TEST_DIR "/algol/hello.alg");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.output.find("PROGRAM   ДЛИHA:   16 00020B"), std::string::npos);
}

// --full: the listing, then the object listing.
TEST(cli, full)
{
    auto r = run_command(compiler + " --full -o /dev/null " TEST_DIR "/algol/hello.alg");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.output.rfind("A L G O L", 0), 0u) << r.output;
    EXPECT_NE(r.output.find("\n\n 40000   7 VTM 40005\n"), std::string::npos) << r.output;
    EXPECT_NE(r.output.find("        HELLO,\n"), std::string::npos) << r.output;
}

TEST(cli, dump)
{
    auto r = run_command(compiler + " --dump " TEST_DIR "/algol/hello.std");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.output.find("PROGRAM"), std::string::npos);
    EXPECT_NE(r.output.find("13 VJM  74001       ; A/START"), std::string::npos);
    EXPECT_NE(r.output.find(" 7 VTM  40005       ; @5"), std::string::npos);
    EXPECT_NE(r.output.find("A/STOP"), std::string::npos);
}

TEST(cli, dump_bad_file)
{
    auto r = run_command(compiler + " --dump " TEST_DIR "/algol/hello.alg");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("expected '`'"), std::string::npos);
}

TEST(cli, bad_stop_after)
{
    auto r = run_command(compiler + " --stop-after=6 " TEST_DIR "/algol/hello.alg");
    EXPECT_NE(r.exit_code, 0);
    EXPECT_NE(r.output.find("--stop-after"), std::string::npos);
}

// The object of FOO.alg goes to FOO.o by default.
TEST(cli, default_output)
{
    const std::string base = BUILD_DIR "/cli_default";
    std::filesystem::copy_file(TEST_DIR "/algol/hello.alg", base + ".alg",
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(base + ".o");
    auto r = run_command(compiler + " " + base + ".alg");
    EXPECT_EQ(r.exit_code, 0) << r.output;
    EXPECT_EQ(file_contents(base + ".o"), file_contents(TEST_DIR "/algol/hello.std"));
}
