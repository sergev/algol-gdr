//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The listing: the compiler in check mode must print NAME.lst. It was the
// listing of the original with *call setftn:one,long (which sets check mode),
// from the banner on, without what passes 5 and 6 printed.
//
#include "listing.h"

#include <gtest/gtest.h>

#include <sstream>

#include "golden.h"
#include "passes.h"
#include "source.h"
#include "util.h"

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
    Stream stream;
    compile(ctx, stream);
    EXPECT_EQ(out.str(), file_contents(path(name, ".lst")));
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
