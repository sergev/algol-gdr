//
// Run objects under dubna and compare the output with NAME.out.
// Skipped when dubna is not installed, or our compiler is not ready.
//
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "util.h"

namespace fs = std::filesystem;

static const std::string compiler = BUILD_DIR "/src/algol-gdr";
static const std::string ref_run  = TEST_DIR "/../scripts/ref-run";

static bool have_dubna()
{
    return run_command("command -v dubna").exit_code == 0;
}

static std::vector<std::string> runnable_names()
{
    std::vector<std::string> names;
    for (auto &e : fs::directory_iterator(TEST_DIR "/algol"))
        if (e.path().extension() == ".out")
            names.push_back(e.path().stem().string());
    std::sort(names.begin(), names.end());
    return names;
}

// Expected object runs as recorded: checks dubna and the scripts.
TEST(run, reference_hello)
{
    if (!have_dubna())
        GTEST_SKIP() << "dubna not found";
    auto r = run_command(ref_run + " " TEST_DIR "/algol/hello.std");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.output, file_contents(TEST_DIR "/algol/hello.out"));
}

class Run : public ::testing::TestWithParam<std::string> {};

TEST_P(Run, output)
{
    if (!have_dubna())
        GTEST_SKIP() << "dubna not found";

    const std::string name = GetParam();
    const std::string dir  = BUILD_DIR "/run_test";
    const std::string obj  = dir + "/" + name + ".std";
    fs::create_directories(dir);
    fs::remove(obj);

    auto c = run_command(compiler + pre_options(TEST_DIR "/algol/" + name + ".pre") + " -o " + obj +
                         " " TEST_DIR "/algol/" + name + ".alg");
    if (c.output.find("not implemented") != std::string::npos)
        GTEST_SKIP() << "compiler not implemented";
    ASSERT_EQ(c.exit_code, 0) << c.output;

    // Side files (.pre, .run, .in) are found next to the object.
    for (const char *ext : { ".pre", ".run", ".in" }) {
        fs::path side = TEST_DIR "/algol/" + name + ext;
        if (fs::exists(side))
            fs::copy_file(side, dir + "/" + name + ext, fs::copy_options::overwrite_existing);
    }
    auto r = run_command(ref_run + " " + obj);
    EXPECT_EQ(r.output, file_contents(TEST_DIR "/algol/" + name + ".out"));
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, Run, ::testing::ValuesIn(runnable_names()), param_name);
