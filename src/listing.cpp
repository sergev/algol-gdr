#include "listing.h"

#include <ostream>

namespace {

// clang-format off
const char *const koi7_listing[128] = {
    "", "", "", "", "!", "Ъ", "×", "×",
    "!", "!", "", "!", "!", "", "⩽", "⩾",
    "‘", "", "", "", "", "―", "↑", "⏨",
    "≠", "°", "÷", "’", "⊃", "≡", "∨", "¬",
    " ", "!", "\"", "≠", "◇", "%", "∧", "′",
    "(", ")", "*", "+", ",", "-", ".", "/",
    "0", "1", "2", "3", "4", "5", "6", "7",
    "8", "9", ":", ";", "<", "=", ">", "↑",
    "°", "A", "B", "C", "D", "E", "F", "G",
    "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W",
    "X", "Y", "Z", "[", "‾", "]", "|", "_",
    "Ю", "A", "Б", "Ц", "Д", "E", "Ф", "Г",
    "X", "И", "Й", "K", "Л", "M", "H", "O",
    "П", "Я", "P", "C", "T", "Y", "Ж", "B",
    "Ь", "Ы", "З", "Ш", "Э", "Щ", "Ч", "",
};
// clang-format on

} // namespace

const char *listing_char(unsigned c)
{
    return koi7_listing[c & 0177];
}

std::string listing_text(const Word *words, unsigned first, unsigned last)
{
    std::string s;
    for (unsigned i = first; i < last; i++) {
        unsigned c = (words[i / 6] >> (8 * (5 - i % 6))) & 0377;
        if (c == 3)
            break;
        s += listing_char(c);
    }
    return s;
}

std::string rtrim(std::string s)
{
    s.erase(s.find_last_not_of(' ') + 1);
    return s;
}

namespace {

// Characters of UTF-8 text, as the printer columns.
std::vector<std::string> columns(const std::string &s)
{
    std::vector<std::string> cols;
    for (size_t i = 0; i < s.size(); i++) {
        if ((s[i] & 0xc0) == 0x80 && !cols.empty())
            cols.back() += s[i];
        else
            cols.emplace_back(1, s[i]);
    }
    return cols;
}

std::string join(const std::vector<std::string> &cols, size_t first, size_t last)
{
    std::string s;
    for (size_t i = first; i < last && i < cols.size(); i++)
        s += cols[i];
    return s;
}

} // namespace

void Listing::print(const std::string &text, unsigned indent)
{
    auto cols = columns(rtrim(text));
    if (split_ && cols.size() > 60) {
        row(std::string(indent, ' ') + join(cols, 0, 60));
        std::string rest = rtrim(join(cols, 60, cols.size()));
        if (!rest.empty())
            print(rest, 15);
        return;
    }
    row(std::string(indent, ' ') + join(cols, 0, cols.size()));
}

void Listing::blank()
{
    row("");
}

void Listing::row(const std::string &cell)
{
    if (one_column_) {
        out_ << rtrim(cell) << '\n';
        return;
    }
    if (rows_.size() == 2 * rows_per_column()) {
        print_page();
        rows_.clear();
        pages_++;
    }
    rows_.push_back(rtrim(cell));
}

void Listing::print_page()
{
    const size_t n = rows_per_column();
    if (rows_.size() < n) {
        for (const auto &r : rows_)
            if (!r.empty())
                out_ << r << '\n';
        return;
    }
    std::vector<std::string> lines;
    for (size_t r = 0; r < n; r++) {
        auto left             = columns(rows_[r]);
        const std::string &rt = r + n < rows_.size() ? rows_[r + n] : "";
        if (rt.empty()) {
            if (!left.empty())
                lines.push_back(rows_[r]);
            continue;
        }
        left.resize(65, " ");
        lines.push_back(join(left, 0, 64) + " " + rt);
    }
    for (size_t i = 0; i < lines.size(); i++)
        out_ << lines[i] << (i + 1 == lines.size() ? "\f\n" : "\n");
}

void Listing::finish()
{
    if (one_column_)
        out_ << '\n';
    else
        print_page();
    rows_.clear();
}
