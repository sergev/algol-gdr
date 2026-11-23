//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Test programs compiled natively and run: the output must be NAME.out from
// dubna, or NAME.native.out where the native run differs for a reason (the
// original prints upper case only, for one).
//
#include <gtest/gtest.h>

#include <filesystem>

#include "util.h"

namespace {

const std::string compiler = BUILD_DIR "/src/algc";

// The programs the code generator handles so far.
const std::vector<std::string> programs = {
    "hello",
    "hello_algol",
};

std::string expected(const std::string &name)
{
    std::string native = TEST_DIR "/algol/" + name + ".native.out";
    if (std::filesystem::exists(native))
        return file_contents(native);
    return file_contents(TEST_DIR "/algol/" + name + ".out");
}

class RunTest : public ::testing::TestWithParam<std::string> {};

} // namespace

TEST_P(RunTest, output)
{
    const std::string name = GetParam();
    const std::string src  = TEST_DIR "/algol/" + name + ".alg";
    const std::string exe  = BUILD_DIR "/run_" + name;
    std::string options    = pre_options(TEST_DIR "/algol/" + name + ".pre");
    bool check             = options.find("--check") != std::string::npos;
    auto r = run_command(compiler + (check ? " --check" : "") + " -o " + exe + " " + src);
    ASSERT_EQ(r.exit_code, 0) << r.output;
    std::string input = TEST_DIR "/algol/" + name + ".in";
    if (!std::filesystem::exists(input))
        input = "/dev/null";
    r = run_command("(" + exe + " < " + input + " 2>/dev/null)");
    EXPECT_EQ(r.output, expected(name));
}

INSTANTIATE_TEST_SUITE_P(algol, RunTest, ::testing::ValuesIn(programs));
