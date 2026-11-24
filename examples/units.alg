_comment A program with _algol procedures of another unit, units.unit.alg;
_algol: _real sum;
_algol: show;
_begin _integer k; _real _array a[1:3];
    a[1] := 1; a[2] := 2; a[3] := 4;
    show(sum(k, 1, 3, a[k]));
    print(sum(k, 1, 10, k), NEWLINE)
_end
