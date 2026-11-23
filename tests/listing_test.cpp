//
// The listing: the whole compiler in check mode must print NAME.lst (from
// the original with *call setftn:one,long, which sets check mode) from the
// banner up to *END FILE.
//
#include "listing.h"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include "golden.h"
#include "passes.h"
#include "source.h"

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
    ctx.options.check_mode = true;
    Listing listing(out);
    ctx.listing = &listing;
    ctx.source  = read_source(path(name, ".alg"));
    StdArray object;
    compile(ctx, object);
    EXPECT_EQ(out.str(), expected(name));
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, ListingTest, ::testing::ValuesIn(program_names()), param_name);

// Rows print at once, long and blank ones too; an empty line ends the printout.
TEST(Listing, rows)
{
    std::ostringstream out;
    Listing listing(out);
    listing.print(std::string(70, 'X') + "  ");
    listing.blank();
    listing.finish();
    EXPECT_EQ(out.str(), std::string(70, 'X') + "\n\n\n");
}
