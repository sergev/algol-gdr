//
// The listing: pass 1, then passes 5 and 6 on the expected stream of pass 4,
// must print NAME.lst from the banner up to *END FILE. Programs with messages
// of passes 2-4 are compared up to them, on one page only.
//
#include "listing.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "golden.h"
#include "pass5.h"
#include "pass6.h"
#include "passes.h"
#include "source.h"
#include "tables.h"

namespace fs = std::filesystem;

// A message of 'COMMON': three words at 16045 + 3n.
static bool common_message(const std::string &line)
{
    for (unsigned n = 0; n < 7; n++) {
        Word w[3];
        for (unsigned i = 0; i < 3; i++)
            w[i] = tables::commsg.at(016045 + 3 * n + i);
        if (line == rtrim(listing_text(w, 0, 18)))
            return true;
    }
    return false;
}

static bool pass1_line(const std::string &line)
{
    if (line.empty() || common_message(line))
        return true;
    for (const char *p :
         { "ERROR ", "HET ", "↑EOP↑", "WARNING ", "30 ERRORS", "A L G O L", "    (7.01.82)" })
        if (line.rfind(p, 0) == 0)
            return true;
    // A card: columns 73-80, then the line number and a dot; or its second half.
    return (line.size() > 12 && line[12] == '.') || (line.size() > 6 && line[6] == '.') ||
           line.rfind("               ", 0) == 0;
}

// NAME.lst from the banner up to *END FILE.
static std::string expected(const std::string &name)
{
    std::ifstream lst(path(name, ".lst"));
    std::string line, text;
    bool on = false;
    while (std::getline(lst, line)) {
        if (line.rfind("A L G O L", 0) == 0)
            on = true;
        if (line.rfind("*END FILE", 0) == 0)
            break;
        if (on)
            text += line + "\n";
    }
    return text;
}

class ListingTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ListingTest, text)
{
    const std::string &name = GetParam();
    Context ctx;
    std::ostringstream out;
    load_options(ctx.options, name);
    Listing listing(out, ctx.options.setftn_one, ctx.options.setftn_long);
    ctx.listing = &listing;
    ctx.source  = read_source(path(name, ".alg"));
    run_pass(1, ctx, {});
    std::string want = expected(name);

    if (!ctx.finished && fs::exists(path(name, ".s4"))) {
        load_mem(ctx, path(name, ".mem"), 5);
        auto stream = pass5(ctx, read_stream_file(path(name, ".s4")));
        pass6(ctx, stream);
        listing.print(summary_line(ctx));
    } else if (!ctx.finished) {
        // Up to the messages of pass 2.
        std::istringstream in(want);
        std::string line;
        want.clear();
        while (std::getline(in, line) && pass1_line(line)) {
            if (std::count_if(line.begin(), line.end(), [](char c) { return (c & 0xc0) != 0x80; }) >
                64)
                GTEST_SKIP() << "messages of pass 2 on a page of two columns";
            want += line + "\n";
        }
        while (want.size() >= 2 && want.substr(want.size() - 2) == "\n\n")
            want.pop_back();
    }
    listing.finish();
    EXPECT_EQ(out.str(), want);
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, ListingTest, ::testing::ValuesIn(program_names()), param_name);

// AG01644 splits a text of 11 words after 10, the rest indented by 15.
TEST(Listing, split)
{
    std::ostringstream out;
    Listing listing(out, false, false);
    listing.print("IDENTIFIER ′IDENTI′ IS NOT DECLARED. NEXT STATEMENT IS   89/ 1");
    listing.print(std::string(60, 'X') + "      ");
    listing.finish();
    EXPECT_EQ(out.str(), "IDENTIFIER ′IDENTI′ IS NOT DECLARED. NEXT STATEMENT IS   89/\n" +
                             std::string(16, ' ') + "1\n" + std::string(60, 'X') + "\n");
}

// With setftn:one,long nothing is split; with setftn:one blank lines show and
// the printout ends with an empty line.
TEST(Listing, one_column)
{
    std::ostringstream out;
    Listing listing(out, true, true);
    listing.print(std::string(70, 'X'));
    listing.blank();
    listing.finish();
    EXPECT_EQ(out.str(), std::string(70, 'X') + "\n\n\n");
}

// Pages of 2 x 65 rows, then 2 x 66; the last one with its left column only.
TEST(Listing, pages)
{
    std::ostringstream out;
    Listing listing(out, false, false);
    for (unsigned i = 1; i <= 2 * 65 + 2 * 66 + 3; i++)
        listing.print(std::to_string(i));
    listing.finish();
    std::string want;
    auto page = [&](unsigned first, unsigned n) {
        for (unsigned r = 0; r < n; r++) {
            std::string left = std::to_string(first + r);
            want += left + std::string(65 - left.size(), ' ') + std::to_string(first + n + r) +
                    (r + 1 < n ? "\n" : "\f\n");
        }
    };
    page(1, 65);
    page(131, 66);
    want += "263\n264\n265\n";
    EXPECT_EQ(out.str(), want);
}
