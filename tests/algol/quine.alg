 _begin_string_array L[1:7];_integer I,J;_string Q,R,S;
 _procedure P(S);_string S;_begin_for J:=1_step 1_until
 LENGTH(S)_do PRINT(_if J_ne 45_and S[J] = ′′/′′ _then Q
 _else S[J]); PRINT(NEWLINE) _end; R := ′′1234567′′;
 L[1]:=′′/BEGIN//STRING//ARRAY/L[1:7];/INTEGER/I,J;/STRING/Q,R,S;′′;
 L[2]:=′′/PROCEDURE/P(S);/STRING/S;/BEGIN//FOR/J:=1/STEP/1/UNTIL/′′;
 L[3]:=′′LENGTH(S)/DO/PRINT(/IF/J/NE/45/AND/S[J] = ///// /THEN/ Q′′;
 L[4]:=′′/ELSE/ S[J]); PRINT(NEWLINE) /END/; R := //1234567//;′′;
 L[5]:=′′Q:=///47///;/FOR/ I:=1,2,3,4/DO/P(L[I]); /FOR/I:=1,2,3,4,′′;
 L[6]:=′′5,6,7/DO/PRINT(//L[//,R[I],//]:=//,Q,Q,L[I],Q,Q,//;//,′′;
 L[7]:=′′NEWLINE);/FOR/ I:=5,6,7 /DO/ P(L[I]); /END//EOP/′′;
 Q:=′′′47′′′;_for I:=1,2,3,4_do P(L[I]); _for I:=1,2,3,4,
 5,6,7_do PRINT(′′L[′′,R[I],′′]:=′′,Q,Q,L[I],Q,Q,′′;′′,
 NEWLINE);_for I:=5,6,7 _do P(L[I]); _end_eop
