_code:_real rndm;
_code:terin;
_begin
    _comment From: github.com/cwi-software-archive/Algol60
    Jungle Generator prints random sentences in English as a demo.
    Author: C.H.A. Koster, Mathematical Centre.
    Date: 1964 or earlier.
    Listing and papertape are from X1 at the HTS"A".
    Copied in 1977/1978 and preserved since that moment.
    Papertape is in 7 bit code for Friden Flexowriter.
    The program refers to "the world famous X8 computer".
    The X8 may be of later date than the original program.;

    _real rr;
    _integer input;

    _real _procedure R;
    _begin
        R := rr := rndm(-1)
    _end;

    _procedure P(u);
        _string u;
    print(u);

    _procedure sentence;
    _begin
        subject;
        predicate;
        _if R < .2 _then
            nextsentence
    _end;

    _procedure nextsentence;
    _begin
        connective;
        sentence
    _end;

    _procedure connective;
        _if R <.3 _then
            P('' and'')
        _else _if rr < .55 _then
            P('' but'')
        _else _if rr <.8 _then
            P('' because'')
        _else
            P('' though'');

    _procedure subject;
        _if R < .8 _then
            subst
        _else
            subname;

    _procedure subst;
    _begin
        article;
        noun phrase;
        _if R < .2 _then
            property
    _end;

    _procedure property;
        _if R <.25 _then
            P('' with blue eyes'')
        _else _if rr < .5 _then
            P('' in a mink coat'')
        _else
            _begin
                P('' from'');
                _if R < .25 _then
                    P('' Australia'')
                _else _if rr < .5 _then
                    P('' Switserland'')
                _else _if rr <.75 _then
                    P('' Siberia'')
                _else
                    P('' Cleveland (Ohio)'')
            _end;

    _procedure noun phrase;
        _if R < .25 _then
            _begin
                adje;
                noun phrase
            _end
        _else
            nounpart;

    _procedure nounpart;
        _if R < .25 _then
            _begin
                noun;
                relsentence
            _end
        _else
            noun;

    _procedure predicate;
        _if R < .2 _then
            _begin
                modifier;
                modpredicate
            _end
        _else _if rr < .5 _then
            quality
        _else
            modpredicate;

    _procedure modpredicate;
    _begin
        verb;
        object
    _end;

    _procedure relsentence;
    _begin
        P('', that'');
        _if R < .4 _then
            _begin
                subject;
                verb
            _end
        _else _if rr < .6 _then
            quality
        _else
            predicate
    _end;

    _procedure object;
        _if R < .8 _then
            subst
        _else
            objname;

    _procedure quality;
    _begin
            _if R < .45 _then
                P('' was'')
            _else
                P('' is'');
            _if R < .4 _then
                adje
            _else
                object
    _end;

    _procedure adje;
        _if R < .2 _then
            _begin
                adverb;
                adjective
            _end
        _else
            adjective;

    _procedure subname;
        _if R < .2 _then
            P('' he'')
        _else _if rr < .4 _then
            P('' Jim'')
        _else _if rr < .6 _then
            P('' Mary'')
        _else _if rr < .8 _then
            P('' she'')
        _else _if rr < .9 _then
            P('' David Livingstone'')
        _else
            P('' the world Famous X8 Computer'');

    _procedure objname;
        _if R < .2 _then
            P('' him'')
        _else _if rr < .4 _then
            P('' Jim'')
        _else _if rr < .6 _then
            P('' Mary'')
        _else _if rr < .8 _then
            P('' her'')
        _else _if rr < .95 _then
            P('' David Livingstone'')
        _else
            P('' a computer'');

    _procedure modifier;
        _if R < .33 _then
            P('' always'')
        _else _if rr < .67 _then
            P('' often'')
        _else
            P('' never'');

    _procedure article;
        _if R < .35 _then
            P('' a'')
        _else _if rr < .75 _then
            P('' the'')
        _else _if rr < .87 _then
            P('' this'')
        _else
            P('' that'');

    _procedure noun;
        _if R < .17 _then
            P('' boy'')
        _else _if rr < .33 _then
            P('' tree'')
        _else _if rr < .5 _then
            P('' girl'')
        _else _if rr < .67 _then
            P('' bear'')
        _else _if rr < .83 _then
            P('' hunter'')
        _else
            P('' lamb'');

    _procedure adjective;
        _if R < .2 _then
            P('' little'')
        _else _if rr < .4 _then
            P('' nice'')
        _else _if rr < .6 _then
            P('' big'')
        _else _if rr < .8 _then
            P('' green'')
        _else
            P('' bad'');

    _procedure adverb;
        _if R < .4 _then
            P('' very'')
        _else _if rr < .6 _then
            P('' extraordinarily'')
        _else
            P('' rather'');

    _procedure verb;
        _if R < .45 _then
            _begin
                _if R < .2 _then
                    P('' saw'')
                _else _if rr < .4 _then
                    P('' liked'')
                _else _if rr < .6 _then
                    P('' dreamt about'')
                _else _if rr < .7 _then
                    P('' smelled'')
                _else _if rr < .8 _then
                    P('' ate'')
                _else
                    P('' had'')
            _end
        _else
            _begin
                _if R < .2 _then
                    P('' sees'')
                _else _if rr < .4 _then
                    P('' likes'')
                _else _if rr < .6 _then
                    P('' dreams about'')
                _else _if rr < .7 _then
                    P('' eats'')
                _else _if rr < .8 _then
                    P('' smells'')
                _else
                    P('' has'')
            _end;
NZ:
    sentence;
    print(newline);

    terin(input, 1);
    _goto NZ
_end
_eop
