_comment Integer division, Revised Report 3.3.4.2: sign(a/b) × entier(abs(a/b));
_begin _integer a, b;
    print(7 ÷ 2, -7 ÷ 2, 7 ÷ (-2), (-7) ÷ (-2), 1 ÷ 3, 0 ÷ 5, NEWLINE);
    print(2 + 7 ÷ 2 × 3, 100 _div 7, 12 ÷ 4 ÷ 2, NEWLINE);
    a := 17; b := 5;
    print(a ÷ b, a - a ÷ b × b, -a ÷ b, NEWLINE)
_end
