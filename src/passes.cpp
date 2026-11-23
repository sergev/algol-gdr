//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "passes.h"

#include <string>

#include "pass1.h"
#include "pass2.h"

void blank_line(const Context &ctx)
{
    if (ctx.listing && ctx.options.list)
        ctx.listing->blank();
}

Stream run_pass(int n, Context &ctx, const Stream &input)
{
    switch (n) {
    case 1:
        // The driver's banner, then pass 1.
        if (ctx.listing && ctx.options.list) {
            ctx.listing->print("A L G O L - Г Д P");
            ctx.listing->print("    (7.01.82)");
        }
        {
            Stream out = Pass1(ctx).run();
            ctx.dlist.clear();
            for (unsigned a = 04400; a < 06000; a++)
                ctx.dlist.push_back(ctx.ws[a]);
            return out;
        }
    case 2:
        return pass2(ctx, input);
    default:
        throw std::invalid_argument("no pass " + std::to_string(n));
    }
}

bool compile(Context &ctx, Stream &stream)
{
    stream = run_pass(1, ctx, {});
    if (ctx.finished)
        return false;
    stream = run_pass(2, ctx, stream);
    if (ctx.flgex)
        return false;
    // G44504; after errors in passes 1-2 the driver skips it.
    if (ctx.listing)
        ctx.listing->finish();
    return true;
}
