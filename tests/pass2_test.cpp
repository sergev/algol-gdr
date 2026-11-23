//
// Pass 2 as a whole: besides the stream (pass_test), the words it leaves in
// the workspace and on drum, and its registers, must be those of NAME.mem.
//
#include "pass2.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include "golden.h"

class Pass2Mem : public ::testing::TestWithParam<std::string> {};

TEST_P(Pass2Mem, workspace)
{
    const std::string &name = GetParam();
    Context ctx;
    load_options(ctx.options, name);
    load_mem(ctx, path(name, ".mem"), 2);
    pass2(ctx, read_stream_file(path(name, ".s1")));

    std::ifstream in(path(name, ".mem"));
    std::string line;
    while (std::getline(in, line) && line != "pass 2") {
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

static std::vector<std::string> cases()
{
    auto all = program_names();
    std::vector<std::string> names;
    std::copy_if(all.begin(), all.end(), std::back_inserter(names), [](const std::string &name) {
        return std::filesystem::exists(path(name, ".s2"));
    });
    return names;
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, Pass2Mem, ::testing::ValuesIn(cases()), param_name);
