_begin
    _comment INTEGER AND REAL ARITHMETIC, PRECEDENCE, POWER;
    _integer I, J, K;
    _real X, Y;
    I := 7; J := 3;
    print(I + J, I - J, I * J, I / J, NEWLINE);
    print(I + J * 2, (I + J) * 2, -I + J, I - J - 1, NEWLINE);
    print(I _power 2, 2 _power 10, 2.0 _power (-1), NEWLINE);
    X := 1.5; Y := -0.25;
    print(X + Y, X * Y, X / Y, X - Y, NEWLINE);
    K := X + 0.6;
    print(K, X + I, 1.0⏨3, 2.5⏨-2, NEWLINE);
_end
