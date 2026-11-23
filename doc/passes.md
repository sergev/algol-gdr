# Passes of algc

How `algc` turns an Algol file into a program: the passes, what each takes and gives, and
where each stops on errors. The passes of the original BESM-6 compiler (the driver at 01614,
`AG13666`…`AG30257`, their drum streams) are described in this file on the `besm6` branch.

## Overview

```
main.cpp     options                                       Options (options.h)
driver.cpp   compile: for each .alg or .a60 file, translate
  source.cpp   read_source: the file, UTF-8 checked          std::string
  frontend.cpp front_end:
    lexer.cpp    symbols, one at a time                      Token (lexer.h)
    parser.cpp   the syntax tree                             syn::Program (syntax.h)
    checker.cpp  scopes, types, conversions                  ast::Program (ast.h)
  codegen.cpp  LLVM IR in text, through ir.cpp               NAME.ll
  clang        -O1 by default: object, assembly or a program linked with libalgol.a and -lm
```

Each pass works on the whole result of the one before; only the lexer and the parser run
together (the parser asks the lexer for the next symbol). Options stop the chain early:

| Option | Stops after | Output |
|---|---|---|
| `--syntax-only` | the checker | messages; with `--list` the listing |
| `--dump-ast` | the checker | the AST in YAML on stdout (`ast.cpp`, after doc/ast.asdl) |
| `--emit-llvm` | the code generator | `NAME.ll` |
| `-S`, `-c` | clang | `NAME.s`, `NAME.o` |
| none | the link | the program `NAME` |

Messages go to stderr as `file:line:column: error: …` (or `warning:`) through one
`Diagnostics` (`diag.h`), which counts the errors. After the checker, a file with an error
gets no code: `front_end` returns nothing, and `algc` exits with status 1.

## Source

`read_source` (`source.cpp`) reads the file and checks that it is UTF-8. The lexer splits it
into lines (dropping a CR before LF) and decodes each into code points when it gets there.
doc/encoding.md has the characters and symbols.

## Lexer

`Lexer::next` (`lexer.cpp`) gives the next symbol as a `Token`: its `Sym`, its position
(line and column), and its value: the text of an identifier or a string, an integer, a real,
the bits of an octal constant, or the block of a `_common` declaration, which the lexer reads
whole.

- The form is decided once, at the start: an underlined letter anywhere in the file makes it
  the publication form.
- Blanks are skipped outside strings and `_keywords`; `′` becomes `'`.
- Comments (`_comment …;` and the text after `_end`) are skipped here, so the parser never
  sees them.
- A lexical error is reported here and gives an `ERROR` symbol; the parser does not report it
  again.
- With `--list`, each line is handed to the listing as the lexer starts reading it
  (`on_line`), so the listing shows the lines in the order they are read.

## Parser

`parse` (`parser.cpp`) is a recursive descent over the symbols, one symbol of lookahead and
more where needed (`peek`: a label is an identifier or a number before `:`). It builds the
syntax tree of `syntax.h`, after doc/algol60.asdl: names are still names, and nothing is
typed.

- The program: `_code:` and `_algol:` lists of external procedures, `_common` declarations,
  then a block or compound statement; or procedures alone, a unit for separate compilation.
  The end of the file ends it.
- Expressions by priority, from the lowest: `≡`, `⊃`, `∨`, `∧`, `¬`, the relations, `+ -`,
  `× / ÷`, `↑`; all binary operators are left-associative. A conditional expression as an
  operand must be in parentheses.
- Every statement gets a position: its line, and its number among the statements started on
  that line (`SrcLoc`), which the AST keeps.
- `_then _if … _else` is read with the `_else` on the inner `_if`, with a warning.
- After a syntax error the parser skips to the end of the statement (`;`, `_else` or the
  `_end` of its level) or past the declaration, leaves an `Error` node and goes on. After 30
  errors, or at an unexpected end of the file, it stops.

## Checker

`check` (`checker.cpp`) resolves the names of the syntax tree and checks their uses. It runs
also after syntax errors, on what the parser built, so one run reports both kinds. It gives
the AST of `ast.h`, after doc/ast.asdl:

- Every declaration gets an id unique in the program, and every use refers to it: the code
  generator needs no scope rules.
- A block declares its names, labels too, before its statements are checked, so they are
  visible in the whole block. The bounds of its arrays are checked in the enclosing scope.
  Then come its switches and procedures, its `_common` blocks, and its statements.
- Types are checked, and implicit conversions are made explicit (`Convert`). An expression
  with an error has type `none`, which keeps the operators above it from reporting again.
- A formal parameter without a specification gets its kind and type from its uses in the
  procedure body (`Usage`): called, subscripted, jumped to, a condition, an operand of `÷`
  (doc/deviations.md has the rules).
- Standard identifiers, in their documented case, unless declared (`SQRT` is undeclared,
  with a hint to `sqrt`): the functions (`sqrt`, `entier`, …),
  Algol-GDR's (`PRINT`, `READ`, `NEWLINE`, …), the IFIP procedures, and the X1 library.
  `print` is X1's in the publication form and Algol-GDR's in the `_keyword` form; `read` is
  X1's function, `READ` Algol-GDR's statement. Each becomes a `StdFunc`, a `StdProc` or an `Io` node with its
  arguments checked.
- A function that never assigns its result gets a warning.

## Code generator

`codegen` (`codegen.cpp`) turns the AST into LLVM IR text through the builder of `ir.h`
(after doc/ir.asdl). doc/codegen.md has the representation: data, frames, calling
convention, thunks, labels, arrays and the runtime names. Its steps:

1. `declare`: storage for every name, before any code: globals for the outer block and
   `own` variables, frame slots for the others, functions for procedures and switches.
2. `scan`: the labels reached from other functions (by a switch, a parameter, a thunk or a
   `goto` from a nested procedure). Their functions `setjmp` at entry (`far_entry`).
3. `statement`: the code of the program block, in `algol_main` (in a unit, `algol_unit`).
4. `procedure` for each procedure, `switch_function` for each switch, then the `_common`
   globals.

With `--check` it adds the run-time checks (doc/codegen.md, Checks).

## Clang and the runtime

The driver (`driver.cpp`) writes the IR of each Algol file to a temporary `NAME.ll` (kept with
`--save-temps`) and runs `clang` (or `$ALGC_CLANG`) with `-O1`, or the `-O` given, and
`--target`:

- `-c` or `-S`: one object or assembly file per input.
- Otherwise one link of all inputs (`.alg`, `.a60`, `.ll`, `.s`, `.o`, `.a`) with
  `libalgol.a` and `-lm`. The runtime is found in `../runtime` or `../lib` from `algc` (for
  `--target=T`, in `T/` there), or in `$ALGC_RUNTIME`.

The runtime (`runtime/`, C) has `main`, which calls the program's `algol_main` and then ends
the output; the input and output of Algol-GDR (`print.c`, `read.c`, `format.c`), IFIP
(`ifip.c`) and the X1 (`x1.c`); strings, arrays, arithmetic and the checks.
