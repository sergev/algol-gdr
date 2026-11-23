_comment A unit of procedures compiled alone, linked with units.alg;
_real _procedure sum(i, lo, hi, term); _value lo, hi; _integer i, lo, hi; _real term;
_begin _real s; s := 0;
    _for i := lo _step 1 _until hi _do s := s + term;
    sum := s
_end;

_comment It calls its sibling;
_procedure show(x); _value x; _real x;
_begin _integer n; PRINT(x, sum(n, 1, 2, x × n)) _end;
