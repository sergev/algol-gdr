//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "util.h"

#include <sys/wait.h>

#include <cstdio>
#include <fstream>
#include <sstream>

RunResult run_command(const std::string &cmd)
{
    RunResult result;
    FILE *pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe)
        return result;
    char buf[4096];
    std::size_t n;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0)
        result.output.append(buf, n);
    int status = pclose(pipe);
    if (status != -1 && WIFEXITED(status))
        result.exit_code = WEXITSTATUS(status);
    return result;
}

std::string file_contents(const std::string &filename)
{
    std::ifstream input(filename, std::ios::binary);
    std::ostringstream buf;
    buf << input.rdbuf();
    return buf.str();
}

std::string pre_options(const std::string &path)
{
    std::ifstream in(path);
    std::string line, opts;
    bool list = true;
    while (std::getline(in, line))
        if (line == "*check")
            opts += " --check";
        else if (line == "*no list")
            list = false;
    return list ? opts + " --list" : opts;
}
