#!/usr/bin/env python3
# Writes syntaxes/algol60.tmLanguage.json, the TextMate grammar of algc's Algol 60.
# Word lists follow doc/encoding.md, doc/symbols.md and doc/tables.md.
import json
import os

CONTROL = "begin end goto if then else for do step until while eop".split()
TYPES = ("own boolean integer real string bits array switch procedure label value "
         "algol code common").split()
CONSTANTS = "true false".split()
OPERATORS = ("and or not impl equiv div power lt le eq ne ge gt less greater equal "
             "notequal notgreater notless").split()
IO = "read print punch write decode encode rewind backspace endfile".split()
STANDARD = ("abs sign sqrt sin cos arctan ln exp entier length maxreal minreal epsilon "
            "maxint read print stop ininteger inreal insymbol outinteger outreal outstring "
            "outsymbol outterminator fault RESYM XEEN EVEN RANDOM TIMEOFDAY SUM NEWLINE "
            "NEWLIN NEWPAGE NEWPAG STOP PRINTTEXT PRSYM SPACE TAB NLCR FIXT ABSFIXT FLOT "
            "SETRANDOM LOAD LOADGO ASSIGN ATOS STOA").split()

UL = r"\x{0332}"
NOT_WORD = r"(?![\p{L}\p{N}])"


def underlined(word):
    """b̲e̲g̲i̲n̲ in any case, doubled underlines too."""
    parts = []
    for c in word:
        if c == " ":
            parts.append(r"\s*")
        else:
            parts.append("[%s%s]%s+" % (c.lower(), c.upper(), UL))
    return "".join(parts)


def keyword(words):
    """_word in any case, or the word underlined."""
    words = sorted(words, key=len, reverse=True)
    pub = [underlined("go to" if w == "goto" else w) for w in words]
    return "(?:_(?i:%s)%s|(?:%s)(?![\\p{L}\\p{N}]%s))" % (
        "|".join(words), NOT_WORD, "|".join(pub), UL)


def string(name, open_, close, escape=None):
    patterns = [{"include": "#" + name}]
    if escape:
        patterns.insert(0, {"name": "constant.character.escape.algol60", "match": escape})
    return {
        "name": "string.quoted.other.algol60",
        "begin": open_,
        "beginCaptures": {"0": {"name": "punctuation.definition.string.begin.algol60"}},
        "end": close,
        "endCaptures": {"0": {"name": "punctuation.definition.string.end.algol60"}},
        "applyEndPatternLast": True,
        "patterns": patterns,
    }


grammar = {
    "$schema": "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
    "name": "Algol 60",
    "scopeName": "source.algol60",
    "fileTypes": ["alg", "a60"],
    "patterns": [{"include": "#" + r} for r in (
        "comment", "end-comment", "string-backquote", "string-curly", "bad-string",
        "octal", "keywords", "standard", "number", "operator", "punctuation")],
    "repository": {
        "comment": {
            "name": "comment.block.algol60",
            "begin": keyword(["comment"]),
            "end": r";|\.,",
        },
        # After _end, the text up to ';', _end, _else or _eop.
        "end-comment": {
            "begin": keyword(["end"]),
            "beginCaptures": {"0": {"name": "keyword.control.algol60"}},
            "end": "(?=;|\\.,|%s)" % keyword(["end", "else", "eop"]),
            "contentName": "comment.block.algol60",
        },
        "string-backquote": string("string-backquote", "`", "['′]", "['′]`"),
        "string-curly": string("string-curly", "‘", "’"),
        "bad-string": {"patterns": [
            {"name": "invalid.illegal.string.algol60", "begin": "''", "end": "''"},
            {"name": "invalid.illegal.string.algol60", "begin": r"'\(", "end": r"'\)"},
            {"name": "invalid.illegal.string.algol60", "begin": '"', "end": '"'},
        ]},
        "octal": {"patterns": [
            {"name": "constant.numeric.octal.algol60", "match": r"['′][0-7]+['′]"},
            {"name": "invalid.illegal.quoted-keyword.algol60",
             "match": r"['′][A-Za-z]+['′]"},
        ]},
        "keywords": {"patterns": [
            {"name": "keyword.control.algol60", "match": keyword(CONTROL)},
            {"name": "storage.type.algol60", "match": keyword(TYPES)},
            {"name": "constant.language.algol60", "match": keyword(CONSTANTS)},
            {"name": "keyword.operator.word.algol60", "match": keyword(OPERATORS)},
            {"name": "keyword.other.io.algol60", "match": keyword(IO)},
            {"name": "invalid.illegal.keyword.algol60", "match": r"_[\p{L}\p{N}]+"},
        ]},
        "standard": {
            "name": "support.function.algol60",
            "match": r"(?<![\p{L}\p{N}%s])(?:%s)(?![\p{L}\p{N}%s])" % (
                UL, "|".join(sorted(set(STANDARD), key=len, reverse=True)), UL),
        },
        "number": {
            "name": "constant.numeric.algol60",
            "match": r"(?<![\p{L}\p{N}])(?:(?:\d+(?:\.\d+)?|\.\d+)(?:⏨[+-]?\d+)?|⏨[+-]?\d+)",
        },
        "operator": {"patterns": [
            {"name": "keyword.operator.assignment.algol60", "match": ":="},
            {"name": "punctuation.section.brackets.algol60", "match": r"\(/|/\)"},
            {"name": "keyword.operator.comparison.algol60", "match": "[<≤⩽=≠≥⩾>]"},
            {"name": "keyword.operator.logical.algol60", "match": "[¬∧&∨⊃≡]"},
            {"name": "keyword.operator.arithmetic.algol60",
             "match": r"\*\*|[×*/÷↑^+\-]"},
        ]},
        "punctuation": {"patterns": [
            {"name": "punctuation.terminator.statement.algol60", "match": r";|\.,"},
            {"name": "punctuation.separator.algol60", "match": "[,:]"},
        ]},
    },
}

path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "syntaxes", "algol60.tmLanguage.json")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w", encoding="utf-8") as f:
    json.dump(grammar, f, ensure_ascii=False, indent=2)
    f.write("\n")
