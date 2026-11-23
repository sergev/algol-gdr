#include "passes.h"

#include <string>

#include "pass1.h"
#include "pass2.h"
#include "pass3.h"
#include "pass4.h"
#include "pass5.h"
#include "pass6.h"
#include "printm.h"

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
        return Pass1(ctx).run();
    case 5:
        return pass5(ctx, input);
    case 2:
        return pass2(ctx, input);
    case 3:
        return pass3(ctx, input);
    case 4:
        return pass4(ctx, input);
    default:
        throw std::invalid_argument("no pass " + std::to_string(n));
    }
}

bool compile(Context &ctx, StdArray &object)
{
    bool made   = false;
    auto stream = run_pass(1, ctx, {});
    if (!ctx.finished) {
        stream = run_pass(2, ctx, stream);
        if (!ctx.flgex) {
            if (!ctx.options.check_mode)
                stream = run_pass(3, ctx, stream);
            stream = run_pass(4, ctx, stream);
            stream = run_pass(5, ctx, stream);
            object = pass6(ctx, stream);
            made   = !object.words.empty();
            if (made && ctx.listing)
                ctx.listing->print(summary_line(ctx));
            // G44504; after errors in passes 1-2 the driver skips it. Then
            // with *FULL the object listing of A/PRINTM.
            if (ctx.listing) {
                ctx.listing->finish();
                if (made && ctx.options.full_listing)
                    for (const auto &row : printm_rows(object))
                        ctx.listing->print(row);
            }
        }
    }
    return made && !ctx.flgex;
}
