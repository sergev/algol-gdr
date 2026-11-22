//
// Each pass on its own: for each tests/algol program and pass N with an
// expected stream NAME.sN, run pass N on the expected stream of the previous
// pass, with the workspace rebuilt from NAME.mem, and compare.
// Skipped while the pass is not implemented.
//
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "passes.h"
#include "source.h"

namespace fs = std::filesystem;

struct PassCase {
    std::string name;
    int pass{};
};

static std::string path(const std::string &name, const std::string &ext)
{
    return TEST_DIR "/algol/" + name + ext;
}

static std::vector<PassCase> pass_cases()
{
    std::vector<PassCase> cases;
    std::vector<std::string> names;
    for (auto &e : fs::directory_iterator(TEST_DIR "/algol"))
        if (e.path().extension() == ".alg")
            names.push_back(e.path().stem().string());
    std::sort(names.begin(), names.end());
    for (auto &name : names)
        for (int n = 1; n <= 5; n++)
            if (fs::exists(path(name, ".s" + std::to_string(n))))
                cases.push_back({ name, n });
    return cases;
}

// Apply the words written by passes before `pass`, from NAME.mem.
static void load_mem(Workspace &ws, const std::string &filename, int pass)
{
    std::ifstream in(filename);
    ASSERT_TRUE(in) << filename;
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        if (line.rfind("pass ", 0) == 0) {
            n = std::stoi(line.substr(5));
            continue;
        }
        if (n >= pass)
            break;
        std::istringstream fields(line);
        unsigned addr;
        Word value;
        fields >> std::oct >> addr >> value;
        ASSERT_TRUE(fields) << filename << ": " << line;
        ws[addr] = value;
    }
}

class Pass : public ::testing::TestWithParam<PassCase> {};

TEST_P(Pass, stream)
{
    auto &[name, pass] = GetParam();
    Context ctx;
    load_mem(ctx.ws, path(name, ".mem"), pass);

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
