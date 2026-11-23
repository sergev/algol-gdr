#include "translated.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "listing.h"

namespace {

constexpr unsigned STACK    = 053401; // M17 when the driver calls a pass
constexpr unsigned RETURN   = 077777; // M13: a jump there ends the run
constexpr unsigned REGIM    = 00523;  // *REGIM//: bit 48 setftn:one, bit 47 setftn:long
constexpr unsigned FLGEX    = 00702;
constexpr unsigned NOLISFL  = 00705;
constexpr unsigned DATED    = 01730;  // the header with the date is printed
constexpr unsigned ASMPRINT = 012000; // a row: first and last word on the stack, indent in A
constexpr unsigned CURSOR_W = 02001;

} // namespace

TranslatedPass::TranslatedPass(Context &ctx, Machine::Code code) : ctx_(ctx), code_(code)
{
    m.drums = &ctx.ws;
    m.load_image();
    for (unsigned i = 1; i < 16; i++)
        m.M[i] = ctx.regs[i] & 077777;
    m.M[Machine::SP] = STACK;
    m.mem[FLGEX]     = ctx.flgex;
    m.mem[NOLISFL]   = ctx.options.no_listing;
    m.mem[CHECKFL]   = ctx.options.check();
    m.mem[REGIM]     = (ctx.options.setftn_one ? 04000000000000000 : 0) |
                   (ctx.options.setftn_long ? 02000000000000000 : 0);
    m.mem[DATED]        = ctx.options.no_listing ? 0 : 06000000000000000;
    m.natives[ASMPRINT] = [this](Machine &mm) {
        mm.M[Machine::SP] = (mm.M[Machine::SP] - 1) & 077777;
        unsigned last     = mm.mem[mm.M[Machine::SP]] & 077777;
        mm.M[Machine::SP] = (mm.M[Machine::SP] - 1) & 077777;
        unsigned first    = mm.mem[mm.M[Machine::SP]] & 077777;
        unsigned indent   = mm.acc & 0177;
        if (ctx_.listing && last >= first)
            ctx_.listing->row(std::string(indent, ' ') +
                              listing_text(&mm.mem[first], 0, 6 * (last - first + 1)));
        mm.set_m(14, 0);
    };
}

void TranslatedPass::write_stream(const Stream &s, unsigned entry, Word cursor, bool keep_cursors)
{
    Word saved[2]   = { m.mem[CURSOR_W - 1], m.mem[CURSOR_W] };
    m.mem[CURSOR_W] = cursor;
    for (const Item &item : s) {
        m.acc = item.bits;
        m.set_m(13, RETURN);
        m.run(code_, entry);
    }
    if (keep_cursors) {
        m.mem[CURSOR_W - 1] = saved[0];
        m.mem[CURSOR_W]     = saved[1];
    }
}

Stream TranslatedPass::run(unsigned entry, unsigned ret)
{
    std::vector<std::pair<unsigned, Word>> writes;
    m.on_entry = [&](Machine &mm, unsigned addr) { writes.emplace_back(addr, mm.acc); };
    m.set_m(13, ret);
    // ALGOL_TRACE=file: address, half and A before each instruction.
    // ALGOL_COVERAGE=file: the halves run, appended.
    std::FILE *tf = nullptr;
    if (const char *name = std::getenv("ALGOL_TRACE"))
        tf = std::fopen(name, "w");
    std::vector<bool> run_halves;
    const char *cov_name = std::getenv("ALGOL_COVERAGE");
    if (cov_name)
        run_halves.resize(2 * Machine::MEMSIZE);
    if (tf || cov_name)
        m.trace = [tf, &run_halves](const Machine &mm, unsigned addr, bool right) {
            if (tf)
                std::fprintf(tf, "%05o %c %016llo\n", addr, right ? 'R' : 'L',
                             (unsigned long long)mm.acc);
            if (!run_halves.empty())
                run_halves[2 * addr + right] = true;
        };
    unsigned pc = m.run(code_, entry);
    m.trace     = nullptr;
    if (tf)
        std::fclose(tf);
    if (cov_name) {
        if (std::FILE *cf = std::fopen(cov_name, "a")) {
            for (unsigned i = 0; i < run_halves.size(); i++)
                if (run_halves[i])
                    std::fprintf(cf, "%05o %c\n", i / 2, i % 2 ? 'R' : 'L');
            std::fclose(cf);
        }
    }
    m.on_entry = nullptr;
    if (pc != ret)
        throw std::runtime_error("translated pass left at " + std::to_string(pc));

    for (unsigned a = Workspace::LOW_BEGIN; a < Workspace::LOW_END; a++)
        ctx_.ws[a] = m.mem[a];
    for (unsigned a = Workspace::COMTAB; a < Workspace::COMTAB_END; a++)
        ctx_.ws[a] = m.mem[a];
    for (unsigned i = 1; i < 16; i++)
        ctx_.regs[i] = m.M[i];
    ctx_.flgex = unsigned(m.mem[FLGEX]);

    Stream out;
    bool backward = false;
    for (auto &[addr, value] : writes) {
        backward = addr != 027126;
        out.push_back(Item{ uint32_t(value & 077777777) });
    }
    if (backward)
        std::reverse(out.begin(), out.end());
    return out;
}
