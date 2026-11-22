#include "passes.h"

#include <ostream>
#include <string>

#include "pass5.h"

void blank_line(const Context &ctx)
{
    if (ctx.listing && !ctx.options.no_listing && ctx.options.setftn_one)
        *ctx.listing << '\n';
}

Stream run_pass(int n, Context &ctx, const Stream &input)
{
    switch (n) {
    case 5:
        return pass5(ctx, input);
    case 1:
    case 2:
    case 3:
    case 4:
        throw NotImplemented("pass " + std::to_string(n) + " not implemented yet");
    default:
        throw std::invalid_argument("no pass " + std::to_string(n));
    }
}
