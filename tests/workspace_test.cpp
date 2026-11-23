#include "workspace.h"

#include <gtest/gtest.h>

TEST(workspace, init)
{
    Workspace ws;
    EXPECT_EQ(ws[02000], 051u);
    EXPECT_EQ(ws[02166], 02025112220254400u); // ARRAY
    EXPECT_EQ(ws[04400], 0u);                 // D-list
    EXPECT_EQ(ws[05754], 06402340000000027u);
    EXPECT_EQ(ws[027245], 0u);
}

TEST(workspace, write_and_reset)
{
    Workspace ws;
    ws[02000]  = 1;
    ws[011777] = 2;
    ws[027644] = 3;
    EXPECT_EQ(ws[02000], 1u);
    EXPECT_EQ(ws[011777], 2u);
    EXPECT_EQ(ws[027644], 3u);
    ws.reset();
    EXPECT_EQ(ws[02000], 051u);
    EXPECT_EQ(ws[027644], 0u);
}

TEST(workspace, bounds)
{
    Workspace ws;
    EXPECT_THROW(ws[01735], std::out_of_range);
    EXPECT_THROW(ws[012000], std::out_of_range);
    EXPECT_THROW(ws[027645], std::out_of_range);
    EXPECT_TRUE(Workspace::contains(06000));
    EXPECT_FALSE(Workspace::contains(027244));
}

TEST(workspace, drum)
{
    Workspace ws;
    EXPECT_EQ(ws.drum(027, 033, 01777), 0u);
    ws.drum(026, 0, 5) = 7;
    EXPECT_EQ(ws.drum(026, 0, 5), 7u);
    EXPECT_THROW(ws.drum(026, 0, 1024), std::out_of_range);
    ws.reset();
    EXPECT_EQ(ws.drum(026, 0, 5), 0u);
}
