//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include <getopt.h>

#include <cstdlib>
#include <iostream>

#include "driver.h"
#include "options.h"
#include "version.h"

static const char *prog_name = "algc";

static void print_usage(std::ostream &out)
{
    out << "Algol 60 compiler, Version " << version_string() << "\n"
        << "Usage:\n"
        << "    " << prog_name << " [options...] files...\n"
        << "Inputs: .alg or .a60 (Algol), .ll (LLVM IR), .s, .o, .a\n"
        << "Options:\n"
        << "    -o FILE                 Output file (default: NAME for NAME.alg)\n"
        << "    -c                      Compile to an object file\n"
        << "    -S                      Compile to assembly\n"
        << "    --emit-llvm             Compile to LLVM IR (.ll)\n"
        << "    --syntax-only           Check the program only\n"
        << "    -O0, -O1, -O2, -O3      Optimization level (default -O1)\n"
        << "    -g                      Debug information\n"
        << "    -L DIR                  Library directory\n"
        << "    -l LIB                  Link with a library\n"
        << "    --target=TRIPLE         Target machine, as for clang\n"
        << "    --save-temps            Keep the intermediate .ll files\n"
        << "    -v                      Show the commands run\n"
        << "    --list                  Listing of the program\n"
        << "    --check                 Run-time checks\n"
        << "    --dump-ast              Print the AST in YAML\n"
        << "    -h, --help              Display available options\n"
        << "    -V, --version           Print the version number and exit\n"
        << "Environment: ALGC_CLANG (the clang to run), ALGC_RUNTIME (where libalgol.a is)\n";
}

int main(int argc, char *argv[])
{
    enum {
        OPT_LIST = 256,
        OPT_CHECK,
        OPT_DUMP_AST,
        OPT_EMIT_LLVM,
        OPT_SYNTAX_ONLY,
        OPT_SAVE_TEMPS,
        OPT_TARGET,
    };
    static const struct option long_options[] = {
        { "help", no_argument, nullptr, 'h' },
        { "version", no_argument, nullptr, 'V' },
        { "list", no_argument, nullptr, OPT_LIST },
        { "check", no_argument, nullptr, OPT_CHECK },
        { "dump-ast", no_argument, nullptr, OPT_DUMP_AST },
        { "emit-llvm", no_argument, nullptr, OPT_EMIT_LLVM },
        { "syntax-only", no_argument, nullptr, OPT_SYNTAX_ONLY },
        { "save-temps", no_argument, nullptr, OPT_SAVE_TEMPS },
        { "target", required_argument, nullptr, OPT_TARGET },
        {},
    };
    Options opt;
    bool syntax_only = false;

    for (;;) {
        int c = getopt_long(argc, argv, "-hVo:cSO:gvL:l:", long_options, nullptr);
        if (c < 0)
            break;
        switch (c) {
        case 1:
            opt.inputs.push_back(optarg);
            break;
        case 'h':
            print_usage(std::cout);
            return EXIT_SUCCESS;
        case 'V':
            std::cout << "Version " << version_string() << "\n";
            return EXIT_SUCCESS;
        case 'o':
            opt.output_file = optarg;
            break;
        case 'c':
            opt.output = Output::OBJECT;
            break;
        case 'S':
            opt.output = Output::ASSEMBLY;
            break;
        case OPT_EMIT_LLVM:
            opt.output = Output::IR;
            break;
        case OPT_SYNTAX_ONLY:
            syntax_only = true;
            break;
        case 'O':
            if (optarg[0] < '0' || optarg[0] > '3' || optarg[1]) {
                std::cerr << prog_name << ": bad optimization level -O" << optarg << "\n";
                return EXIT_FAILURE;
            }
            opt.optimize = optarg[0] - '0';
            break;
        case 'g':
            opt.debug_info = true;
            break;
        case 'v':
            opt.verbose = true;
            break;
        case 'L':
            opt.lib_dirs.push_back(optarg);
            break;
        case 'l':
            opt.libs.push_back(optarg);
            break;
        case OPT_TARGET:
            opt.target = optarg;
            break;
        case OPT_SAVE_TEMPS:
            opt.save_temps = true;
            break;
        case OPT_LIST:
            opt.list = true;
            break;
        case OPT_CHECK:
            opt.check_mode = true;
            break;
        case OPT_DUMP_AST:
            opt.dump_ast = true;
            break;
        default:
            print_usage(std::cerr);
            return EXIT_FAILURE;
        }
    }
    if (opt.inputs.empty()) {
        print_usage(std::cerr);
        return EXIT_FAILURE;
    }
    if (syntax_only || opt.dump_ast)
        opt.output = Output::NONE;

    try {
        return compile(opt);
    } catch (const std::exception &e) {
        std::cerr << prog_name << ": " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
