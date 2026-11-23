//
// Compile each tests/algol/*.alg with our compiler and compare the object
// with the expected NAME.std from the original compiler.
// Skipped while the compiler reports "not implemented".
//
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "util.h"

namespace fs = std::filesystem;

static const std::string compiler = BUILD_DIR "/src/algol-gdr";

static std::vector<std::string> program_names()
{
    std::vector<std::string> names;
    for (auto &e : fs::directory_iterator(TEST_DIR "/algol"))
        if (e.path().extension() == ".alg")
            names.push_back(e.path().stem().string());
    std::sort(names.begin(), names.end());
    return names;
}

class Compile : public ::testing::TestWithParam<std::string> {};

TEST_P(Compile, object)
{
    const std::string name = GetParam();
    const std::string src  = TEST_DIR "/algol/" + name + ".alg";
    const std::string want = TEST_DIR "/algol/" + name + ".std";
    const std::string dir  = BUILD_DIR "/compile_test";
    const std::string got  = dir + "/" + name + ".std";
    fs::create_directories(dir);
    fs::remove(got);

    auto r = run_command(compiler + pre_options(TEST_DIR "/algol/" + name + ".pre") + " -o " + got +
                         " " + src);
    if (r.output.find("not implemented") != std::string::npos)
        GTEST_SKIP() << "compiler not implemented";

    if (fs::exists(want)) {
        EXPECT_EQ(r.exit_code, 0) << r.output;
        EXPECT_EQ(file_contents(got), file_contents(want));
    } else {
        // Program with errors: no object.
        EXPECT_NE(r.exit_code, 0);
        EXPECT_FALSE(fs::exists(got));
    }
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, Compile, ::testing::ValuesIn(program_names()), param_name);
