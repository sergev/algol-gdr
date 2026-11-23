_comment A COMMENT BEFORE THE PROGRAM;
_begin
    _comment A COMMENT IN A BLOCK, WITH _begin AND _end INSIDE;
    _integer I;
    I := 1;
    _begin
        I := 2
    _end THIS TEXT AFTER END IS A COMMENT;
    ;;
    PRINT(I); _comment AFTER A STATEMENT;
_end
