#include "tables.h"

#include <gtest/gtest.h>

using namespace tables;

static Word workspace(unsigned addr)
{
    for (size_t i = 0; i < workspace_init_size; i++)
        if (workspace_init[i].addr == addr)
            return workspace_init[i].value;
    return 0;
}

TEST(tables, workspace)
{
    EXPECT_EQ(workspace(02166), 02025112220254400u); // BEGIN
    EXPECT_EQ(workspace(02167), 07760000000000040u);
    EXPECT_EQ(workspace(03771), 00020000000000000u);
    EXPECT_EQ(workspace(05754), 06402340000000027u);
}

TEST(tables, sizes)
{
    EXPECT_EQ(chclass.words.size(), 128u);
    EXPECT_EQ(p2disp.words.size(), 0106u);
    EXPECT_EQ(p4disp.words.size(), 0170u / 2);
    EXPECT_EQ(rtnames.words.size(), 0132u);
    EXPECT_EQ(p3regs.words.size(), 8u);
}

TEST(tables, contents)
{
    EXPECT_EQ(text_to_utf8(rtnames.at(030124)), "A/MINUS");
    EXPECT_EQ(p2disp.at(021720), 00301760501020000u);
    EXPECT_EQ(p3regs.at(023154), 01200u); // R5
    EXPECT_THROW(p3regs.at(023164), std::out_of_range);
}
