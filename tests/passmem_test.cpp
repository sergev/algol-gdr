//
// Passes 2-4 as a whole: besides the stream (pass_test), the words each leaves
// in the workspace and on drum, and its registers, must be those of NAME.mem.
//
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "golden.h"
#include "passes.h"

namespace fs = std::filesystem;

struct MemCase {
    std::string name;
    int pass{};
};

class PassMem : public ::testing::TestWithParam<MemCase> {};

TEST_P(PassMem, workspace)
{
    auto &[name, pass] = GetParam();
    Context ctx;
    load_options(ctx.options, name);
    load_mem(ctx, path(name, ".mem"), pass);
    Stream input;
    for (int k = pass - 1; k >= 1; k--) {
        auto file = path(name, ".s" + std::to_string(k));
        if (fs::exists(file)) {
            input = read_stream_file(file);
            break;
        }
    }
    run_pass(pass, ctx, input);

    std::ifstream in(path(name, ".mem"));
    std::string line, head = "pass " + std::to_string(pass);
    while (std::getline(in, line) && line != head) {
    }
    unsigned bad = 0;
    while (std::getline(in, line) && line.rfind("pass", 0) != 0) {
        std::istringstream f(line);
        f >> std::oct;
        std::string word;
        unsigned addr;
        Word want, got;
        if (line.rfind("reg", 0) == 0) {
            f >> word >> addr >> want;
            got = ctx.regs[addr];
        } else if (line.rfind("drum", 0) == 0) {
            unsigned unit, zone;
            f >> word >> unit >> zone >> addr >> want;
            got = ctx.ws.drum(unit, zone, addr);
        } else {
            f >> addr >> want;
            got = ctx.ws[addr];
        }
        if (got != want && bad++ < 5)
            ADD_FAILURE() << line << ": got " << std::oct << got;
    }
    EXPECT_EQ(bad, 0u);
}

static std::vector<MemCase> cases()
{
    std::vector<MemCase> all;
    for (auto &name : program_names())
        for (int n = 2; n <= 4; n++)
            if (fs::exists(path(name, ".s" + std::to_string(n))))
                all.push_back({ name, n });
    return all;
}

static std::string param_name(const ::testing::TestParamInfo<MemCase> &p)
{
    return p.param.name + "_p" + std::to_string(p.param.pass);
}

INSTANTIATE_TEST_SUITE_P(algol, PassMem, ::testing::ValuesIn(cases()), param_name);
