//
// Pass 5 on the expected stream of pass 4, then pass 6 on its output: the line
// table must match NAME.lst, and the object NAME.std.
//
#include "pass5.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include "golden.h"
#include "pass6.h"

namespace fs = std::filesystem;

static std::vector<std::string> cases()
{
    auto all = program_names();
    std::vector<std::string> names;
    std::copy_if(all.begin(), all.end(), std::back_inserter(names), [](const std::string &name) {
        return fs::exists(path(name, ".s5")) && fs::exists(path(name, ".std"));
    });
    return names;
}

// Lines of the listing from the line table heading (with an empty line
// before it) up to the summary line.
static std::string line_table(const std::string &filename)
{
    std::ifstream in(filename);
    std::string line, prev, text;
    bool on = false;
    while (std::getline(in, line)) {
        if (line.rfind("CTPOK", 0) == 0) {
            on = true;
            if (prev.empty())
                text = "\n";
        }
        if (line.rfind("PROGRAM   ", 0) == 0)
            break;
        if (on)
            text += line + "\n";
        prev = line;
    }
    return text;
}

class Pass5 : public ::testing::TestWithParam<std::string> {};

TEST_P(Pass5, object)
{
    const std::string &name = GetParam();
    Context ctx;
    std::ostringstream listing;
    ctx.listing = &listing;
    load_options(ctx.options, name);
    load_mem(ctx, path(name, ".mem"), 5);

    // Pass 4 has no .s3 input in check mode, but always an .s4.
    auto stream = pass5(ctx, read_stream_file(path(name, ".s4")));
    EXPECT_EQ(listing.str(), line_table(path(name, ".lst")));

    auto obj  = pass6(ctx, stream);
    auto want = StdArray::read_cards_file(path(name, ".std"));
    ASSERT_EQ(obj.words.size(), want.words.size());
    for (size_t i = 0; i < want.words.size(); i++)
        ASSERT_EQ(obj.words[i], want.words[i]) << "word " << i;
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, Pass5, ::testing::ValuesIn(cases()), param_name);
