_comment Own variables and arrays, Revised Report 5: kept between entries of
their block, also in recursion;
_begin _integer i, n;
    _integer _procedure count; _begin _own _integer c; c := c + 1; count := c _end;

    _comment The bounds of an own array are evaluated at the first entry;
    _procedure fib(k); _value k; _integer k;
    _begin _own _integer _array f[0:n]; _own _boolean init;
        _if ¬init _then _begin f[0] := 0; f[1] := 1; init := _true _end;
        _if k > 1 _then _begin _if f[k] = 0 _then _begin fib(k - 1); f[k] := f[k - 1] + f[k - 2] _end _end;
        print(f[k], NEWLINE)
    _end;

    _comment Recursive calls share one own variable;
    _procedure deep(k); _value k; _integer k;
    _begin _own _integer depth; depth := depth + 1;
        _if k > 0 _then deep(k - 1) _else print(depth, NEWLINE)
    _end;

    _comment An own variable of an inner block of the program;
    _for i := 1 _step 1 _until 3 _do
    _begin _own _real s; s := s + 0.5; print(s, NEWLINE) _end;
    print(count, count, count, NEWLINE);
    print(NEWLINE); n := 10; fib(10);
    print(NEWLINE); n := 2; fib(7);
    print(NEWLINE); deep(4); deep(2);
    print(count, NEWLINE)
_end
