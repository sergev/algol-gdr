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
#include <fstream>
#include <regex>
#include <sstream>

#include "util.h"

namespace {

const std::string compiler = BUILD_DIR "/src/algc";

// The programs the code generator handles so far.
// clang-format off
const std::vector<std::string> programs = {
    "arith",
    "arrays",
    "arrays_check",
    "blocks",
    "boolean",
    "comment",
    "cond",
    "empty",
    "for",
    "hello",
    "hello_algol",
    "man_or_boy",
    "mixed",
    "pages",
    "print",
    "proc",
    "stdfunc",
    "symbols",
    "test1",
    "test2",
};
// clang-format on

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

// The output of a program given as text.
std::string run_text(const std::string &name, const std::string &text, bool check = false)
{
    const std::string src = BUILD_DIR "/run_" + name + ".alg";
    std::ofstream(src) << text;
    return run(src, BUILD_DIR "/run_" + name, check);
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

TEST(Run, control)
{
    EXPECT_EQ(run_text("control", R"(_begin _integer I, S;
  I := 0; S := 0;
L: I := I + 1;
  _if I _le 10 _then _begin S := S + I; _goto L _end;
  _if S = 55 _then PRINT(''OK'') _else PRINT(''BAD'');
  _if S > 100 _then PRINT(''BAD'');
  _begin _integer J; J := I; _if J = 11 _then _goto M; PRINT(''BAD'') _end;
M: PRINT(S, I)
_end _eop
)"),
              "OK            55             11 \n");
}

TEST(Run, for_lists)
{
    EXPECT_EQ(run_text("for_lists", R"(_begin _integer I; _real X;
  _for I := 1, 2 _step 2 _until 6, 10 _do PRINT(I);
  X := 0;
  _for X := X + 0.5 _while X < 2 _do PRINT(X);
  PRINT(NEWLINE, I)
_end _eop
)"),
              "             1              2              4              6             10 "
              " 5.00000000000e-01    1.00000000000e+00    1.50000000000e+00   \n"
              "            10 \n");
}

TEST(Run, domain_error)
{
    const std::string src = BUILD_DIR "/run_sqrt.alg";
    std::ofstream(src) << "_begin PRINT(1); PRINT(SQRT(-1.0)) _end _eop\n";
    auto r =
        run_command(compiler + " -o " BUILD_DIR "/run_sqrt " + src + " && " BUILD_DIR "/run_sqrt");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_EQ(r.output, "             1 \nError: Square root of a negative number -1\n");
}

TEST(Run, arrays)
{
    EXPECT_EQ(run_text("arrays2", R"(_begin _integer N, I, J;
  N := 2;
  _begin _integer _array A[0:N, -1:1];
    _for I := 0 _step 1 _until N _do
      _for J := -1 _step 1 _until 1 _do A[I, J] := 10 * I + J;
    PRINT(A[2, -1], A[1, 1]);
    PRINT(NEWLINE, A)
  _end
_end _eop
)"),
              "            19             11 \n"
              "            -1              0              1              9             10 "
              "            11             19             20             21 \n");
}

TEST(Run, bounds_check)
{
    const std::string src = BUILD_DIR "/run_bounds.alg";
    std::ofstream(src) << "_begin _array A[1:3]; _integer I; I := 4; A[I] := 1 _end _eop\n";
    auto r = run_command(compiler + " --check -o " BUILD_DIR "/run_bounds " + src +
                         " && " BUILD_DIR "/run_bounds");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_EQ(r.output, "Error: Subscript 4 out of bounds [1:3]\n");
}

TEST(Run, procedures)
{
    EXPECT_EQ(run_text("procedures", R"(_begin _integer _array A[1:3]; _integer I;
  _integer _procedure FACT(N); _value N; _integer N;
    FACT := _if N _le 1 _then 1 _else N * FACT(N - 1);
  _procedure ZAP(B); _integer _array B; B[1] := 99;
  _real _procedure HALF(X); _value X; _real X; HALF := X / 2;
  _for I := 1 _step 1 _until 3 _do A[I] := I;
  ZAP(A);
  PRINT(A[1], FACT(10), HALF(3))
_end _eop
)"),
              "            99        3628800  1.50000000000e+00   \n");
}

// Jensen's device, assignment through a name parameter, a procedure and
// strings as parameters.
TEST(Run, by_name)
{
    EXPECT_EQ(run_text("by_name", R"(_begin _integer I; _real S; _real _array A[1:4]; _string T;
  _real _procedure SUM(K, LO, HI, TERM); _value LO, HI; _integer K, LO, HI; _real TERM;
  _begin _real R; R := 0;
    _for K := LO _step 1 _until HI _do R := R + TERM;
    SUM := R
  _end;
  _procedure INC(X); _integer X; X := X + 1;
  _real _procedure TWICE(F, X); _real _procedure F; _real X; TWICE := F(F(X));
  _real _procedure SQ(Y); _value Y; _real Y; SQ := Y * Y;
  _procedure SAY(W); _string W; PRINT(W);
  _for I := 1 _step 1 _until 4 _do A[I] := I;
  S := SUM(I, 1, 4, A[I] * A[I]);
  PRINT(S);
  I := 5; INC(I); PRINT(I);
  PRINT(TWICE(SQ, 3));
  T := ''HI''; SAY(T); SAY(''THERE'')
_end _eop
)"),
              " 3.00000000000e+01                6  8.10000000000e+01   HITHERE\n");
}

// juggle needs its _code procedures to run, but it compiles.
TEST(Run, juggle_compiles)
{
    auto r = run_command(compiler + " -c -o " BUILD_DIR "/juggle.o " TEST_DIR "/algol/juggle.alg");
    EXPECT_EQ(r.exit_code, 0) << r.output;
}
