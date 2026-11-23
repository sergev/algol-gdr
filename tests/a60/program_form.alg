_comment The program form of the Revised Report: unsigned integers as
labels, the parameter delimiter ) letter string: (, no _eop at the end;
_begin _integer i;
    _switch s := 20, 30, 0040;
    _procedure jump(n) to: (l); _value n; _integer n; _label l;
        _if n > 2 _then _goto l;
    _real _procedure ratio(a) over: (b); _value a, b; _real a, b;
        ratio := a / b;
    i := 0;
10: i := i + 1;
    _goto s[i];
20: PRINT(`twenty', i);
    _goto 10;
30: PRINT(NEWLINE, `thirty', i);
    _goto 010;
40: PRINT(NEWLINE, `forty', i);
    jump(i) to: (50);
    PRINT(`missed');
50: PRINT(NEWLINE, ratio(3) over: (4), ratio(1, 8))
_end
