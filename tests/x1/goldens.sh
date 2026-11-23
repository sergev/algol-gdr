#!/bin/sh
#
# Copy the examples of x1-algol-compiler here and record their output under
# x1sim: NAME.out, frozen. Inputs: NAME.in from the example's NAME.sh, or
# written here (quinio, bulls_and_cows). pentominoWxH.in runs pentomino.a60.
# time and bulls_and_cows have no NAME.out: the time of day, the random secret.
#
# Usage: goldens.sh [EXAMPLES [X1SIM]]
#
set -e
here=$(cd "$(dirname "$0")" && pwd)
examples=${1:-$here/../../../../Algol/x1-algol-compiler/examples}
x1sim=${2:-x1sim}
examples=$(cd "$examples" && pwd)
: "${X1ALGOL_LIB:=$examples/../library/x1algol.lib}"
export X1ALGOL_LIB

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cp "$examples"/*.a60 "$here"/
for sh in "$examples"/*.sh; do
    name=$(basename "$sh" .sh)
    sed -n '/<< *END_OF_INPUT/,/^END_OF_INPUT/p' "$sh" | sed '1d;$d' > "$here/$name.in"
done

cd "$work"
for src in "$here"/*.a60; do
    name=$(basename "$src" .a60)
    case $name in
    time | bulls_and_cows) continue ;;
    pentomino)
        for in in "$here"/pentomino?*.in; do
            run=$(basename "$in" .in)
            echo "$run"
            "$x1sim" "$src" < "$in" > "$here/$run.out" || true
        done
        continue
        ;;
    esac
    in=$here/$name.in
    [ -f "$in" ] || in=/dev/null
    echo "$name"
    "$x1sim" "$src" < "$in" > "$here/$name.out" || true
done
