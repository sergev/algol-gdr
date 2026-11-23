//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The streams of passes 1 and 2 of each tests/algol program must be
// NAME.s1 and NAME.s2 (when the original made them).
//
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "golden.h"
#include "passes.h"
#include "source.h"

namespace fs = std::filesystem;

struct PassCase {
    std::string name;
    int pass{};
};

static std::vector<PassCase> pass_cases()
{
    std::vector<PassCase> cases;
    for (auto &name : program_names())
        for (int n = 1; n <= 2; n++)
            if (fs::exists(path(name, ".s" + std::to_string(n))))
                cases.push_back({ name, n });
    return cases;
}

class Pass : public ::testing::TestWithParam<PassCase> {};

TEST_P(Pass, stream)
{
    auto &[name, pass] = GetParam();
    Context ctx;
    load_options(ctx.options, name);
    ctx.source = read_source(path(name, ".alg"));
    Stream got;
    for (int n = 1; n <= pass; n++)
        got = run_pass(n, ctx, got);
    auto want = read_stream_file(path(name, ".s" + std::to_string(pass)));
    auto diff = std::mismatch(got.begin(), got.end(), want.begin(), want.end());
    EXPECT_TRUE(diff.first == got.end() && diff.second == want.end())
        << "first difference at item " << (diff.first - got.begin()) << " of " << want.size();
}

static std::string param_name(const ::testing::TestParamInfo<PassCase> &p)
{
    return p.param.name + "_s" + std::to_string(p.param.pass);
}

INSTANTIATE_TEST_SUITE_P(algol, Pass, ::testing::ValuesIn(pass_cases()), param_name);
