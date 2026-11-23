//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "item.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <sstream>

#include "util.h"

namespace fs = std::filesystem;

TEST(item, fields)
{
    auto item = Item::make(051, 2, 3);
    EXPECT_EQ(item.bits, 030451u);
    EXPECT_EQ(item.code(), 051u);
    EXPECT_EQ(item.count(), 2u);
    EXPECT_EQ(item.value(), 3u);
    EXPECT_EQ(Item{ 016340025 }.value(), 01634u);
}

TEST(item, bad_text)
{
    std::istringstream in("00000102\n0000x102\n");
    EXPECT_THROW(read_stream(in), std::runtime_error);
    std::istringstream big("100000000\n");
    EXPECT_THROW(read_stream(big), std::runtime_error);
}

// Every captured stream reads and writes back unchanged.
TEST(item, round_trip_all)
{
    unsigned count = 0;
    for (auto &e : fs::directory_iterator(TEST_DIR "/algol")) {
        auto ext = e.path().extension().string();
        if (ext.size() != 3 || ext[1] != 's' || ext[2] < '1' || ext[2] > '5')
            continue;
        auto stream = read_stream_file(e.path().string());
        EXPECT_FALSE(stream.empty()) << e.path();
        std::ostringstream out;
        write_stream(out, stream);
        EXPECT_EQ(out.str(), file_contents(e.path().string())) << e.path();
        count++;
    }
    EXPECT_GT(count, 0u);
}
