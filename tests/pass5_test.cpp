//
// Pass 5 on the expected stream of pass 4, then pass 6 on its output: the object
// must match NAME.std. The line table is checked by listing_test.
//
#include "pass5.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <iterator>

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

class Pass5 : public ::testing::TestWithParam<std::string> {};

TEST_P(Pass5, object)
{
    const std::string &name = GetParam();
    Context ctx;
    load_options(ctx.options, name);
    load_mem(ctx, path(name, ".mem"), 5);

    // Pass 4 has no .s3 input in check mode, but always an .s4.
    auto stream = pass5(ctx, read_stream_file(path(name, ".s4")));

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
