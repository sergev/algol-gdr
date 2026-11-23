#include <getopt.h>

#include <cstdlib>
#include <fstream>
#include <iostream>

#include "dump.h"
#include "options.h"
#include "passes.h"
#include "source.h"
#include "version.h"

static const char *prog_name = "algol-gdr";

static void print_usage(std::ostream &out)
{
    out << "Algol-GDR compiler, Version " << version_string() << "\n"
        << "Usage:\n"
        << "    " << prog_name << " [options...] file.alg\n"
        << "Options:\n"
        << "    -o FILE, --output=FILE  Object file (default file.o)\n"
        << "    --list                  Listing of the program (else errors only)\n"
        << "    --full                  Full listing, with object code\n"
        << "    --check                 Run-time checks, no optimization\n"
        << "    --stop-after=N          Print the stream of pass N (1-5) and stop\n"
        << "    --dump                  Print object file (standard array) and exit\n"
        << "    -h, --help              Display available options\n"
        << "    -V, --version           Print the version number and exit\n";
}

int main(int argc, char *argv[])
{
    enum { OPT_FULL = 256, OPT_LIST, OPT_DUMP, OPT_STOP_AFTER, OPT_CHECK };
    static const struct option long_options[] = {
        { "help", no_argument, nullptr, 'h' },
        { "version", no_argument, nullptr, 'V' },
        { "output", required_argument, nullptr, 'o' },
        { "full", no_argument, nullptr, OPT_FULL },
        { "list", no_argument, nullptr, OPT_LIST },
        { "dump", no_argument, nullptr, OPT_DUMP },
        { "stop-after", required_argument, nullptr, OPT_STOP_AFTER },
        { "check", no_argument, nullptr, OPT_CHECK },
        {},
    };
    Options opt;
    bool dump = false;

    for (;;) {
        int c = getopt_long(argc, argv, "-hVo:", long_options, nullptr);
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
        case 'o':
            opt.object_file = optarg;
            break;
        case OPT_FULL:
            opt.full_listing = true;
            opt.list         = true;
            break;
        case OPT_LIST:
            opt.list = true;
            break;
        case OPT_DUMP:
            dump = true;
            break;
        case OPT_CHECK:
            opt.check_mode = true;
            break;
        case OPT_STOP_AFTER: {
            std::string arg = optarg;
            if (arg.size() != 1 || arg[0] < '1' || arg[0] > '5') {
                std::cerr << prog_name << ": --stop-after: pass number 1-5 expected\n";
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

    if (opt.object_file.empty()) {
        std::string base = opt.source_file;
        if (base.size() > 4 && base.compare(base.size() - 4, 4, ".alg") == 0)
            base.resize(base.size() - 4);
        opt.object_file = base + ".o";
    }

    try {
        if (dump) {
            dump_stdarray(StdArray::read_cards_file(opt.source_file), std::cout);
            return EXIT_SUCCESS;
        }
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
        if (opt.full_listing)
            throw NotImplemented("--full: the object listing is not implemented yet");
        Listing listing(std::cout);
        ctx.listing = &listing;
        StdArray object;
        if (!compile(ctx, object))
            return EXIT_FAILURE;
        std::ofstream out(opt.object_file);
        object.write_cards(out);
        if (!out) {
            std::cerr << prog_name << ": cannot write " << opt.object_file << "\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        std::cerr << prog_name << ": " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
