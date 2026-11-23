_code:_real rndm;
_algol:BOZP;
_algol:YBYB;
_begin
    _real _array a[1 : 10];
    _integer i;

    _for i := 1 _step 1 _until 10 _do
        a[i] := rndm(-1);

    _print(`(10f7.3)') a;

    _comment Расположение элементов вектора по возрастанию;
    BOZP(a, 10);
    _print(`(10f7.3)') a;

    _comment Расположение элементов вектора по убыванию;
    YBYB(a, 10);
    _print(`(10f7.3)') a;
_end
_eop
