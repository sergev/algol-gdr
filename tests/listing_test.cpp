//
// The listing of pass 1: the banner, the cards and the messages of pass 1,
// as in NAME.lst up to the messages of the later passes.
//
#include "listing.h"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include "golden.h"
#include "passes.h"
#include "source.h"
#include "tables.h"

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

class Listing : public ::testing::TestWithParam<std::string> {};

TEST_P(Listing, pass1)
{
    const std::string &name = GetParam();
    std::ifstream lst(path(name, ".lst"));
    std::string line, want;
    bool on = false;
    while (std::getline(lst, line)) {
        if (line.rfind("A L G O L", 0) == 0) {
            on = true;
            if (line.size() > 40)
                GTEST_SKIP() << "two pages side by side, todo1.md B2.6";
        }
        if (!on)
            continue;
        if (!pass1_line(line))
            break;
        want += line + "\n";
    }
    // Empty lines before the line table belong to pass 5.
    while (want.size() >= 2 && want.substr(want.size() - 2) == "\n\n")
        want.pop_back();

    Context ctx;
    std::ostringstream out;
    ctx.listing = &out;
    load_options(ctx.options, name);
    ctx.source = read_source(path(name, ".alg"));
    run_pass(1, ctx, {});
    EXPECT_EQ(out.str(), want);
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, Listing, ::testing::ValuesIn(program_names()), param_name);
