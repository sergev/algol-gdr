#include <gtest/gtest.h>

#include <fstream>

#include "util.h"

static const std::string compiler = BUILD_DIR "/src/algol-gdr";

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

TEST(cli, full_and_no_list)
{
    auto r = run_command(compiler + " --full --no-list " TEST_DIR "/algol/hello.alg");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("mutually exclusive"), std::string::npos);
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

TEST(cli, not_implemented)
{
    auto r = run_command(compiler + " -o /dev/null " TEST_DIR "/algol/hello.alg");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.output.find("not implemented"), std::string::npos);
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
