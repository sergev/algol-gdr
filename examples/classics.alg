_comment Classic programs: Ackermann's function, Euclid's algorithm, the
eight queens, the towers of Hanoi, the sieve of Eratosthenes;
_begin _integer count, i, j, primes;
    _integer _array col[1:8]; _boolean _array row[1:8], up[2:16], down[-7:7];
    _boolean _array sieve[2:100];

    _integer _procedure ack(m, n); _value m, n; _integer m, n;
        ack := _if m = 0 _then n + 1
               _else _if n = 0 _then ack(m - 1, 1)
               _else ack(m - 1, ack(m, n - 1));

    _integer _procedure gcd(a, b); _value a, b; _integer a, b;
        gcd := _if b = 0 _then a _else gcd(b, a - a ÷ b × b);

    _comment Queens in columns c to 8: the solutions counted;
    _procedure place(c); _value c; _integer c;
    _begin _integer r;
        _if c > 8 _then count := count + 1
        _else _for r := 1 _step 1 _until 8 _do
            _if ¬row[r] ∧ ¬up[r + c] ∧ ¬down[r - c] _then
            _begin
                row[r] := up[r + c] := down[r - c] := _true;
                place(c + 1);
                row[r] := up[r + c] := down[r - c] := _false
            _end
    _end;

    _integer _procedure moves(n); _value n; _integer n;
        moves := _if n = 0 _then 0 _else 2 × moves(n - 1) + 1;

    outinteger(1, ack(2, 3)); outinteger(1, ack(3, 3));
    outinteger(1, gcd(1071, 462));
    count := 0; place(1); outinteger(1, count);
    outinteger(1, moves(10));
    _for i := 2 _step 1 _until 100 _do sieve[i] := _true;
    _for i := 2 _step 1 _until 10 _do
        _if sieve[i] _then
            _for j := i × i _step i _until 100 _do sieve[j] := _false;
    primes := 0;
    _for i := 2 _step 1 _until 100 _do _if sieve[i] _then primes := primes + 1;
    outinteger(1, primes)
_end
