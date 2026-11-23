//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Test programs compiled natively and run. The output is compared with
// NAME.out from dubna by tokens: split on white space, the original's reals
// d.ddd±ee read as d.ddde±ee, TRUE and FALSE in lower case, and reals equal
// to the last digit or so (BESM-6 rounds otherwise). NAME.native.out, in
// the native layout, is the reference where the output really differs.
//
#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <regex>
#include <sstream>

#include "util.h"

namespace {

const std::string compiler = BUILD_DIR "/src/algc";

// The programs the code generator handles so far.
const std::vector<std::string> programs = {
    "arith", "blocks",      "boolean", "comment", "empty",
    "hello", "hello_algol", "pages",   "print",   "symbols",
};

std::vector<std::string> tokens(const std::string &text)
{
    static const std::regex real(R"(([-+]?\d\.\d+)([-+]\d+))");
    std::istringstream in(text);
    std::vector<std::string> list;
    std::string t;
    while (in >> t) {
        if (t == "TRUE" || t == "FALSE")
            t = t == "TRUE" ? "true" : "false";
        else
            t = std::regex_replace(t, real, "$1e$2");
        list.push_back(t);
    }
    return list;
}

// Equal, or both reals equal to 1e-10 relative.
bool same(const std::string &a, const std::string &b)
{
    if (a == b)
        return true;
    if (a.find('.') == std::string::npos || b.find('.') == std::string::npos)
        return false;
    char *ea, *eb;
    double x = std::strtod(a.c_str(), &ea);
    double y = std::strtod(b.c_str(), &eb);
    if (*ea || *eb)
        return false;
    return std::fabs(x - y) <= 1e-10 * std::max(std::fabs(x), std::fabs(y));
}

// Empty when the outputs match, otherwise where they differ.
std::string compare(const std::string &output, const std::string &expected)
{
    auto got = tokens(output), want = tokens(expected);
    for (size_t i = 0; i < std::max(got.size(), want.size()); i++) {
        std::string g = i < got.size() ? got[i] : "(end)";
        std::string w = i < want.size() ? want[i] : "(end)";
        if (!same(g, w))
            return "token " + std::to_string(i + 1) + ": got '" + g + "', expected '" + w + "'";
    }
    return "";
}

std::string expected(const std::string &name)
{
    std::string native = TEST_DIR "/algol/" + name + ".native.out";
    if (std::filesystem::exists(native))
        return file_contents(native);
    return file_contents(TEST_DIR "/algol/" + name + ".out");
}

// Compile and run a program; its output.
std::string run(const std::string &src, const std::string &exe, bool check,
                const std::string &input = "/dev/null")
{
    auto r = run_command(compiler + (check ? " --check" : "") + " -o " + exe + " " + src);
    EXPECT_EQ(r.exit_code, 0) << r.output;
    return run_command("(" + exe + " < " + input + " 2>/dev/null)").output;
}

class RunTest : public ::testing::TestWithParam<std::string> {};

} // namespace

TEST_P(RunTest, output)
{
    const std::string name = GetParam();
    const std::string base = TEST_DIR "/algol/" + name;
    bool check             = pre_options(base + ".pre").find("--check") != std::string::npos;
    std::string input      = std::filesystem::exists(base + ".in") ? base + ".in" : "/dev/null";
    std::string out        = run(base + ".alg", BUILD_DIR "/run_" + name, check, input);
    EXPECT_EQ(compare(out, expected(name)), "") << out;
}

INSTANTIATE_TEST_SUITE_P(algol, RunTest, ::testing::ValuesIn(programs));

// The exact layout of PRINT.
TEST(Run, print_layout)
{
    EXPECT_EQ(run(TEST_DIR "/algol/print.alg", BUILD_DIR "/run_layout", false),
              "             0              1             -1      123456789 "
              " 0.00000000000e+00    1.00000000000e+00   -1.00000000000e+00   "
              " 3.14159260000e+00    1.00000000000e+18    1.00000000000e-18   "
              "true  false STRING ONESTRING TWO\n"
              "AFTER NEWLINE             1 \n"
              "             2 \fLAST\n");
}

TEST(Run, compare)
{
    EXPECT_EQ(compare(" 1.00000000001+00  TRUE\n", "1.00000000000e+00 true"), "");
    EXPECT_NE(compare("1.0001e+00", "1.0e+00"), "");
    EXPECT_NE(compare("12", "13"), "");
    EXPECT_NE(compare("1 2", "1"), "");
}
