# Algol 60 for VS Code

Syntax highlighting for Algol 60 as `algc` reads it (doc/encoding.md), in `.alg` and `.a60`
files:

- keywords in both forms: `_begin` (any case) and underlined `b̲e̲g̲i̲n̲` (`g̲o̲ t̲o̲` too);
  control words, declarators, operator words (`_and`, `_le`), I/O statements, `_true`/`_false`;
- comments: `_comment … ;` and the text after `_end` up to `;`, `_end` or `_else`;
- strings `` `…' `` and `‘…’`, nested, with the `` '` `` apostrophe;
- numbers with `⏨`, octal constants `'777'`, the symbols `× ÷ ↑ ≤ ≥ ≠ ¬ ∧ ∨ ⊃ ≡`, `:=`;
- standard identifiers in their documented case (`sqrt`, `outreal`, `PRINTTEXT`, `print`);
- errors: keywords in quotes (`'BEGIN'`), strings in `''…''`, `'('…')'`, `"…"`, unknown
  `_words`.

A monospace font draws the combining underline U+0332 as a cell of its own, so `b̲e̲g̲i̲n̲`
would show as `b_e_g_i_n_`. The extension hides the U+0332 characters and underlines the
letters before them instead; the text itself is unchanged. The cursor still steps over
each hidden character. The setting `algol60.underlineKeywords` (on by default) turns
this off.

It also matches `_begin` with `_end`, indents after `_begin`, auto-closes `` ` `` with `'`,
and turns off VS Code's warnings about "ambiguous" characters such as `×` and `‘`.

## Installation

Link this directory into the VS Code extensions directory under the name
`PUBLISHER.NAME-VERSION`, then restart VS Code (or run *Developer: Reload Window*):

```
ln -s "$PWD/vscode" ~/.vscode/extensions/algc.algol60-0.1.0
```

Run this from the top of the repository. The extension then follows the repository: after
a `git pull`, reload the window. Check that it is installed:

```
code --list-extensions | grep algol60
```

For other builds of VS Code the directory differs: `~/.vscode-oss/extensions` (Code - OSS,
VSCodium), `~/.vscode-insiders/extensions` (Insiders), `~/.vscode-server/extensions` (a
remote host over SSH). On Windows use a copy instead of a link, in
`%USERPROFILE%\.vscode\extensions\algc.algol60-0.1.0`.

To remove it, delete the link and reload.

If another extension also claims `.alg`, pick *Algol 60* in the language box at the right
of the status bar, or add to settings.json:

```json
"files.associations": { "*.alg": "algol60", "*.a60": "algol60" }
```

## Files

- `package.json` — the extension manifest: the language `algol60` and its file types.
- `extension.js` — the underlines of the publication form (no build step: plain
  JavaScript).
- `language-configuration.json` — brackets, comments, quotes, words, indentation.
- `syntaxes/algol60.tmLanguage.json` — the TextMate grammar, generated; do not edit it.
- `grammar.py` — writes the grammar. It holds the lists of keywords and standard
  identifiers; after changing it, run `python3 vscode/grammar.py` and reload the window.

The scopes are the usual TextMate ones (`keyword.control`, `storage.type`,
`support.function`, `comment.block`, `string.quoted.other`, `invalid.illegal`, all ending
in `.algol60`), so any color theme applies. To change a color, use
`editor.tokenColorCustomizations` with `textMateRules` for a scope, for example
`keyword.operator.word.algol60`. *Developer: Inspect Editor Tokens and Scopes* shows the
scope under the cursor.
