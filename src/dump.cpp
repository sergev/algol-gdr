#include "dump.h"

#include <iomanip>
#include <ostream>
#include <sstream>

#include "besm6.h"

static std::string octal(unsigned long long v, int width)
{
    std::ostringstream s;
    s << std::oct << std::setfill('0') << std::setw(width) << v;
    return s.str();
}

//
// Name of description table row: TEXT name or symbol.
//
static std::string row_name(const StdArray &obj, const Layout &lay, unsigned row)
{
    unsigned nrows = lay.head_len + lay.sym_len + lay.long_len;
    if (row >= nrows)
        return "row " + octal(row, 1) + "?";
    if (row < lay.head_len || row >= lay.head_len + lay.sym_len)
        return text_to_utf8(obj.table_row(lay, row));

    auto sym = Symbol::decode(obj.table_row(lay, row));
    if (sym.type == SYM_EXT_LONG && sym.ref >= 04000 && sym.ref - 04000 < nrows)
        return text_to_utf8(obj.table_row(lay, sym.ref - 04000));
    if (sym.type == SYM_EXT_SHORT)
        return text_to_utf8(Word(sym.ref) << 24);
    if (sym.type == SYM_ABS)
        return "=" + octal(sym.addr, 5);
    return "sym " + octal(sym.type, 3);
}

static void print_insn(const StdArray &obj, const Layout &lay, unsigned cmd, std::ostream &out)
{
    auto insn = Insn::decode(cmd);
    std::ostringstream text;

    // Register in decimal, as Madlen writes it.
    if (insn.reg)
        text << std::setw(2) << std::dec << insn.reg << ' ';
    else
        text << "   ";
    text << std::left << std::setw(4) << besm6_opname(insn.opcode) << ' '
         << octal(insn.addr, insn.is_long ? 5 : 4);

    std::string comment;
    if (insn.is_long && insn.addr >= 074000)
        comment = row_name(obj, lay, insn.addr - 074000);
    else if (insn.is_long && insn.addr >= 040000)
        comment = "@" + octal(insn.addr - 040000, 1);

    if (comment.empty())
        out << text.str();
    else
        out << std::left << std::setw(20) << text.str() << std::right << "; " << comment;
}

void dump_stdarray(const StdArray &obj, std::ostream &out)
{
    auto lay = obj.layout();

    out << "Label: " << obj.label << "\n"
        << "Words: " << octal(obj.words.size(), 1) << ", used " << octal(lay.end_off, 1) << "\n";

    out << "Entries:\n";
    for (const auto &e : lay.entries)
        out << "    " << std::left << std::setw(8) << text_to_utf8(e.name) << std::right << ' '
            << octal(e.info, 16) << "\n";

    out << "Header: head " << octal(lay.head_len, 1) << ", sym " << octal(lay.sym_len, 1)
        << ", word2 " << octal(lay.word2, 1) << ", debug " << octal(lay.debug_len, 1) << ", long "
        << octal(lay.long_len, 1) << ", cmd " << octal(lay.cmd_len, 1) << ", bss "
        << octal(lay.bss_len, 1) << ", const " << octal(lay.const_len, 1) << ", data "
        << octal(lay.data_len, 1) << ", set " << octal(lay.set_len, 1) << "\n";

    out << "Code:\n";
    for (unsigned i = 0; i < lay.cmd_len; i++) {
        Word w = obj.words[lay.cmd_off + i];
        out << ' ' << octal(i, 5) << "  " << octal(w, 16) << "  ";
        print_insn(obj, lay, unsigned(w >> 24), out);
        out << "\n" << std::string(26, ' ');
        print_insn(obj, lay, unsigned(w & 077777777), out);
        out << "\n";
    }

    out << "Constants:\n";
    for (unsigned i = 0; i < lay.const_len; i++)
        out << ' ' << octal(lay.cmd_len + i, 5) << "  " << octal(obj.words[lay.const_off + i], 16)
            << "\n";

    for (unsigned i = 0; i < lay.data_len + lay.set_len; i++)
        out << " data " << octal(i, 5) << "  " << octal(obj.words[lay.data_off + i], 16) << "\n";

    out << "Description table:\n";
    unsigned nrows = lay.head_len + lay.sym_len + lay.long_len;
    for (unsigned row = 0; row < nrows; row++) {
        Word w = obj.table_row(lay, row);
        out << ' ' << octal(row, 5) << "  " << octal(w, 16) << "  ";
        if (row >= lay.head_len && row < lay.head_len + lay.sym_len) {
            auto sym = Symbol::decode(w);
            out << "type " << octal(sym.type, 3) << " addr " << octal(sym.addr, 5) << "  "
                << row_name(obj, lay, row);
        } else {
            out << text_to_utf8(w);
        }
        out << "\n";
    }

    for (unsigned i = 0; i < lay.debug_len; i++)
        out << " debug " << octal(i, 5) << "  " << octal(obj.words[lay.debug_off + i], 16) << "\n";
}
