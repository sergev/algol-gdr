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
