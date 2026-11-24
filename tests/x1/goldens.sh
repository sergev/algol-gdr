#!/bin/sh
#
# Copy the examples of x1-algol-compiler to examples/ (encoding to tests/x1)
# and record their output under x1sim: NAME.out, frozen. Inputs: NAME.in from
# the example's NAME.sh, or written there (quinio, bulls_and_cows).
# pentominoWxH.in runs pentomino.a60. time and bulls_and_cows have no
# NAME.out: the time of day, the random secret. quine is left out: its output
# is not its source. output, input, helpers of tests/x1 are run too.
#
# Usage: goldens.sh [EXAMPLES [X1SIM]]
#
set -e
here=$(cd "$(dirname "$0")" && pwd)
dest=$(cd "$here/../../examples" && pwd)
examples=${1:-$here/../../../../Algol/x1-algol-compiler/examples}
x1sim=${2:-x1sim}
examples=$(cd "$examples" && pwd)
: "${X1ALGOL_LIB:=$examples/../library/x1algol.lib}"
export X1ALGOL_LIB

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for src in "$examples"/*.a60; do
    case $(basename "$src") in
    quine.a60) ;;
    encoding.a60) cp "$src" "$here"/ ;;
    *) cp "$src" "$dest"/ ;;
    esac
done
for sh in "$examples"/*.sh; do
    name=$(basename "$sh" .sh)
    sed -n '/<< *END_OF_INPUT/,/^END_OF_INPUT/p' "$sh" | sed '1d;$d' > "$dest/$name.in"
done

cd "$work"
for src in "$here"/*.a60 "$dest"/*.a60; do
    dir=$(dirname "$src")
    name=$(basename "$src" .a60)
    case $name in
    time | bulls_and_cows | revised_report) continue ;;
    pentomino)
        for in in "$dir"/pentomino?*.in; do
            run=$(basename "$in" .in)
            echo "$run"
            "$x1sim" "$src" < "$in" > "$dir/$run.out" || true
        done
        continue
        ;;
    esac
    in=$dir/$name.in
    [ -f "$in" ] || in=/dev/null
    echo "$name"
    "$x1sim" "$src" < "$in" > "$dir/$name.out" || true
done
