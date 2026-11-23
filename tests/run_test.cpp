//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Test programs compiled natively and run. The output is compared with
// NAME.out from dubna by tokens: split on white space, the original's reals
// d.ddd±ee read as d.ddde±ee, TRUE and FALSE in lower case, and reals equal
// to the last digit or so (BESM-6 rounds otherwise). NAME.native.out, in
// the native layout, is the reference where the output really differs.
// NAME.c holds C for the _code procedures of a program, NAME.unit.alg the
// _algol ones; NAME.in its input.
//
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#include "util.h"

namespace {

const std::string compiler = BUILD_DIR "/src/algc";

// The programs, as DIR/NAME under tests/.
// clang-format off
const std::vector<std::string> programs = {
    "algol/arith",
    "algol/arrays",
    "algol/arrays_check",
    "algol/blocks",
    "algol/boolean",
    "algol/bozp_ybyb",
    "algol/comment",
    "algol/common",
    "algol/cond",
    "algol/empty",
    "algol/exprs",
    "algol/exprs2",
    "algol/for",
    "algol/for2",
    "algol/for2_check",
    "algol/goto",
    "algol/hello",
    "algol/hello_algol",
    "algol/man_or_boy",
    "algol/mixed",
    "algol/pages",
    "algol/print",
    "algol/proc",
    "algol/quine",
    "algol/quinio",
    "algol/stdfunc",
    "algol/strings",
    "algol/switch",
    "algol/symbols",
    "algol/test1",
    "algol/test2",
    "algol/test3",
    "a60/for",
    "a60/goto_arrays",
    "a60/ifip",
    "a60/intdiv",
    "a60/own",
    "a60/parameters",
    "a60/publication",
    "a60/units",
    "a60/program_form",
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
    std::string native = TEST_DIR "/" + name + ".native.out";
    if (std::filesystem::exists(native))
        return file_contents(native);
    return file_contents(TEST_DIR "/" + name + ".out");
}

// C for the _code procedures of a program, or a unit of _algol ones.
std::string stub_of(const std::string &base)
{
    for (const char *ext : { ".c", ".unit.alg" })
        if (std::filesystem::exists(base + ext))
            return base + ext;
    return "";
}

// The program: NAME.alg, or NAME.a60.
std::string source_of(const std::string &base)
{
    return std::filesystem::exists(base + ".a60") ? base + ".a60" : base + ".alg";
}

// qemu-ARCH for a target triple.
std::string qemu(const std::string &target)
{
    return "qemu-" + target.substr(0, target.find('-'));
}

// Compile and run a program, with the object of stub (C) if given; its
// output.
std::string run(const std::string &src, const std::string &exe, bool check,
                const std::string &input = "/dev/null", const std::string &stub = "",
                const std::string &target = "")
{
    std::string tflag = target.empty() ? "" : " --target=" + target;
    std::string objects;
    if (!stub.empty()) {
        std::string cc =
            stub.size() > 4 && stub.substr(stub.size() - 4) == ".alg" ? compiler : "clang";
        auto c = run_command(cc + tflag + " -c -o " + exe + "_stub.o " + stub);
        EXPECT_EQ(c.exit_code, 0) << c.output;
        objects = " " + exe + "_stub.o";
    }
    auto r = run_command(compiler + tflag + (check ? " --check" : "") + " -o " + exe + " " + src +
                         objects);
    EXPECT_EQ(r.exit_code, 0) << r.output;
    std::string runner;
    if (!target.empty())
        runner = qemu(target) + " -L /usr/" + target + " ";
    return run_command("(" + runner + exe + " < " + input + " 2>/dev/null)").output;
}

// The output of a program given as text.
std::string run_text(const std::string &name, const std::string &text, bool check = false)
{
    const std::string src = BUILD_DIR "/run_" + name + ".alg";
    std::ofstream(src) << text;
    return run(src, BUILD_DIR "/run_" + name, check);
}

// algol_arith for algol/arith.
std::string test_name(const std::string &path)
{
    std::string s = path;
    std::replace(s.begin(), s.end(), '/', '_');
    return s;
}

class RunTest : public ::testing::TestWithParam<std::string> {};

} // namespace

TEST_P(RunTest, output)
{
    const std::string name = GetParam();
    const std::string base = TEST_DIR "/" + name;
    bool check             = pre_options(base + ".pre").find("--check") != std::string::npos;
    std::string input      = std::filesystem::exists(base + ".in") ? base + ".in" : "/dev/null";
    std::string stub       = stub_of(base);
    std::string out = run(source_of(base), BUILD_DIR "/run_" + test_name(name), check, input, stub);
    EXPECT_EQ(compare(out, expected(name)), "") << out;
}

INSTANTIATE_TEST_SUITE_P(algol, RunTest, ::testing::ValuesIn(programs),
                         [](const ::testing::TestParamInfo<std::string> &p) {
                             return test_name(p.param);
                         });

// The same for other targets, run under qemu; skipped without the runtime for
// the target (built when /usr/TRIPLE exists) or without qemu.
class CrossTest : public ::testing::TestWithParam<std::tuple<std::string, std::string>> {};

TEST_P(CrossTest, output)
{
    const auto &[target, name] = GetParam();
    if (!std::filesystem::exists(BUILD_DIR "/runtime/" + target + "/libalgol.a"))
        GTEST_SKIP() << "no runtime for " << target;
    if (run_command("command -v " + qemu(target)).exit_code != 0)
        GTEST_SKIP() << "no " << qemu(target);
    const std::string base = TEST_DIR "/" + name;
    bool check             = pre_options(base + ".pre").find("--check") != std::string::npos;
    std::string input      = std::filesystem::exists(base + ".in") ? base + ".in" : "/dev/null";
    std::string stub       = stub_of(base);
    std::string out = run(source_of(base), BUILD_DIR "/run_" + target + "_" + test_name(name),
                          check, input, stub, target);
    EXPECT_EQ(compare(out, expected(name)), "") << out;
}

// aarch64_hello and the like.
std::string cross_name(const ::testing::TestParamInfo<std::tuple<std::string, std::string>> &p)
{
    const std::string &triple = std::get<0>(p.param);
    return triple.substr(0, triple.find('-')) + "_" + test_name(std::get<1>(p.param));
}

INSTANTIATE_TEST_SUITE_P(algol, CrossTest,
                         ::testing::Combine(::testing::Values(std::string("aarch64-linux-gnu"),
                                                              std::string("riscv64-linux-gnu")),
                                            ::testing::ValuesIn(programs)),
                         cross_name);

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

// Non-local gotos free the arrays they leave: 32 GB otherwise.
TEST(Run, goto_frees_arrays)
{
    const std::string exe = BUILD_DIR "/run_goto_arrays_limited";
    auto c = run_command(compiler + " -o " + exe + " " TEST_DIR "/a60/goto_arrays.alg");
    ASSERT_EQ(c.exit_code, 0) << c.output;
    auto r = run_command("ulimit -v 1000000; " + exe);
    EXPECT_EQ(r.exit_code, 0) << r.output;
}

// fault stops the program with a message; stop without one.
TEST(Run, fault_and_stop)
{
    const std::string src = BUILD_DIR "/run_fault.alg";
    std::ofstream(src) << "_begin outinteger(1, 5); fault(''bad value'', 2.5) _end\n";
    auto r = run_command(compiler + " -o " BUILD_DIR "/run_fault " + src +
                         " && " BUILD_DIR "/run_fault");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_EQ(r.output, "5 \nError: bad value 2.5\n");
    EXPECT_EQ(run_text("stop", "_begin PRINT(1); STOP; PRINT(2) _end\n"), "             1 \n");
}

// Units, formats, _encode and _decode: unit 7 a file of its own.
TEST(Run, gdr_io)
{
    const std::string exe = BUILD_DIR "/run_gdr_io";
    auto c                = run_command(compiler + " -o " + exe + " " TEST_DIR "/a60/gdr_io.alg");
    ASSERT_EQ(c.exit_code, 0) << c.output;
    std::filesystem::remove(exe + ".7");
    auto r = run_command("ALGC_UNIT7=" + exe + ".7 " + exe + " < " TEST_DIR "/a60/gdr_io.in");
    EXPECT_EQ(r.exit_code, 0) << r.output;
    EXPECT_EQ(compare(r.output, file_contents(TEST_DIR "/a60/gdr_io.out")), "") << r.output;
    EXPECT_EQ(file_contents(exe + ".7"), "  9\n");
}

// Each check of --check stops the program with its message.
TEST(Run, checks)
{
    const std::vector<std::pair<std::string, std::string>> cases = {
        { "_integer i; i := maxint; i := i + 1", "Integer overflow" },
        { "_integer i; i := -maxint; i := i × 3", "Integer overflow" },
        { "_integer i; i := 10; i := i ↑ 30", "Integer overflow" },
        { "_real x; x := 0; x := 1 / x", "Division by zero" },
        { "_real x; x := maxreal; x := x × 10", "Real overflow" },
        { "_real x; x := 1000; x := exp(x)", "Real overflow" },
        { "_integer i; i := maxreal", "Real 1.79769e+308 too large for an integer" },
        { "_string s; s := ''abc''; PRINT(s[5])", "String index [5:5] out of range 1..3" },
        { "_switch w := L; _goto w[2]; L:", "Switch index 2 out of range" },
        { "_procedure p(a, b); _real a, b; ; _procedure q(f); _procedure f; f(1); q(p)",
          "Procedure p called with 1 parameters instead of 2" },
        { "_procedure p(a); _real _array a; ; _procedure q(f); _procedure f; f(1); q(p)",
          "Parameter 1 of p is a value, not what the formal takes" },
        { "_procedure p(a); _real a; ; _procedure q(f); _procedure f; f(_true); q(p)",
          "Parameter 1 of p is Boolean, not real" },
    };
    int n = 0;
    for (const auto &[body, message] : cases) {
        const std::string exe = BUILD_DIR "/run_check" + std::to_string(n++);
        std::ofstream(exe + ".alg") << "_begin " << body << " _end\n";
        auto c = run_command(compiler + " --check -o " + exe + " " + exe + ".alg");
        ASSERT_EQ(c.exit_code, 0) << body << "\n" << c.output;
        auto r = run_command(exe);
        EXPECT_EQ(r.exit_code, 1) << body;
        EXPECT_EQ(r.output, "Error: " + message + "\n") << body;
    }
}

TEST(Run, division_by_zero)
{
    const std::string src = BUILD_DIR "/run_idiv.alg";
    std::ofstream(src) << "_begin _integer z; z := 0; PRINT(1 ÷ z) _end _eop\n";
    auto r =
        run_command(compiler + " -o " BUILD_DIR "/run_idiv " + src + " && " BUILD_DIR "/run_idiv");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_EQ(r.output, "Error: Integer division by zero\n");
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

// Labels and switches as parameters; jumps out of nested procedures.
TEST(Run, far_goto)
{
    EXPECT_EQ(run_text("far_goto", R"(_begin _integer N;
  _switch SW := L1, L2;
  _procedure JUMP(L); _label L; _goto L;
  _procedure PICK(S, I); _switch S; _integer I; _goto S[I];
  _procedure DEEP(K); _value K; _integer K;
    _begin _procedure INNER; _if K = 0 _then _goto L3 _else DEEP(K - 1);
      INNER
    _end;
  N := 0;
  JUMP(L1);
  PRINT(-1);
L1: N := N + 1; PRINT(N);
  _if N = 1 _then PICK(SW, 2);
  _if N = 2 _then DEEP(5);
  PRINT(-2);
L2: _if N < 2 _then _goto L1;
  _goto L1;
L3: PRINT(''DONE'')
_end _eop
)"),
              "             1              2 DONE\n");
}

TEST(Run, strings)
{
    EXPECT_EQ(run_text("strings2", R"(_begin _string S, T;
  S := ''Счёт'' + ''-42'';
  PRINT(LENGTH(S), S[2], S[2:4], S[5:7]);
  T := S[1:4];
  PRINT(T = ''Счёт'', T < ''Сча'', T _ne S, ''ab'' < ''b'')
_end _eop
)"),
              "             7 ччёт-42true  false true  true  \n");
}

// _code procedures in C, and a _common block they share.
TEST(Run, code_procedures)
{
    const std::string stub = BUILD_DIR "/run_code_stub.c";
    std::ofstream(stub) << R"(#include <stdint.h>
#include <stdio.h>
extern int64_t SHARED[2];
double twice(double *x) { return *x * 2; }
void show(int64_t *a, int64_t *n, double *x)
{
    printf("[%lld %lld %lld] %lld %g %lld\n", (long long)a[0], (long long)a[1],
           (long long)a[2], (long long)*n, *x, (long long)SHARED[1]);
}
)";
    const std::string src = BUILD_DIR "/run_code.alg";
    std::ofstream(src) << R"(_code: _real twice; _code: show;
_begin _real X; _integer _array A[1:3]; _integer N, M;
  _common /SHARED/ N, M;
  N := 7; M := 8; A[1] := 1; A[2] := 2; A[3] := 3;
  X := twice(2.5); PRINT(X, NEWLINE);
  show(A, N, X + 1)
_end _eop
)";
    EXPECT_EQ(run(src, BUILD_DIR "/run_code", false, "/dev/null", stub),
              " 5.00000000000e+00   \n[1 2 3] 7 6 8\n");
}

TEST(Run, format_and_read)
{
    const std::string src = BUILD_DIR "/run_format.alg";
    std::ofstream(src) << R"(_begin _integer I; _real X; _integer _array A[1:3];
  READ(I, X, A);
  _write(54, ''(4HI = I3, 2X, F6.2/3(I2, 1X), E12.4)'') I, X, A, X * 100;
  _print(''(2HA:, 2I3)'') A
_end _eop
)";
    const std::string input = BUILD_DIR "/run_format.in";
    std::ofstream(input) << "42 3.14159\n1 2 3\n";
    EXPECT_EQ(run(src, BUILD_DIR "/run_format", false, input),
              "I =  42    3.14\n 1  2  3   0.3142E+03\nA:  1  2\nA:  3\n");
}
