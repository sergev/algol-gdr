//
// Pass 6 on the expected stream of pass 5: the object must match NAME.std,
// and the summary line the one in NAME.lst (in check mode, as the listing).
//
#include "pass6.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "golden.h"

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

class Pass6 : public ::testing::TestWithParam<std::string> {};

TEST_P(Pass6, object)
{
    const std::string &name = GetParam();
    Context ctx;
    load_mem(ctx, path(name, ".mem"), 6);
    auto obj  = pass6(ctx, read_stream_file(path(name, ".s5")));
    auto want = StdArray::read_cards_file(path(name, ".std"));
    ASSERT_EQ(obj.words.size(), want.words.size());
    for (size_t i = 0; i < want.words.size(); i++)
        ASSERT_EQ(obj.words[i], want.words[i]) << "word " << i;
    EXPECT_EQ(obj.label, want.label);

    load_options(ctx.options, name);
    if (!ctx.options.check_mode)
        return;
    std::ifstream lst(path(name, ".lst"));
    std::string line, summary;
    while (std::getline(lst, line))
        if (line.rfind("PROGRAM   ДЛИHA", 0) == 0)
            summary = line;
    EXPECT_EQ(summary_line(ctx), summary);
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, Pass6, ::testing::ValuesIn(cases()), param_name);
