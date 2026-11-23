//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Each pass on its own: for each tests/algol program and pass N with an
// expected stream NAME.sN, run pass N on the expected stream of the previous
// pass, with the workspace rebuilt from NAME.mem, and compare.
// Skipped while the pass is not implemented.
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
        for (int n = 1; n <= 5; n++)
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
    load_mem(ctx, path(name, ".mem"), pass);

    Stream input;
    if (pass == 1) {
        ctx.source = read_source(path(name, ".alg"));
    } else {
        // Pass 3 is skipped in check mode: take the latest stream before.
        for (int k = pass - 1; k >= 1; k--) {
            auto file = path(name, ".s" + std::to_string(k));
            if (fs::exists(file)) {
                input = read_stream_file(file);
                break;
            }
        }
    }
    Stream got;
    try {
        got = run_pass(pass, ctx, input);
    } catch (const NotImplemented &e) {
        GTEST_SKIP() << e.what();
    }
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
