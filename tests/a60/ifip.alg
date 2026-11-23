_comment The input and output procedures of the IFIP modified report;
_begin _integer i, k; _real x; _integer _array a[1:2];
    ininteger(0, i); inreal(0, x); inreal(0, a[2]);
    outinteger(1, i); outreal(1, x); outreal(1, x × 2); outinteger(1, a[2]);
    outstring(1, ''sum:''); outterminator(1); outinteger(1, i + 1);
    outsymbol(1, ''abc'', 2); outsymbol(1, ''abc'', -65); outsymbol(1, ''абв'', 3);
    insymbol(0, ''xyz'', k); outinteger(1, k);
    insymbol(0, ''xyz'', k); outinteger(1, k);
    outinteger(1, length(''hello''));
    outinteger(1, maxint); outreal(1, maxreal); outreal(1, minreal); outreal(1, epsilon);
    outreal(1, 1 / 3)
_end
