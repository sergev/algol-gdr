_comment The for statement, Revised Report 4.6;
_begin _integer i, k, n; _integer _array a[1:10];
    _integer _procedure next; _begin n := n + 1; next := n _end;

    _comment A subscripted controlled variable: its subscripts are
    evaluated at each assignment;
    i := 1;
    _for a[i] := 1, 2, 3 _do i := i + 1;
    PRINT(a[1], a[2], a[3], i);
    i := 1;
    _for a[i] := 1 _step 2 _until 10 _do _begin i := i + 1; a[i] := a[i - 1] _end;
    PRINT(NEWLINE, i, a[i]);

    _comment Each element is evaluated when its turn comes, the step and
    the limit at each round;
    n := 0;
    _for k := next, next × 10 _do PRINT(NEWLINE, k);
    PRINT(NEWLINE);
    _for k := 1 _step k _until 20 _do PRINT(k);
    PRINT(NEWLINE);
    k := 0;
    _for k := k + 3 _while k < 10 _do PRINT(k);

    _comment A goto out of the body, also out of an inner block with an
    array, and out of nested loops;
    _for k := 1 _step 1 _until 100 _do
        _begin _real _array t[1:k]; t[k] := k; _if t[k] × t[k] > 50 _then _goto out _end;
out: PRINT(NEWLINE, k);
    _for i := 1 _step 1 _until 10 _do
        _for k := 1 _step 1 _until 10 _do
            _if i × k = 42 _then _goto found;
found: PRINT(i, k)
_end
