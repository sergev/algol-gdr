#include "passes.h"

#include <string>

Stream run_pass(int n, Context &ctx, const Stream &input)
{
    (void)ctx;
    (void)input;
    switch (n) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        throw NotImplemented("pass " + std::to_string(n) + " not implemented yet");
    default:
        throw std::invalid_argument("no pass " + std::to_string(n));
    }
}
