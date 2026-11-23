#include "passes.h"

#include <string>

#include "pass1.h"
#include "pass2.h"
#include "pass5.h"

void blank_line(const Context &ctx)
{
    if (ctx.listing && !ctx.options.no_listing)
        ctx.listing->blank();
}

Stream run_pass(int n, Context &ctx, const Stream &input)
{
    switch (n) {
    case 1:
        // The driver's banner, then pass 1.
        if (ctx.listing && !ctx.options.no_listing) {
            ctx.listing->print("A L G O L - Г Д P");
            ctx.listing->print("    (7.01.82)");
        }
        return Pass1(ctx).run();
    case 5:
        return pass5(ctx, input);
    case 2:
        return pass2(ctx, input);
    case 3:
    case 4:
        throw NotImplemented("pass " + std::to_string(n) + " not implemented yet");
    default:
        throw std::invalid_argument("no pass " + std::to_string(n));
    }
}
