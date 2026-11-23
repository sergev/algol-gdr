//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Pass 1 as a whole: besides the stream (pass_test), the words it leaves in
// the workspace and on drum must be those of NAME.mem.
//
#include "pass1.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include "golden.h"
#include "source.h"

class Pass1Mem : public ::testing::TestWithParam<std::string> {};

TEST_P(Pass1Mem, workspace)
{
    const std::string &name = GetParam();
    Context ctx;
    load_options(ctx.options, name);
    ctx.source = read_source(path(name, ".alg"));
    Pass1(ctx).run();

    std::ifstream in(path(name, ".mem"));
    std::string line;
    std::getline(in, line); // pass 1
    unsigned bad = 0;
    while (std::getline(in, line) && line.rfind("pass", 0) != 0) {
        std::istringstream f(line);
        f >> std::oct;
        unsigned addr;
        Word want, got;
        if (line.rfind("reg", 0) == 0)
            continue;
        if (line.rfind("drum", 0) == 0) {
            unsigned unit, zone;
            std::string word;
            f >> word >> unit >> zone >> addr >> want;
            const Word *w = drum_word(ctx, unit, zone, addr);
            if (!w)
                continue;
            got = *w;
        } else {
            f >> addr >> want;
            // The listing state, the card and the lexer's scratch cells differ
            // or do not matter; the tables do.
            if (addr < Pass1::NAMES && addr != Pass1::D_CONST && addr != Pass1::D_STRINGS)
                continue;
            // The stream pages: the stream is checked by pass_test.
            if (stream_page(addr))
                continue;
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

INSTANTIATE_TEST_SUITE_P(algol, Pass1Mem, ::testing::ValuesIn(cases()), param_name);
