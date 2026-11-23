_comment THE PRIMES FROM 2 TO 100: THE SIEVE OF ERATOSTHENES;
_begin
    _boolean _array PRIME [2:100];
    _integer N, K;
    _for N:=2 _step 1 _until 100 _do
        PRIME [N] := _true;
    _for N:=2 _step 1 _until 100 _do
        _if PRIME[N] _then
            _for K:=N+N _step N _until 100 _do
                PRIME [K] := _false;
    _for N:=2 _step 1 _until 100 _do
        _if PRIME[N] _then
            PRINT (N);
_end
