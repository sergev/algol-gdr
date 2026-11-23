#include "pass3.h"

#include "translated.h"

unsigned pass3_code(Machine &m, unsigned pc);

Stream pass3(Context &ctx, const Stream &input)
{
    // The stream of the previous pass is in the workspace and on the drums,
    // where it left it.
    (void)input;
    TranslatedPass pass(ctx, pass3_code);
    return pass.run(022162, 01632);
}
