//
// The object listing of --full: our rows, laid out in pages as A/PRINTM
// prints them, must be NAME.full.lst after the summary line (the original
// with *call setftn:one,long, so in check mode).
//
#include "printm.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>

#include "golden.h"
#include "passes.h"
#include "source.h"

// NAME.full.lst after the summary line, up to *END FILE; empty after errors.
static std::vector<std::string> expected(const std::string &name)
{
    std::ifstream in(path(name, ".full.lst"));
    std::vector<std::string> lines;
    std::string line;
    bool on = false;
    while (std::getline(in, line)) {
        if (line.rfind("*END FILE", 0) == 0)
            break;
        if (on)
            lines.push_back(line);
        if (line.rfind("PROGRAM   ДЛИHA", 0) == 0)
            on = true;
    }
    return lines;
}

static size_t width(const std::string &s)
{
    return std::count_if(s.begin(), s.end(), [](char c) { return (c & 0xc0) != 0x80; });
}

//
// The pages of A/PRINTM: a line with a form feed, then pages of 4 columns of
// 64 rows, 32 characters wide; a full page ends with a form feed. The last
// page has 32 + its words rows (at most 64), or on the first page as many as
// it has rows. A row blank in all columns is left out within the rows of the
// first column. A name prints 8 characters, the last 2 as nothing, so the
// next column comes 2 characters early.
//
static std::vector<std::string> pages(const std::vector<std::string> &rows,
                                      const std::set<size_t> &names)
{
    std::vector<std::string> out{ "\f" };
    const size_t n = rows.size();
    for (size_t first = 0; first < n; first += 256) {
        const size_t len = std::min<size_t>(256, n - first);
        const bool last  = first + 256 >= n;
        size_t height    = 64;
        if (last)
            height = std::min<size_t>(64, first ? 32 + len / 2 : len);
        for (size_t r = 0; r < height; r++) {
            std::string line;
            bool blank = true;
            for (size_t c = 0; c < 4; c++) {
                size_t i         = first + c * 64 + r;
                std::string cell = c * 64 + r < len ? rows[i] : "";
                blank            = blank && cell.empty();
                size_t w         = names.count(i) ? 30 : 32;
                line += cell + std::string(w - std::min(w, width(cell)), ' ');
            }
            if (blank && r < len)
                continue;
            out.push_back(rtrim(line));
        }
        if (!last)
            out.back() += '\f';
    }
    return out;
}

class Printm : public ::testing::TestWithParam<std::string> {};

TEST_P(Printm, pages)
{
    const std::string &name = GetParam();
    auto want               = expected(name);
    if (want.empty())
        GTEST_SKIP() << "no object";
    Context ctx;
    load_options(ctx.options, name);
    ctx.options.check_mode = true;
    ctx.source             = read_source(path(name, ".alg"));
    StdArray object;
    ASSERT_TRUE(compile(ctx, object));

    // The value rows of the names in the description table.
    auto lay     = object.layout();
    size_t table = 2 * (lay.cmd_len + lay.const_len);
    std::set<size_t> names;
    for (unsigned row = 0; row < lay.head_len + lay.sym_len + lay.long_len; row++)
        if (row < lay.head_len || row >= lay.head_len + lay.sym_len)
            names.insert(table + 2 * row + 1);

    auto got = pages(printm_rows(object), names);
    ASSERT_EQ(got.size(), want.size());
    for (size_t i = 0; i < want.size(); i++)
        EXPECT_EQ(got[i], want[i]) << "line " << i;
}

static std::vector<std::string> cases()
{
    auto all = program_names();
    std::vector<std::string> names;
    std::copy_if(all.begin(), all.end(), std::back_inserter(names),
                 [](const std::string &name) { return bool(std::ifstream(path(name, ".full.lst"))); });
    return names;
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, Printm, ::testing::ValuesIn(cases()), param_name);

// No rounding, and 0377 in the low bits of the fraction: 0.6 and 3.0 get a
// 1 in the last digit, 0.5 does not.
TEST(Printm, real)
{
    EXPECT_EQ(printm_real(0'4011'4631'4631'4632), " 6.00000000001-01");
    EXPECT_EQ(printm_real(0'4114'0000'0000'0000), " 3.00000000001+00");
    EXPECT_EQ(printm_real(0'4010'0000'0000'0000), " 5.00000000000-01");
    EXPECT_EQ(printm_real(0'7615'7013'3263'5166), " 1.00000000000+18");
    EXPECT_EQ(printm_real(0'0251'1622'7350'7223), " 1.00000000000-18");
}

// 0 prints as a blank and a blank as ⊃.
TEST(Printm, text)
{
    EXPECT_EQ(printm_text(0'1005'3517'2444'6104), "⊃WORLD");
    EXPECT_EQ(printm_text(0'0000'0000'0000'0001), "     !");
}
