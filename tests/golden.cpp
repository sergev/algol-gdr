//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

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

Word *drum_word(Context &ctx, unsigned unit, unsigned zone, unsigned offset)
{
    auto at = [](std::vector<Word> &v, size_t i) {
        if (v.size() <= i)
            v.resize(i + 1);
        return &v[i];
    };
    if (unit == 026 && zone == 0)
        return at(ctx.saved_names, offset);
    if (unit == 027 && zone >= 034)
        return at(ctx.strings, (zone - 034) * 1024 + offset);
    return nullptr;
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
            if (Word *w = drum_word(ctx, unit, zone, addr))
                *w = value;
        } else if (line.rfind("reg ", 0) == 0) {
            fields >> word >> addr >> value;
            ASSERT_TRUE(fields && addr < ctx.regs.size()) << filename << ": " << line;
            ctx.regs[addr] = value;
        } else {
            fields >> addr >> value;
            ASSERT_TRUE(fields) << filename << ": " << line;
            if (!stream_page(addr))
                ctx.ws[addr] = value;
        }
    }
}

void load_options(Options &opt, const std::string &name)
{
    std::ifstream in(path(name, ".pre"));
    std::string line;
    opt.list = true;
    while (std::getline(in, line)) {
        if (line == "*check")
            opt.check_mode = true;
        if (line == "*no list")
            opt.list = false;
    }
}
