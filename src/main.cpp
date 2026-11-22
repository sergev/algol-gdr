#include <getopt.h>

#include <cstdlib>
#include <iostream>

#include "dump.h"
#include "options.h"
#include "source.h"
#include "version.h"

static const char *prog_name = "algol-gdr";

static void print_usage(std::ostream &out)
{
    out << "Algol-GDR compiler, Version " << version_string() << "\n"
        << "Usage:\n"
        << "    " << prog_name << " [options...] file.alg\n"
        << "Options:\n"
        << "    -o FILE, --output=FILE  Object file (default stdarray.out)\n"
        << "    --full                  Full listing, with object code (like *FULL)\n"
        << "    --no-list               No listing (like *NO LIST)\n"
        << "    --dump                  Print object file (standard array) and exit\n"
        << "    -h, --help              Display available options\n"
        << "    -V, --version           Print the version number and exit\n";
}

int main(int argc, char *argv[])
{
    enum { OPT_FULL = 256, OPT_NO_LIST, OPT_DUMP };
    static const struct option long_options[] = {
        { "help", no_argument, nullptr, 'h' },
        { "version", no_argument, nullptr, 'V' },
        { "output", required_argument, nullptr, 'o' },
        { "full", no_argument, nullptr, OPT_FULL },
        { "no-list", no_argument, nullptr, OPT_NO_LIST },
        { "dump", no_argument, nullptr, OPT_DUMP },
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
            break;
        case OPT_NO_LIST:
            opt.no_listing = true;
            break;
        case OPT_DUMP:
            dump = true;
            break;
        default:
            print_usage(std::cerr);
            return EXIT_FAILURE;
        }
    }
    if (opt.source_file.empty()) {
        print_usage(std::cerr);
        return EXIT_FAILURE;
    }
    if (opt.full_listing && opt.no_listing) {
        std::cerr << prog_name << ": --full and --no-list are mutually exclusive\n";
        return EXIT_FAILURE;
    }

    try {
        if (dump) {
            dump_stdarray(StdArray::read_cards_file(opt.source_file), std::cout);
            return EXIT_SUCCESS;
        }
        std::string text = read_source(opt.source_file);
        (void)text;
        std::cerr << prog_name << ": compilation not implemented yet\n";
        return EXIT_FAILURE;
    } catch (const std::exception &e) {
        std::cerr << prog_name << ": " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
