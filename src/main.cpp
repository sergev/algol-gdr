//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include <getopt.h>

#include <cstdlib>
#include <iostream>

#include "ast.h"
#include "ast_builder.h"
#include "options.h"
#include "passes.h"
#include "source.h"
#include "version.h"

static const char *prog_name = "algc";

static void print_usage(std::ostream &out)
{
    out << "Algol-GDR compiler, Version " << version_string() << "\n"
        << "Usage:\n"
        << "    " << prog_name << " [options...] file.alg\n"
        << "Options:\n"
        << "    --list                  Listing of the program (else errors only)\n"
        << "    --check                 Run-time checks\n"
        << "    --stop-after=N          Print the stream of pass N (1-2) and stop\n"
        << "    --dump-ast              Print the AST in YAML\n"
        << "    -h, --help              Display available options\n"
        << "    -V, --version           Print the version number and exit\n";
}

int main(int argc, char *argv[])
{
    enum { OPT_LIST = 256, OPT_STOP_AFTER, OPT_CHECK, OPT_DUMP_AST };
    static const struct option long_options[] = {
        { "help", no_argument, nullptr, 'h' },
        { "version", no_argument, nullptr, 'V' },
        { "list", no_argument, nullptr, OPT_LIST },
        { "stop-after", required_argument, nullptr, OPT_STOP_AFTER },
        { "check", no_argument, nullptr, OPT_CHECK },
        { "dump-ast", no_argument, nullptr, OPT_DUMP_AST },
        {},
    };
    Options opt;

    for (;;) {
        int c = getopt_long(argc, argv, "-hV", long_options, nullptr);
        if (c < 0)
            break;
        switch (c) {
        case 1:
            if (!opt.source_file.empty()) {
                std::cerr << prog_name << ": too many input files\n";
                return EXIT_FAILURE;
            }
            opt.source_file = optarg;
            break;
        case 'h':
            print_usage(std::cout);
            return EXIT_SUCCESS;
        case 'V':
            std::cout << "Version " << version_string() << "\n";
            return EXIT_SUCCESS;
        case OPT_LIST:
            opt.list = true;
            break;
        case OPT_CHECK:
            opt.check_mode = true;
            break;
        case OPT_DUMP_AST:
            opt.dump_ast = true;
            break;
        case OPT_STOP_AFTER: {
            std::string arg = optarg;
            if (arg.size() != 1 || arg[0] < '1' || arg[0] > '2') {
                std::cerr << prog_name << ": --stop-after: pass number 1-2 expected\n";
                return EXIT_FAILURE;
            }
            opt.stop_after = arg[0] - '0';
            break;
        }
        default:
            print_usage(std::cerr);
            return EXIT_FAILURE;
        }
    }
    if (opt.source_file.empty()) {
        print_usage(std::cerr);
        return EXIT_FAILURE;
    }

    try {
        Context ctx;
        ctx.options = opt;
        ctx.source  = read_source(opt.source_file);
        if (opt.stop_after) {
            Stream stream;
            for (int n = 1; n <= opt.stop_after; n++)
                stream = run_pass(n, ctx, stream);
            write_stream(std::cout, stream);
            return EXIT_SUCCESS;
        }
        // With --dump-ast the listing goes to stderr.
        Listing listing(opt.dump_ast ? std::cerr : std::cout);
        ctx.listing = &listing;
        Stream stream;
        if (!compile(ctx, stream))
            return EXIT_FAILURE;
        if (opt.dump_ast)
            ast::print(std::cout, build_ast(ctx, stream));
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        std::cerr << prog_name << ": " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
