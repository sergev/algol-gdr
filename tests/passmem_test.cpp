//
// Passes 2-4 as a whole: besides the stream (pass_test), the words each leaves
// in the workspace and on drum must be those of NAME.mem. The passes keep
// their streams in vectors: the stream pages, their drum zones and cursors
// are not compared, nor the registers (no later pass reads them).
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

// Registers, the stream cursors 02000-02001 (and pass 3's at 02013, 02015,
// 02042), pages 06000-11777, drum 26 from zone 2; for pass 2 also the cells
// of 01644 (01736-01740) and the cursors saved with page 02000 (drum 26
// zones 0, 1, words 0-1); for pass 4 the template cursor 02007.
static bool stream_part(int pass, const std::string &line)
{
    if (line.rfind("reg", 0) == 0)
        return true;
    std::istringstream f(line);
    f >> std::oct;
    if (line.rfind("drum", 0) == 0) {
        std::string word;
        unsigned unit, zone, offset;
        f >> word >> unit >> zone >> offset;
        return unit == 026 && (zone >= 2 || (pass == 2 && offset < 2));
    }
    unsigned addr;
    f >> addr;
    if (pass == 3 && (addr == 02013 || addr == 02015 || addr == 02042))
        return true;
    if (pass == 2 && addr >= 01736 && addr <= 01740)
        return true;
    if (pass == 4 && addr == 02007) // the template cursor of the original
        return true;
    return addr == 02000 || addr == 02001 || stream_page(addr);
}

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
        if (stream_part(pass, line))
            continue;
        std::istringstream f(line);
        f >> std::oct;
        unsigned addr;
        Word want, got;
        if (line.rfind("drum", 0) == 0) {
            std::string word;
            unsigned unit, zone;
            f >> word >> unit >> zone >> addr >> want;
            const Word *w = drum_word(ctx, unit, zone, addr);
            if (!w)
                continue;
            got = *w;
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
