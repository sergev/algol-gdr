//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The listing of each program must be NAME.lst: the cards as the original
// printed them (its messages are not in it any more). With
// ALGC_UPDATE_GOLDENS set, NAME.lst is written instead.
//
#include "listing.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "diag.h"
#include "frontend.h"
#include "golden.h"
#include "source.h"
#include "util.h"

class ListingTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ListingTest, text)
{
    const std::string &name = GetParam();
    std::ostringstream out, err;
    Listing listing(out);
    Diagnostics diag(name + ".alg", err);
    front_end(read_source(path(name, ".alg")), diag, &listing);
    if (std::getenv("ALGC_UPDATE_GOLDENS")) {
        std::ofstream(path(name, ".lst")) << out.str();
        return;
    }
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
