_code:_integer xeen,available;
_code:draw;
_begin
    _comment L. Meertens, R 1082, LM 010964/9219.
    QUINIO (GO-MOKU). Zie voor de spelregels aparte beschrijving.:
    Indien X1 moet beginnen, d25 = 0 maken, anders d25 = 1 maken.
    Inbrengen van zet: x-coordinaat binair in d9 t/m d5,
    y-coordinaat in d4 t/m d0.
    Bijv. 11-7 wordt in d9 t/m d0 (XEEN(1023)
    genoteerd als 01011 00111.
    Doorstarten met BVA. De zet wordt uitgetypt,
    en de machine stopt weer.
    Indien nu blijkt dat men zich vergist heeft,
     dan kan men met negatief consolewoord doorstarten,
    waarna opnieuw gelegenheid geboden wordt een zet
    in te brengen. Wordt met positief consolewoord
    doorgestart, dan wordt de zet uitgevoerd en
    bezint de X1 zich op zijn volg'end'e zet.
    Is bijgeval helemaal geen zet uitgetypt,
    dan correspondeert de stand van de tien rechter-
    schakelaars niet met een correcte zet.
    Men kan alsnog een betere zet inbrengen;

    _integer k, j, av, tn, nt, vnl, x, y, sw, wm, kleur, znr, km;
    _boolean klaar;
    _integer _array DP[-6 : +6, 0 : 2], Z[1 : 6];

    _integer _procedure div(a, b);
        _value a, b;
        _integer a, b;
    div := entier(a/b);

    _procedure NLCR;
    _write (54,`(/)');

    _integer _procedure SUM(a, b, c, d);
        _value b, c;
        _integer a, b, c;
        _real d;
    _begin
        _real t;
        t := 0;
        _for a := b _step 1 _until c _do
            t := t + d;
        SUM := entier(t)
    _end;

    _boolean _procedure minim(x, p);
        _value p;
        _integer x, p;
    _if p < x _then
        _begin
            x := p;
            minim := _true
        _end
    _else
        minim := _false;

    _boolean _procedure maxim(x, p);
        _value p;
        _integer x, p;
    _if p > x _then
        _begin
            x := p;
            maxim := _true
        _end
    _else
        maxim := _false;

    nt := div(entier(sqrt((available - 1000) / 36) - 1), 2) * 2 + 1;
    minim(nt, 19);
    av := nt * nt;
    tn := 1 + div(nt, 2);
    NLCR;
    _write(54, `(13HField size = I2/23HO - Human, X - Computer)') nt;

    _begin
        _integer _array RNUM, RLANG, RVRY[1 : av, 0 : 1, 1 : 4],
            RVAL[1 : av, 1 : 4], TEMPO, WT[1 : av, 0 : 1], AX,
            AY[1 :av], BORD, AVNL[1 : nt, 1 : nt];

        _procedure CORR(x0, y0);
            _value x0, y0;
            _integer x0, y0;
        _begin
            _integer x, y, dx, dy, min, max, q, k, j, b, t, d, w,
                      xdx, ydy, zdz;
            dx := +1;
            dy := -1;
            klaar := _false;
            _for q := 1, 2, 3, 4 _do
            _begin
                zdz := xdx := (x0 - tn) * dx;
                ydy := (y0 - tn) * dy;
                minim(zdz, ydy);
                min := 3 - tn - zdz;
                max := - min - xdx - ydy;
                maxim(min, -6);
                minim(max, +6);
                _if min > max _then
                    _goto NOR;
                _for k := 0, 1, 2 _do
                    DP[min, k] := 0;
                _for k := -2 _step 1 _until +2 _do
                _begin
                    b := BORD[x0 + (min + k) * dx, y0 + (min + k) * dy];
                    DP[min, b] := DP[min, b] + 1
                _end;
                _for k := min + 1 _step 1 _until max _do
                _begin
                    _if DP[k - 1, kleur] = 5 _then
                        _goto WIN;
                    _for b := 0, 1, 2 _do
                        DP[k, b] := DP[k - 1, b];
                    _for j := -3, +2 _do
                    _begin
                        b := BORD[x0 + (k + j) * dx, y0 + (k + j) * dy];
                        DP[k, b] := DP[k, b] + sign(j)
                    _end
                _end;
                _if DP[max, kleur] = 5 _then
WIN:            _begin
                    klaar := _true;
                    _goto UIT
                _end;
                _for k := -6 _step 1 _until min - 1,
                      max + 1 _step 1 _until 6 _do
                        DP[k, 0] := DP[k, 1] := 1;
                min := 1 - tn - zdz;
                max := - min - xdx - ydy;
                maxim(min, -4);
                minim(max, +4);
                _for k := min _step 1 _until -1,
                            +1 _step 1 _until max _do
                _begin
                    x := x0 + k * dx;
                    y := y0 + k * dy;
                    _if BORD[x, y] = 2 _then
                    _begin
                        vnl := AVNL[x, y];
                        _if vnl > sw _then
                        _begin
                            AVNL[x, y] := 0;
                            _goto NXT
                        _end;
                        _if vnl _ne 0 _then
                        _begin
                            RVAL[vnl, q] := 0;
                            _for b := 0, 1 _do
                                RNUM [vnl, b, q] :=
                                RLANG [vnl, b, q] :=
                                RVRY[vnl, b, q] := 0
                        _end _else _begin
                            vnl := AVNL[x, y] := sw;
                            AX[sw] := x;
                            AY[sw] := y;
                            sw := sw + 1;
                            _for j := 1, 2, 3, 4 _do
                            _begin
                                RVAL[vnl, j] := 0;
                                _for b := 0, 1 _do
                                    RNUM[vnl, b, j] :=
                                    RLANG[vnl, b, j] :=
                                    RVRY[vnl, b, j] := 0
                            _end
                        _end;
                        _for j := -2 _step 1 _until +2 _do
                            _for b := 0, 1 _do
                            _begin
                                _if DP[k + j, 1 - b] = 0 _then
                                _begin
                                    d := DP[k + j, b];
                                    RVAL[vnl, q] := RVAL[vnl, q] + d;
                                    _if maxim(RLANG[vnl, b, q], d)
                                    _then RVRY[vnl, b, q] := 0
                                    _else _if RLANG [vnl, b, q] = d
                                    _then RVRY[vnl, b, q] := 1;
                                    RNUM[vnl, b, q] :=
                                        RNUM[vnl, b, q] + 1
                                _end
                            _end;
                        TEMPO[vnl, 0] :=
                        TEMPO[vnl, 1] :=
                        WT[vnl, 0] :=
                        WT[vnl, 1] := 4;
                        _for j := 1, 2, 3, 4 _do
                            _for b := 0, 1 _do
                            _begin
                                t := 4 - RLANG[vnl, b, j];
                                w := t - RVRY[vnl, b, j];
                                minim(TEMPO[vnl, b], t);
                                _if _if minim(WT[vnl, b], w)
                                    _then _false _else w = 1
                                    _and WT[vnl, b] = 1 _then
                                    WT[vnl, b] := 0;
                            _end
                    _end;
NXT:            _end;
NOR:            _if q = 3 _then
                    dx := 0
                _else
                    dy := dy + 1;
            _end;
UIT:    _end;

        _procedure ZET(x, y);
            _value x, y;
            _integer x, y;
        _begin
            BORD[x, y] := kleur;
            CORR(x, y);
            kleur := 1 - kleur;
            znr := znr + 1
        _end;

        _procedure PRINTZET(x, y);
            _value x, y;
            _integer x, y;
        _begin
            draw(BORD, nt, znr, kleur, x, y);
        _end;

        _integer _procedure SEL;
        _begin
            _integer v, k, j, wsl, w, m;
            w := 4;
            v := m := 0;
            _for k := sw - 1 _step -1 _until 1 _do
            _begin
                _if _if BORD [AX[k], AY[k]] = 2 _then
                          WT[k, 1 - kleur] = 0 _else _false
                _then
                    minim(w, TEMPO[k, 1 - kleur])
            _end;
            _for k := sw - 1 _step -1 _until 1 _do
            _begin
                _if _if BORD[AX[k], AY[k]] = 2 _then
                        (_if TEMPO[k, kleur]_le w _then _true _else
                         _if WT[k, 1 - kleur] = 0 _then
                            w = TEMPO[k, 1 - kleur] _else _false)
                     _else _false _then
                _begin
                    _integer a, b;
                    a := SUM(j, 1, 4,
                             2.0 * RVAL[k, j] + RNUM[k, kleur, j]);
                    b := SUM(j, 0, 1, 2 **
                             (7.0 - WT[k, j] - abs(j - kleur) / 2.0));
                    _if maxim(v, a + b) _then
                        m := k
                _end
            _end;
            SEL := m
        _end;

        NLCR;
        _for x := 1 _step 1 _until nt _do
            _for y := 1 _step 1 _until nt _do
            _begin
                BORD[x, y] := 2;
                AVNL[x, y] := 0
            _end;
        kleur := sign(xeen(33554432));
        NLCR;
        sw := znr := 1;
        _if kleur = 0 _then
        _begin
            PRINTZET(tn, tn);
            ZET(tn, tn)
        _end;
MENS:
        x := div(xeen(992), 32);
        y := xeen(31);
        _if x = 0 _or x > nt _or y = 0 _or y > nt _then
            _goto MENS;
        _if BORD[x, y] _ne 2 _then
            _goto MENS;
        PRINTZET(x, y);
        _if xeen(-1) < 0 _then
            _goto MENS;
        ZET(x, y);
        _if klaar _then
            _goto LKLAAR;

MACH:   km := SEL;
        _if km = 0 _then
        _begin
            NLCR;
            _write (54, `(11H Ik geef op)');
            _goto LKLAAR
        _end;
        x := AX[km];
        y := AY[km];
        PRINTZET(x, y);
        ZET(x, y);
        _if _not klaar _then
            _goto MENS;
LKLAAR: NLCR;
        _if kleur = 0 _then
            _write (54, `(16H U hebt gewonnen)')
        _else
            _write (54, `(16H Ik heb gewonnen)')
    _end
_end
_eop
