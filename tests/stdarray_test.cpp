#include "stdarray.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <sstream>

#include "util.h"

namespace fs = std::filesystem;

static std::vector<fs::path> std_files()
{
    std::vector<fs::path> files;
    for (auto &e : fs::directory_iterator(TEST_DIR "/algol"))
        if (e.path().extension() == ".std")
            files.push_back(e.path());
    std::sort(files.begin(), files.end());
    return files;
}

TEST(stdarray, text_code)
{
    EXPECT_EQ(text_to_utf8(06062574762415500), "PROGRAM");
    EXPECT_EQ(text_to_utf8(04117636441626400), "A/START");
    EXPECT_EQ(utf8_to_text("PROGRAM"), 06062574762415500u);
    EXPECT_EQ(utf8_to_text("A/START"), 04117636441626400u);
    EXPECT_EQ(text_to_utf8(utf8_to_text("ДЛИЯ")), "ДЛИЯ");
}

TEST(stdarray, symbol)
{
    auto s = Symbol::decode(00000400463000000);
    EXPECT_EQ(s.ref, 04004u);
    EXPECT_EQ(s.type, SYM_EXT_LONG);
    EXPECT_EQ(s.addr, 0u);
    EXPECT_EQ(s.encode(), 00000400463000000u);
}

TEST(stdarray, round_trip)
{
    auto files = std_files();
    ASSERT_FALSE(files.empty());
    for (auto &path : files) {
        SCOPED_TRACE(path.string());
        auto obj = StdArray::read_cards_file(path);
        EXPECT_EQ(obj.words.size() % WORDS_PER_CARD, 0u);
        std::ostringstream out;
        obj.write_cards(out);
        EXPECT_EQ(out.str(), file_contents(path));
    }
}

TEST(stdarray, layout_all)
{
    for (auto &path : std_files()) {
        SCOPED_TRACE(path.string());
        auto obj = StdArray::read_cards_file(path);
        auto lay = obj.layout();
        ASSERT_EQ(lay.entries.size(), 1u);
        EXPECT_EQ(text_to_utf8(lay.entries[0].name), "PROGRAM");
        EXPECT_EQ(lay.head_len, 1u);
        EXPECT_EQ(obj.table_row(lay, 0), lay.entries[0].name);
        // Padding is zero.
        for (unsigned i = lay.end_off; i < obj.words.size(); i++)
            EXPECT_EQ(obj.words[i], 0u);
        // Long-name references point into the long-name rows.
        for (unsigned k = 0; k < lay.sym_len; k++) {
            auto s = Symbol::decode(obj.table_row(lay, lay.head_len + k));
            if (s.type == SYM_EXT_LONG || s.type == SYM_COMMON_LONG) {
                unsigned row = s.ref - 04000;
                EXPECT_GE(row, lay.head_len + lay.sym_len);
                EXPECT_LT(row, lay.head_len + lay.sym_len + lay.long_len);
            } else {
                EXPECT_TRUE(s.type == SYM_EXT_SHORT || s.type == SYM_COMMON_SHORT ||
                            s.type == SYM_ABS)
                    << std::oct << s.type;
            }
        }
    }
}

TEST(stdarray, layout_hello)
{
    auto obj = StdArray::read_cards_file(TEST_DIR "/algol/hello.std");
    EXPECT_EQ(obj.label, "PROGRAM ");
    auto lay = obj.layout();
    EXPECT_EQ(lay.head_off, 2u);
    EXPECT_EQ(lay.sym_len, 3u);
    EXPECT_EQ(lay.long_len, 3u);
    EXPECT_EQ(lay.cmd_len, 5u);
    EXPECT_EQ(lay.const_len, 013u);
    EXPECT_EQ(lay.end_off, 35u);
    EXPECT_EQ(obj.words[lay.cmd_off], 03644000567174001u);
    EXPECT_EQ(text_to_utf8(obj.table_row(lay, 4)), "A/START");
    EXPECT_EQ(text_to_utf8(obj.table_row(lay, 5)), "A/SPRI");
    EXPECT_EQ(text_to_utf8(obj.table_row(lay, 6)), "A/STOP");
}

TEST(stdarray, write_without_label)
{
    StdArray obj;
    obj.words = { 06062574762415500, 02000000000000000 };
    std::ostringstream out;
    obj.write_cards(out);
    std::istringstream in(out.str());
    auto back = StdArray::read_cards(in);
    EXPECT_EQ(back.label, "PROGRAM ");
    ASSERT_EQ(back.words.size(), WORDS_PER_CARD);
    EXPECT_EQ(back.words[1], 02000000000000000u);
}

TEST(stdarray, errors)
{
    auto parse = [](const std::string &s) {
        std::istringstream in(s);
        return StdArray::read_cards(in);
    };
    EXPECT_THROW(parse(""), StdArrayError);
    EXPECT_THROW(parse("hello\n"), StdArrayError);
    EXPECT_THROW(parse("`77761 X 1\n`1\n"), StdArrayError); // short card
    EXPECT_THROW(parse("`1\n"), StdArrayError);             // no card header
    EXPECT_THROW(parse("`77777777777777777\n"), StdArrayError);
}
