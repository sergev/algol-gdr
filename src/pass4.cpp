#include "pass4.h"

#include "translated.h"

unsigned pass4_code(Machine &m, unsigned pc);

Stream pass4(Context &ctx, const Stream &input)
{
    // The stream of the previous pass is in the workspace and on the drums,
    // where it left it.
    (void)input;
    TranslatedPass pass(ctx, pass4_code);
    return pass.run(023523, 01633);
}
