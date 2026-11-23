_comment Parameters as the Revised Report has them, section 4.7;
_begin _integer k, n, m; _real r; _integer _array ia[1:3];

    _comment Jensen's device: the term is evaluated at each access;
    _real _procedure sum(i, lo, hi, term); _value lo, hi;
        _integer i, lo, hi; _real term;
    _begin _real s; s := 0;
        _for i := lo _step 1 _until hi _do s := s + term;
        sum := s
    _end;

    _comment A real formal for an integer variable: the value is rounded
    when assigned;
    _procedure inc(x); _real x; x := x + 0.6;

    _comment An integer formal for a real variable;
    _procedure half(n); _integer n; n := n ÷ 2;

    _comment Calls through a formal procedure;
    _integer _procedure sq(n); _value n; _integer n; sq := n × n;
    _real _procedure apply(f, x); _real _procedure f; _real x; apply := f(x);

    _comment Formals without specifications;
    _procedure swap(a, b); _begin _real t; t := a; a := b; b := t _end;
    _real _procedure first(v); first := v[1];
    _procedure twice(p); _begin p; p _end;
    _procedure pass(q); twice(q);
    _procedure jump(l); _goto l;
    _procedure bump; k := k + 1;

    _comment An array and a label by value;
    _procedure zap(a); _value a; _integer _array a;
    _begin a[1] := 99; PRINT(a[1]) _end;
    _procedure go(l); _value l; _label l; _goto l;

    PRINT(sum(k, 1, 4, 1 / k), sum(k, 1, 4, k × k));
    n := 1; inc(n); r := 7.4; half(r);
    PRINT(NEWLINE, n, r);
    PRINT(NEWLINE, apply(sq, 3), apply(sq, 2.6));
    n := 1; m := 2; swap(n, m);
    ia[1] := 5; ia[2] := 6; ia[3] := 7;
    PRINT(NEWLINE, n, m, first(ia));
    k := 0; twice(bump); pass(bump);
    PRINT(NEWLINE, k);
    jump(L1);
    PRINT(`missed');
L1: zap(ia);
    PRINT(ia[1]);
    go(L2);
    PRINT(`missed');
L2: PRINT(NEWLINE, `done')
_end
