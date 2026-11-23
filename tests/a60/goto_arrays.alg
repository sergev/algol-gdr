_comment Arrays of the blocks and procedures left by a goto are freed: 2000 rounds
of 8 MB arrays each, run in 1 GB;
_begin _integer i, j;
    _procedure p; _begin _real _array a[1:1000000]; a[1] := i; _goto again _end;
    i := 0;
again: i := i + 1;
    _if i < 2000 _then p;
    j := 0;
    _begin _real _array c[1:10];
        _procedure q; _begin _real _array a[1:1000000]; a[1] := j; _goto more _end;
        c[1] := 5;
more:   j := j + 1;
        _if j < 2000 _then _begin _real _array b[1:1000000]; b[1] := j; q _end;
        print(i, j, c[1], NEWLINE)
    _end
_end
