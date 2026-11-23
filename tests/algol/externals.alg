_comment CALLS OF _code PROCEDURES, READ AND LOAD, A LOOP THAT
    NEVER ENDS;
_code: CP;
_begin _integer I; _real X;
  _integer _array IA[1:3];
  CP(IA[1]); CP(IA); CP(CP); CP(`S'+`T');
  READ(I); READ(IA[1]); READ(IA); READ(LENGTH);
  LOAD(I);
  _for I := 1 _step 1 _until I _do X := 1
_end
_eop
