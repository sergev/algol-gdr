//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// With --check, a procedure checks its actual parameters on entry: their
// number (the word before args) and, from the tag of each, its kind and
// type against the formal (doc/codegen.md).
//
#include <stdint.h>

#include "algol.h"
#include "internal.h"

// Kinds of actuals, as the code generator tags them.
enum { A_VALUE, A_VARIABLE, A_ARRAY, A_PROCEDURE, A_LABEL, A_SWITCH };

static const char *const kind_names[] = { "a value",     "a variable", "an array",
                                          "a procedure", "a label",    "a switch" };
static const char *const type_names[] = { "integer", "real", "Boolean", "string", "bits", "none" };

static int numeric(int t)
{
    return t == T_INTEGER || t == T_REAL;
}

// spec: a kind (s a p l w) and a type (i r b s t n) for each formal.
void algol_check_actuals(void **args, int64_t n, const char *spec, const char *name)
{
    int64_t count = args ? ((const int64_t *)args)[-1] : 0;
    if (count != n)
        algol_error("Procedure %s called with %lld parameters instead of %lld", name,
                    (long long)count, (long long)n);
    for (int64_t k = 0; k < n; k++) {
        int64_t tag = (int64_t)(intptr_t)args[3 * k + 2];
        int kind    = (int)(tag >> 4);
        int type    = (int)(tag & 15);
        char want   = spec[2 * k];
        int ok;
        switch (want) {
        case 's':
            // A procedure is called at each access.
            ok = kind == A_VALUE || kind == A_VARIABLE || kind == A_PROCEDURE;
            break;
        case 'a':
            ok = kind == A_ARRAY;
            break;
        case 'p':
            ok = kind == A_PROCEDURE;
            break;
        case 'l':
            ok = kind == A_LABEL;
            break;
        default:
            ok = kind == A_SWITCH;
        }
        if (!ok)
            algol_error("Parameter %lld of %s is %s, not what the formal takes", (long long)k + 1,
                        name, kind <= A_SWITCH ? kind_names[kind] : "unknown");
        static const char types[] = "irbstn";
        int formal                = 0;
        while (types[formal] != spec[2 * k + 1])
            formal++;
        if (want == 'l' || want == 'w' || formal == 5)
            continue;
        if (formal == type || (numeric(formal) && numeric(type)))
            continue;
        if (type == 5 && (want == 'p' || kind == A_PROCEDURE))
            algol_error("Parameter %lld of %s is a procedure without a value, not %s",
                        (long long)k + 1, name, type_names[formal]);
        algol_error("Parameter %lld of %s is %s, not %s", (long long)k + 1, name,
                    type < 6 ? type_names[type] : "unknown", type_names[formal]);
    }
}
