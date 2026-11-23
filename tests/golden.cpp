#include "golden.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

std::vector<std::string> program_names()
{
    std::vector<std::string> names;
    for (auto &e : fs::directory_iterator(TEST_DIR "/algol"))
        if (e.path().extension() == ".alg")
            names.push_back(e.path().stem().string());
    std::sort(names.begin(), names.end());
    return names;
}

std::string path(const std::string &name, const std::string &ext)
{
    return TEST_DIR "/algol/" + name + ext;
}

void load_mem(Context &ctx, const std::string &filename, int pass)
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
        fields >> std::oct;
        std::string word;
        unsigned addr;
        Word value;
        if (line.rfind("drum ", 0) == 0) {
            unsigned unit, zone;
            fields >> word >> unit >> zone >> addr >> value;
            ASSERT_TRUE(fields) << filename << ": " << line;
            ctx.ws.drum(unit, zone, addr) = value;
        } else if (line.rfind("reg ", 0) == 0) {
            fields >> word >> addr >> value;
            ASSERT_TRUE(fields && addr < ctx.regs.size()) << filename << ": " << line;
            ctx.regs[addr] = value;
        } else {
            fields >> addr >> value;
            ASSERT_TRUE(fields) << filename << ": " << line;
            ctx.ws[addr] = value;
        }
    }
}

void load_options(Options &opt, const std::string &name)
{
    std::ifstream in(path(name, ".pre"));
    std::string line;
    while (std::getline(in, line)) {
        if (line == "*check")
            opt.check_mode = true;
        if (line.rfind("*call setftn:", 0) != 0)
            continue;
        std::istringstream args(line.substr(13));
        std::string arg;
        while (std::getline(args, arg, ',')) {
            if (arg == "one")
                opt.setftn_one = true;
            else if (arg == "long")
                opt.setftn_long = true;
        }
    }
}
