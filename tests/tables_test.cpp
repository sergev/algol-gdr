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

TEST(tables, iso)
{
    EXPECT_EQ(iso_word("HELLO,"), 02204251423047454u);
    EXPECT_EQ(iso_word("′**′"), 01162505211600000u);
    // A format of pass 2 with its escapes, as at 021676 up to its zero byte.
    auto words = iso_words("IDENTIFIER ′\200′ IS NOT DECLARED\202");
    ASSERT_EQ(words.size(), 6u);
    for (unsigned i = 0; i < 5; i++)
        EXPECT_EQ(words[i], image[021676 + i - image_base]) << i;
    EXPECT_EQ(words[5], image[021703 - image_base] & 07777770000000000u);
}
