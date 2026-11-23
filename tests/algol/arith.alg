_begin
    _comment INTEGER AND REAL ARITHMETIC, PRECEDENCE, POWER;
    _integer I, J, K;
    _real X, Y;
    I := 7; J := 3;
    PRINT(I + J, I - J, I * J, I / J);
    PRINT(I + J * 2, (I + J) * 2, -I + J, I - J - 1);
    PRINT(I _power 2, 2 _power 10, 2.0 _power (-1));
    X := 1.5; Y := -0.25;
    PRINT(X + Y, X * Y, X / Y, X - Y);
    K := X + 0.6;
    PRINT(K, X + I, 1.0⏨3, 2.5⏨-2);
_end
_eop
