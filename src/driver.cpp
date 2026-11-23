//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The driver, after blang: .alg → IR → clang → object, assembly or an
// executable linked with libalgol.a.
//

#include "driver.h"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "codegen.h"
#include "diag.h"
#include "frontend.h"
#include "listing.h"
#include "source.h"

namespace fs = std::filesystem;

namespace {

std::string extension(const std::string &path)
{
    return fs::path(path).extension().string();
}

// Algol source: .alg, or .a60 (usually the publication form).
bool is_algol(const std::string &path)
{
    std::string ext = extension(path);
    return ext == ".alg" || ext == ".a60";
}

std::string stem(const std::string &path)
{
    return fs::path(path).stem().string();
}

// Run a command; its status, or -1 when it cannot be started.
int run(const Options &opt, const std::vector<std::string> &args)
{
    if (opt.verbose) {
        std::cerr << opt.arg0 << ": running";
        for (const auto &a : args)
            std::cerr << ' ' << a;
        std::cerr << '\n';
    }
    std::vector<char *> argv(args.size() + 1, nullptr);
    std::transform(args.begin(), args.end(), argv.begin(),
                   [](const std::string &a) { return const_cast<char *>(a.c_str()); });
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        execvp(argv[0], argv.data());
        std::cerr << opt.arg0 << ": cannot run " << args[0] << '\n';
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// clang with the options shared by all its uses.
std::vector<std::string> clang_command(const Options &opt)
{
    std::vector<std::string> cmd{ clang_path() };
    cmd.push_back("-O" + std::to_string(opt.optimize));
    if (opt.debug_info)
        cmd.push_back("-g");
    if (!opt.target.empty())
        cmd.push_back("--target=" + opt.target);
    cmd.push_back("-Wno-override-module");
    return cmd;
}

// A temporary file: NAME in the current directory with --save-temps,
// otherwise a unique one in $TMPDIR.
class Temp {
public:
    Temp(const Options &opt, const std::string &name) : keep_(opt.save_temps)
    {
        if (keep_) {
            path_ = name;
            return;
        }
        std::string tmpl = (fs::temp_directory_path() / ("algc-XXXXXX-" + name)).string();
        int fd           = mkstemps(tmpl.data(), int(name.size()) + 1);
        if (fd < 0)
            throw std::runtime_error("cannot create a temporary file");
        close(fd);
        path_ = tmpl;
    }
    ~Temp()
    {
        if (!keep_)
            std::remove(path_.c_str());
    }
    Temp(const Temp &)            = delete;
    Temp &operator=(const Temp &) = delete;

    const std::string &path() const { return path_; }

private:
    bool keep_;
    std::string path_;
};

// Front end and code generator: the IR of one .alg file; false on errors.
bool translate(const Options &opt, const std::string &input, const std::string &ll)
{
    Diagnostics diag(input, std::cerr);
    Listing listing(std::cout);
    auto program = front_end(read_source(input), diag, opt.list ? &listing : nullptr);
    if (!program)
        return false;
    if (opt.dump_ast)
        ast::print(std::cout, *program);
    if (opt.output == Output::NONE)
        return true;
    CodegenOptions cg;
    cg.module_name = input;
    cg.check       = opt.check_mode;
    cg.target      = opt.target;
    std::ofstream out(ll);
    if (!out)
        throw std::runtime_error(ll + ": cannot write");
    out << codegen(*program, cg);
    if (opt.verbose)
        std::cerr << opt.arg0 << ": generated " << ll << '\n';
    return bool(out);
}

// The output file for one input in a mode without linking.
std::string output_name(const Options &opt, const std::string &input, const char *ext)
{
    if (!opt.output_file.empty())
        return opt.output_file;
    return stem(input) + ext;
}

bool executable_dir(std::string &dir)
{
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return false;
    buf[n] = 0;
    dir    = fs::path(buf).parent_path().string();
    return true;
}

} // namespace

std::string clang_path()
{
    const char *env = std::getenv("ALGC_CLANG");
    return env && *env ? env : "clang";
}

std::string runtime_dir(const std::string &exe_dir, const std::string &target)
{
    const char *env = std::getenv("ALGC_RUNTIME");
    if (env && *env)
        return env;
    fs::path dir(exe_dir);
    std::vector<fs::path> candidates;
    if (!target.empty()) {
        // A runtime of the target only, never the host's.
        candidates.push_back(dir / ".." / "runtime" / target);
        candidates.push_back(dir / ".." / "lib" / target);
    } else {
        candidates.push_back(dir / ".." / "runtime");
        candidates.push_back(dir / ".." / "lib");
        candidates.push_back(dir);
    }
    auto it = std::find_if(candidates.begin(), candidates.end(),
                           [](const fs::path &c) { return fs::exists(c / "libalgol.a"); });
    return it == candidates.end() ? "" : fs::weakly_canonical(*it).string();
}

int compile(const Options &opt)
{
    // Inputs by kind.
    for (const auto &in : opt.inputs) {
        std::string ext = extension(in);
        if (!is_algol(in) && ext != ".ll" && ext != ".s" && ext != ".o" && ext != ".a") {
            std::cerr << opt.arg0 << ": " << in << ": unknown kind of file\n";
            return EXIT_FAILURE;
        }
        bool ok = opt.output == Output::EXECUTABLE || is_algol(in) ||
                  (ext == ".ll" && opt.output != Output::IR && opt.output != Output::NONE) ||
                  (ext == ".s" && opt.output == Output::OBJECT);
        if (!ok) {
            std::cerr << opt.arg0 << ": " << in << ": not an input for this kind of output\n";
            return EXIT_FAILURE;
        }
    }
    if (!opt.output_file.empty() && opt.inputs.size() > 1 && opt.output != Output::EXECUTABLE &&
        opt.output != Output::NONE) {
        std::cerr << opt.arg0 << ": -o with several inputs needs an executable output\n";
        return EXIT_FAILURE;
    }

    if (opt.output == Output::NONE || opt.output == Output::IR) {
        bool ok = true;
        for (const auto &in : opt.inputs)
            ok &= translate(opt, in, opt.output == Output::IR ? output_name(opt, in, ".ll") : "");
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // IR for every .alg first; temporaries live until the end.
    std::vector<std::unique_ptr<Temp>> temps;
    std::vector<std::string> sources; // .alg replaced by its .ll
    bool ok = true;
    for (const auto &in : opt.inputs) {
        if (!is_algol(in)) {
            sources.push_back(in);
            continue;
        }
        temps.push_back(std::make_unique<Temp>(opt, stem(in) + ".ll"));
        ok &= translate(opt, in, temps.back()->path());
        sources.push_back(temps.back()->path());
    }
    if (!ok)
        return EXIT_FAILURE;

    if (opt.output == Output::OBJECT || opt.output == Output::ASSEMBLY) {
        const char *flag = opt.output == Output::OBJECT ? "-c" : "-S";
        const char *ext  = opt.output == Output::OBJECT ? ".o" : ".s";
        for (size_t i = 0; i < sources.size(); i++) {
            auto cmd = clang_command(opt);
            cmd.insert(cmd.end(), { flag, "-o", output_name(opt, opt.inputs[i], ext), sources[i] });
            if (run(opt, cmd) != 0) {
                std::cerr << opt.arg0 << ": clang failed on " << opt.inputs[i] << '\n';
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }

    // Link.
    std::string exe = opt.output_file;
    if (exe.empty()) {
        auto it = std::find_if(opt.inputs.begin(), opt.inputs.end(),
                               [](const std::string &in) { return is_algol(in); });
        exe     = it == opt.inputs.end() ? "a.out" : stem(*it);
    }
    std::string self;
    std::string rt = executable_dir(self) ? runtime_dir(self, opt.target) : "";
    if (rt.empty()) {
        std::cerr << opt.arg0 << ": cannot find libalgol.a\n";
        return EXIT_FAILURE;
    }
    auto cmd = clang_command(opt);
    cmd.insert(cmd.end(), { "-o", exe });
    cmd.insert(cmd.end(), sources.begin(), sources.end());
    auto prefixed = [&cmd](const std::vector<std::string> &list, const char *flag) {
        std::transform(list.begin(), list.end(), std::back_inserter(cmd),
                       [flag](const std::string &x) { return flag + x; });
    };
    prefixed(opt.lib_dirs, "-L");
    prefixed(opt.libs, "-l");
    cmd.push_back("-L" + rt);
    cmd.insert(cmd.end(), { "-lalgol", "-lm" });
    if (run(opt, cmd) != 0) {
        std::cerr << opt.arg0 << ": linking failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
