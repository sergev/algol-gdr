# Symbols

The basic symbols of the language as the lexer gives them to the parser: the `Sym` codes of
src/lexer.h, how each is written in the source, and how messages name it (`to_string(Sym)`
in src/lexer.cpp). doc/encoding.md has the characters; doc/algol60.asdl the syntax built
from these symbols.

## Values

| Sym | Written | In messages | Token value |
|---|---|---|---|
| `IDENT` | a letter, then letters and digits; blanks inside are dropped | identifier | the name, UTF-8, as written |
| `INTEGER_CONST` | digits, up to 2⁶³ − 1 | integer constant | `int64_t` |
| `REAL_CONST` | `1.5`, `.5`, `1⏨3`, `⏨-2` | real constant | `double` |
| `BITS_CONST` | `'777'`, up to 16 octal digits | octal constant | the bits |
| `STRING_CONST` | `` `…' ``, `‘…’`, nested; `` '` `` an apostrophe | string | the text, UTF-8, the inner quotes kept |
| `COMMON` | `_common /NAME/ elements;`, the whole declaration | `_common` | the block: its name and elements |

A `_common` declaration is read whole by the lexer, so the parser gets one symbol for it.

## Operators

| Sym | Written | In messages |
|---|---|---|
| `PLUS`, `MINUS` | `+`, `-` | `'+'`, `'-'` |
| `TIMES` | `×`, `*` | `'*'` |
| `SLASH` | `/` | `'/'` |
| `INTDIV` | `÷`, `_div` | `_div` |
| `POWER` | `↑`, `^`, `**`, `_power` | `'**'` |
| `LT` | `<`, `_lt`, `_less` | `'<'` |
| `LE` | `≤`, `⩽`, `_le`, `_notgreater` | `'<='` |
| `EQ` | `=`, `_eq`, `_equal` | `'='` |
| `NE` | `≠`, `_ne`, `_notequal` | `'!='` |
| `GE` | `≥`, `⩾`, `_ge`, `_notless` | `'>='` |
| `GT` | `>`, `_gt`, `_greater` | `'>'` |
| `NOT` | `¬`, `_not` | `_not` |
| `AND` | `∧`, `&`, `_and` | `_and` |
| `OR` | `∨`, `_or` | `_or` |
| `IMPL` | `⊃`, `_impl` | `_impl` |
| `EQUIV` | `≡`, `_equiv` | `_equiv` |

## Delimiters

| Sym | Written |
|---|---|
| `LPAREN`, `RPAREN` | `(`, `)` |
| `LBRACKET`, `RBRACKET` | `[`, `]`; also `(/`, `/)` |
| `COMMA`, `COLON` | `,`, `:` |
| `SEMICOLON` | `;`, `.,` |
| `ASSIGN` | `:=` |

## Keywords

A keyword is `_` and a word in any case (`_begin`, `_BEGIN`), or the word underlined in the
publication form (`b̲e̲g̲i̲n̲`). Messages give it as `_begin`. Several words may give one symbol:

| Sym | Words |
|---|---|
| `GOTO` | `goto` (`g̲o̲ t̲o̲` in the publication form) |
| `IF`, `THEN`, `ELSE` | `if`, `then`, `else` |
| `FOR`, `DO`, `STEP`, `UNTIL`, `WHILE` | `for`, `do`, `step`, `until`, `while` |
| `BEGIN`, `END` | `begin`, `end` |
| `OWN`, `BOOLEAN`, `INTEGER`, `REAL` | `own`, `boolean`, `integer`, `real` |
| `STRING`, `BITS` | `string`, `bits` (Algol-GDR) |
| `ARRAY`, `SWITCH`, `PROCEDURE` | `array`, `switch`, `procedure` |
| `LABEL`, `VALUE` | `label`, `value` |
| `TRUE`, `FALSE` | `true`, `false` |
| `ALGOL`, `CODE` | `algol`, `code`: external procedures (Algol-GDR) |
| `READ`, `PRINT`, `PUNCH`, `WRITE` | `read`, `print`, `punch`, `write`: the formatted I/O statements of Algol-GDR |
| `DECODE`, `ENCODE` | `decode`, `encode` |
| `REWIND`, `BACKSPACE`, `ENDFILE` | `rewind`, `backspace`, `endfile` |

The operator words (`div`, `power`, `lt`, `and`, …) are in the tables above. `_comment`
starts a comment, which the lexer skips; it gives no symbol. `_read` and `_print` are
keywords; the standard procedures `read` and `print` are identifiers (doc/tables.md).

## Others

| Sym | Meaning |
|---|---|
| `ERROR` | a lexical error, already reported; the parser does not report it again |
| `END_OF_FILE` | no more lines: the end of the program |

`EOP` is the old end-of-program keyword: the parser reports it and stops reading there.
