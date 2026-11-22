#include "item.h"

#include <fstream>
#include <iomanip>
#include <stdexcept>

Stream read_stream(std::istream &in)
{
    Stream stream;
    std::string line;
    unsigned lineno = 0;
    while (std::getline(in, line)) {
        lineno++;
        if (line.empty())
            continue;
        size_t pos      = 0;
        unsigned long v = 0;
        try {
            v = std::stoul(line, &pos, 8);
        } catch (const std::exception &) {
        }
        if (pos != line.size() || v > Item::MASK)
            throw std::runtime_error("line " + std::to_string(lineno) + ": bad item '" + line +
                                     "'");
        stream.push_back(Item{ uint32_t(v) });
    }
    return stream;
}

Stream read_stream_file(const std::string &filename)
{
    std::ifstream in(filename);
    if (!in)
        throw std::runtime_error(filename + ": cannot open");
    try {
        return read_stream(in);
    } catch (const std::exception &e) {
        throw std::runtime_error(filename + ": " + e.what());
    }
}

void write_stream(std::ostream &out, const Stream &stream)
{
    auto flags = out.flags();
    for (auto item : stream)
        out << std::oct << std::setw(8) << std::setfill('0') << item.bits << '\n';
    out.flags(flags);
}
