//
// Parser of pass 1 (G13666 and the parser part of G14504), translated label by
// label. The original passes return addresses on its stack and resumes after a
// syntax error at the point saved in 02106 (address) and 02107 (stack depth),
// so the routines are one function here: Lnnnnn is the code at nnnnn, `stk`
// is the stack of M17, and `pc` with `dispatch` stands for a computed jump.
//
#include <cstdio>
#include <ostream>
#include <stdexcept>
#include <string>

#include "pass1.h"
#include "tables.h"

namespace {

constexpr Word MASK24  = 077777777;
constexpr unsigned R15 = 077777;

// Constants of pass 1 (AE16472 + offset, in the table at 15146…).
Word K(unsigned addr)
{
    return tables::symattr.at(addr);
}

// Attributes of a symbol code: the dispatch fields of the parser.
Word attr(unsigned code)
{
    return code < tables::symattr.words.size() ? tables::symattr.words[code] : 0;
}

Word arx(Word a, Word b)
{
    Word s = a + b;
    if (s >> 48)
        s = (s + 1) & 07777777777777777;
    return s;
}

// APX: gather the bits of a selected by mask into the high bits.
Word apx(Word a, Word mask)
{
    Word r = 0;
    for (int i = 0; i < 48; i++) {
        if ((mask >> i) & 1) {
            r >>= 1;
            r |= ((a >> i) & 1) << 47;
        }
    }
    return r;
}

} // namespace

// AG15120: write an item of the stream. An identifier of a 'COMMON' block
// becomes a subscripted variable with the constant 1.
void Pass1::emit(Word a)
{
    emit_item_ = a;
    if ((a & 07777) == 0) {
        Word slot = (a >> 12) & 07777;
        for (unsigned k = 3;; k += 3) {
            if (ws_[027246 + k] == 0)
                break;
            if (((ws_[027247 + k] & 077777) ^ 1) != 0)
                continue;
            if (((ws_[027246 + k] >> 24) & 07777) != slot)
                continue;
            out_.push_back(Item{ uint32_t(K(015357) & MASK24) });
            a = emit_item_ | K(015304);
            break;
        }
    }
    out_.push_back(Item{ uint32_t(a & MASK24) });
}

// AG15104: a statement marker, line << 12 | n << 7 | 051.
void Pass1::stmark()
{
    Word a;
    if (ws_[D_LINE] == ws_[D_MARKLINE]) {
        a = arx(ws_[D_MARKN], K(015262));
    } else {
        ws_[D_MARKLINE] = ws_[D_LINE];
        a               = K(015262);
    }
    ws_[D_MARKN] = a;
    emit(((ws_[D_MARKLINE] << 12) & 07777777777777777) | ws_[D_MARKN] | K(015361));
}

// AG15102: the point to resume at after a syntax error, then a marker.
void Pass1::setres(unsigned addr)
{
    ws_[D_RESSP] = stk_.size();
    ws_[D_RESPC] = addr;
    stmark();
}

// AG15114: push a word onto the D-list, which grows down from 05753.
void Pass1::dpush(Word a)
{
    ws_[m3_] = a;
    m3_      = (m3_ - 1) & R15;
    if (((m3_ + 073401) & R15) == 0)
        throw SyntaxError{ 015 };
}

void Pass1::print_line(const std::string &line)
{
    if (ctx_.listing)
        *ctx_.listing << line << '\n';
}

void Pass1::parse()
{
    auto &stk = stk_;
    auto push = [&](Word w) { stk.push_back(w); };
    auto pop  = [&]() {
        Word w = stk.back();
        stk.pop_back();
        return w;
    };
    // M[M17 - k]: the k-th word from the top.
    auto S    = [&](unsigned k) -> Word    &{ return stk[stk.size() - k]; };
    auto drop = [&](unsigned n) { stk.resize(stk.size() - n); };
    auto D0   = [&]() { return ws_[D_SYM]; };

    Word a      = 0;
    unsigned m1 = 0, m8 = 0, m13 = 0, m14 = 0;
    unsigned pc = 013666;
    Word x14417 = 0; // a cell of the code, *0531B

    for (;;) {
        try {
            goto dispatch;
        dispatch:
            switch (pc) {
            case RET_DRIVER:
                return;
            case 013666:
                goto L13666;
            case 013711:
                goto L13711;
            case 013715:
                goto L13715;
            case 013724:
                goto L13724;
            case 013730:
                goto L13730;
            case 013732:
                goto L13732;
            case 013736:
                goto L13736;
            case 013742:
                goto L13742;
            case 013743:
                goto L13743;
            case 013745:
                goto L13745;
            case 013750:
                goto L13750;
            case 013755:
                goto L13755;
            case 013756:
                goto L13756;
            case 013760:
                goto L13760;
            case 013767:
                goto L13767;
            case 014010:
                goto L14010;
            case 014011:
                goto L14011;
            case 014013:
                goto L14013;
            case 014020:
                goto L14020;
            case 014023:
                goto L14023;
            case 014046:
                goto L14046;
            case 014047:
                goto L14047;
            case 014050:
                goto L14050;
            case 014056:
                goto L14056;
            case 014060:
                goto L14060;
            case 014062:
                goto L14062;
            case 014063:
                goto L14063;
            case 014066:
                goto L14066;
            case 014070:
                goto L14070;
            case 014074:
                goto L14074;
            case 014075:
                goto L14075;
            case 014105:
                goto L14105;
            case 014111:
                goto L14111;
            case 014112:
                goto L14112;
            case 014113:
                goto L14113;
            case 014114:
                goto L14114;
            case 014116:
                goto L14116;
            case 014117:
                goto L14117;
            case 014120:
                goto L14120;
            case 014121:
                goto L14121;
            case 014131:
                goto L14131;
            case 014136:
                goto L14136;
            case 014140:
                goto L14140;
            case 014143:
                goto L14143;
            case 014146:
                goto L14146;
            case 014147:
                goto L14147;
            case 014150:
                goto L14150;
            case 014151:
                goto L14151;
            case 014153:
                goto L14153;
            case 014154:
                goto L14154;
            case 014160:
                goto L14160;
            case 014165:
                goto L14165;
            case 014166:
                goto L14166;
            case 014167:
                goto L14167;
            case 014170:
                goto L14170;
            case 014174:
                goto L14174;
            case 014175:
                goto L14175;
            case 014201:
                goto L14201;
            case 014202:
                goto L14202;
            case 014206:
                goto L14206;
            case 014210:
                goto L14210;
            case 014212:
                goto L14212;
            case 014213:
                goto L14213;
            case 014217:
                goto L14217;
            case 014223:
                goto L14223;
            case 014224:
                goto L14224;
            case 014225:
                goto L14225;
            case 014226:
                goto L14226;
            case 014230:
                goto L14230;
            case 014232:
                goto L14232;
            case 014234:
                goto L14234;
            case 014236:
                goto L14236;
            case 014240:
                goto L14240;
            case 014241:
                goto L14241;
            case 014243:
                goto L14243;
            case 014244:
                goto L14244;
            case 014245:
                goto L14245;
            case 014250:
                goto L14250;
            case 014252:
                goto L14252;
            case 014256:
                goto L14256;
            case 014257:
                goto L14257;
            case 014262:
                goto L14262;
            case 014265:
                goto L14265;
            case 014277:
                goto L14277;
            case 014301:
                goto L14301;
            case 014313:
                goto L14313;
            case 014315:
                goto L14315;
            case 014323:
                goto L14323;
            case 014325:
                goto L14325;
            case 014327:
                goto L14327;
            case 014342:
                goto L14342;
            case 014350:
                goto L14350;
            case 014352:
                goto L14352;
            case 014356:
                goto L14356;
            case 014401:
                goto L14401;
            case 014402:
                goto L14402;
            case 014404:
                goto L14404;
            case 014410:
                goto L14410;
            case 014446:
                goto L14446;
            case 014451:
                goto L14451;
            case 014460:
                goto L14460;
            case 014470:
                goto L14470;
            case 014472:
                goto L14472;
            case 014504:
                goto L14504;
            case 014510:
                goto L14510;
            case 014526:
                goto L14526;
            case 014546:
                goto L14546;
            case 014563:
                goto L14563;
            case 014564:
                goto L14564;
            case 014601:
                goto L14601;
            case 014610:
                goto L14610;
            case 014613:
                goto L14613;
            case 014614:
                goto L14614;
            case 014637:
                goto L14637;
            case 014641:
                goto L14641;
            case 014642:
                goto L14642;
            case 014651:
                goto L14651;
            case 014655:
                goto L14655;
            case 014666:
                goto L14666;
            case 014670:
                goto L14670;
            case 014700:
                goto L14700;
            case 014704:
                goto L14704;
            case 014705:
                goto L14705;
            case 014715:
                goto L14715;
            case 014716:
                goto L14716;
            case 014722:
                goto L14722;
            case 014724:
                goto L14724;
            case 014726:
                goto L14726;
            case 014736:
                goto L14736;
            case 014743:
                goto L14743;
            case 014745:
                goto L14745;
            case 014746:
                goto L14746;
            case 014750:
                goto L14750;
            case 014752:
                goto L14752;
            case 014753:
                goto L14753;
            case 014762:
                goto L14762;
            case 014775:
                goto L14775;
            case 014777:
                goto L14777;
            case 015004:
                goto L15004;
            case 015010:
                goto L15010;
            case 015011:
                goto L15011;
            case 015013:
                goto L15013;
            case 015015:
                goto L15015;
            case 015016:
                goto L15016;
            case 015024:
                goto L15024;
            case 015026:
                goto L15026;
            case 015032:
                goto L15032;
            case 015036:
                goto L15036;
            case 015037:
                goto L15037;
            case 015043:
                goto L15043;
            case 015045:
                goto L15045;
            case 015063:
                goto L15063;
            case 015072:
                goto L15072;
            case 015076:
                goto L15076;
            case 017015:
                goto L17015;
            case 017056:
                goto L17056;
            case 017062:
                goto L17062;
            case 017066:
                goto L17066;
            case 017076:
                goto L17076;
            case 017103:
                goto L17103;
            default:
                throw std::logic_error("pass 1: no code at " + std::to_string(pc));
            }
            //
            // AG13666: the start of the pass and the program head.
            //
        L13666:
            push(RET_DRIVER);
            ws_[D_RESSP] = stk.size();
            if (!ctx_.options.no_listing)
                blank_line(ctx_);
            ws_[D_LIST]  = ctx_.options.no_listing ? 0 : 1;
            m5_          = 0;
            m6_          = 0;
            m3_          = 05753;
            ws_[02001]   = K(015403);
            ws_[D_POOL]  = K(015371);
            m1           = 013760;
            ws_[D_RESPC] = m1;
            emit(0102);
            stmark();
            lex();
            ws_[02054] = 0;
        L13711: // *0023B
            ws_[02055] = K(015316);
            switch ((attr(m4_) & 017000000) >> 18) {
            case 1: // 'ALGOL'
                a = K(015275);
                goto L13745;
            case 2: // 'CODE'
                a = K(015360);
                goto L13745;
            case 3:
                a = K(015331);
                goto L13750;
            case 4:
                a = K(015324);
                goto L13750;
            case 5:
                a = K(015326);
                goto L13750;
            case 6:
                a = K(015330);
                goto L13750;
            case 7: // 'PROCEDURE'
                a = ws_[02055];
                goto L13730;
            case 010: // a label
                goto L13767;
            case 011: // 'BEGIN'
                m13 = 013732;
                goto L14020;
            case 012:
                a = K(015327);
                goto L13750;
            default:
                goto L13715;
            }
        L13715:
            m8 = 0137;
            goto L14013;
        L13724:
            a = ws_[02055];
        L13730:
            push(a | K(015317));
            push(K(015336));
            m13 = 013732;
            goto L14510;
        L13732: // *0044B: after the program block
            m1           = 014011;
            ws_[D_RESPC] = m1;
            ws_[D_RESSP] = stk.size();
            if (D0() != 075)
                goto L13743;
        L13736: // *0050B: the last sector of the string pool
            push(ws_[D_POOL]);
            a           = ws_[D_POOL] | K(015413);
            ws_[D_POOL] = a;
            pool_put(a);
            ws_[D_POOL] = pop();
        L13742: // AG13742: return through the stack
            pc = pop();
            goto dispatch;
        L13743:
            stmark();
            m8 = 0143;
            goto L17015;
        L13745: // *0057B: 'ALGOL' : or 'CODE' :
            ws_[02054] = a;
            lex();
            m4_ = (m4_ - 050) & R15;
            if (m4_)
                goto L13715;
            lex();
            goto L13711;
        L13750: // a type before an external name
            ws_[02055] = a;
            lex();
            if (D0() == 042)
                goto L13724;
            if (m4_)
                goto L13715;
            ws_[02056] = D0() >> 12;
            lex();
        L13755:
            a = ws_[02054];
            if (!a)
                goto L13715;
        L13756:
            a |= ws_[02055];
            a |= ws_[02056];
            dpush(a);
        L13760: // *0072B
            m4_ = (m4_ - 051) & R15;
            if (!m4_) {
                lex();
                goto L13711;
            }
            m4_ = (m4_ + 2) & R15;
            if (m4_)
                goto L13715;
            lex();
            if (m4_)
                goto L13715;
            ws_[02056] = D0() >> 12;
            lex();
            a = ws_[02054];
            goto L13756;
        L13767: // a label before the program
            ws_[02056] = D0() >> 12;
            lex();
            if (D0() == 047 || D0() == 051)
                goto L13755;
            if (D0() != 050)
                goto L13715;
            lex();
            emit((Word(m3_) << 12) | K(015256));
            if (!ws_[D_RENAME]) {
                ws_[02136]    = ws_[NAMES + ws_[02056]];
                ws_[D_RENAME] = apx(ws_[02136], K(015407));
            }
            dpush(ws_[02056] | K(015351));
            if (!m4_)
                goto L13767;
            if (D0() == 036) {
                m13 = 013732;
                goto L14020;
            }
            goto L13715;
        L14010: // *0122B: skip to 'EOP'
            lex();
        L14011:
            if (D0() != 075)
                goto L14010;
            goto L13736;
        L14013: // *0125B: error in the program head
            if (D0() == 075)
                goto L17066;
            if (D0() == 037) {
                lex();
                m8 = 0137;
                goto L17015;
            }
            if (D0() != 051)
                goto L17015;
            m8 = 0137;
            goto L17015;

            //
            // AG14020: a statement; for a block with declarations see L14257.
            //
        L14020:
            m8 = m13;
            setres(m8);
            push(m8);
            push(ws_[D_RESSP]);
            push(ws_[D_RESPC]);
        L14023: // *0135B
            switch ((attr(m4_) & 03600) >> 7) {
            case 0:
                m8 = 042;
                goto L17015;
            case 1: // 'BEGIN'
                goto L14056;
            case 2: // 'IF'
                lex();
                goto L14074;
            case 3: // 'GOTO'
                lex();
                goto L14117;
            case 4: // identifier
                a = D0();
                goto L14121;
            case 5: // 'FOR'
                a = m3_;
                goto L14046;
            case 6:
                a = K(015335);
                goto L14154;
            case 7:
                a = K(015373);
                goto L14213;
            case 010:
                a = K(015335);
                goto L14250;
            case 011:
                a = K(015373);
                goto L14240;
            case 012: // 'PRINT'
                a = K(015373);
                goto L14230;
            case 013:
                a = K(015373);
                goto L14236;
            case 014:
                a = K(015350);
                goto L14151;
            case 015:
                a = K(015362);
                goto L14151;
            case 016:
                a = K(015367);
                goto L14151;
            default: // an empty statement
                goto L14066;
            }
        L14046: // *0160B: 'FOR'
            push(a);
            m13 = 014047;
            goto L14655;
        L14047:
            m13 = 014050;
            goto L14020;
        L14050: // AG14050: the end of a FOR statement
            a = arx(pop(), K(015274));
            a |= K(015305);
            dpush(a);
            emit((Word(arx(m3_, K(015274))) << 12) | K(015366));
            stmark();
            goto L14066;
        L14056: // *0170B: 'BEGIN'
            m8 = 014060;
            setres(m8);
            lex();
        L14060: // *0172B
            if (attr(m4_) & 04000)
                goto L14257;
        L14062: // *0174B
            m13 = 014063;
            goto L14020;
        L14063:
            if (D0() == 051) {
                lex();
                goto L14062;
            }
            if (D0() != 037)
                goto L14070;
            lex();
        L14066: // *0200B
            ws_[D_RESPC] = pop();
            ws_[D_RESSP] = pop();
            goto L13742;
        L14070: // *0202B
            m8           = 036;
            m14          = 014062;
            ws_[D_RESPC] = m14;
            goto L17015;
        L14074: // *0206B: 'IF'
            m1  = 0;
            m13 = 014075;
            goto L14762;
        L14075:
            if (D0() != 030) {
                m8 = 037;
                goto L17015;
            }
            lex();
            emit(K(015346));
            if (D0() == 027) {
                m8 = 040;
                goto L17015;
            }
            if (D0() == 032)
                goto L14113;
            m13 = 014105;
            goto L14020;
        L14105:
            if (D0() != 031)
                goto L14116;
            lex();
            emit(K(015363));
            m13 = 014111;
            goto L14020;
        L14111:
            m14 = 054;
        L14112: // *0224B
            a = m14;
        L14150: // *0262B
            emit(a);
            goto L14066;
        L14113: // *0225B
            m13 = 014114;
            goto L14020;
        L14114:
            if (D0() == 031) {
                m8 = 041;
                goto L17015;
            }
        L14116: // *0230B
            m14 = 053;
            goto L14112;
        L14117: // *0231B: 'GOTO'
            m1  = 0;
            m13 = 014120;
            goto L14762;
        L14120:
            m14 = 026;
            goto L14112;
        L14121: // *0233B: an identifier: a label, assignment or call
            push(a);
            lex();
            if (D0() != 050)
                goto L14131;
            lex();
            emit((Word(m3_) << 12) | K(015256));
            dpush((pop() >> 12) | K(015351));
            goto L14023;
        L14131: // *0243B
            m1 = 011;
            a  = pop();
            push(014136);
            push(m1);
            push(a);
            goto L15037;
        L14136: // *0250B
            if (D0() != 022)
                goto L14066;
            a = K(015262);
        L14140: // *0252B
            push(a);
            lex();
            if (m4_)
                goto L14146;
            m1  = 0;
            m13 = 014143;
            goto L14762;
        L14143:
            if (D0() != 022)
                goto L14147;
            a = arx(pop(), K(015262));
            goto L14140;
        L14146: // *0260B
            m1  = 0;
            m13 = 014147;
            goto L14762;
        L14147: // *0261B
            a = pop() | K(015306);
            goto L14150;
        L14151: // *0263B
            push(a);
            lex();
            m1  = 0;
            m13 = 014153;
            goto L14762;
        L14153:
            a = pop();
            goto L14150;

            // I/O statements.
        L14154: // *0266B
            ws_[02052] = a;
            lex();
            if (D0() != 020)
                goto L14206;
            lex();
            m1  = 0;
            m13 = 014160;
            goto L14762;
        L14160:
            if (D0() == 045)
                goto L14202;
            m8 = 0132;
            if (D0() != 047)
                goto L17015;
            lex();
            m1  = 0;
            m13 = 014165;
            goto L14762;
        L14165:
            m4_ = (m4_ - 045) & R15;
            m8  = 0132;
        L14166: // *0300B
            if (m4_)
                goto L17015;
            lex();
        L14167: // *0301B
            m14 = 064;
            a   = m14;
        L14170: // *0302B
            emit(a);
            if ((attr(m4_) & 03600) == 03600)
                goto L14201;
        L14174: // *0306B
            m13 = 014175;
            goto L14716;
        L14175:
            if (!ws_[02117])
                emit(ws_[02052]);
            if (D0() == 047) {
                lex();
                goto L14174;
            }
        L14201: // *0313B
            m14 = 073;
            goto L14112;
        L14202: // *0314B
            lex();
            if (D0() != 047)
                goto L14212;
            lex();
            a = K(015300);
            goto L14170;
        L14206: // *0320B
            emit(K(015332));
            m1  = 0;
            m13 = 014210;
            goto L14762;
        L14210:
            m4_ = (m4_ - 047) & R15;
            if (!m4_)
                goto L14166;
            m4_ = (m4_ + 047) & R15;
            goto L14167;
        L14212: // *0324B
            a = K(015257);
            goto L14170;
        L14213: // *0325B
            ws_[02052] = a;
            lex();
            m4_ = (m4_ - 020) & R15;
            m8  = 0133;
            if (m4_)
                goto L17015;
            lex();
            m1  = 0;
            m13 = 014217;
            goto L14762;
        L14217:
            m4_ = (m4_ - 045) & R15;
            if (!m4_)
                goto L14226;
            m4_ = (m4_ - 2) & R15;
            m8  = 0133;
            if (m4_)
                goto L17015;
            lex();
            m1  = 0;
            m13 = 014223;
            goto L14762;
        L14223:
            m4_ = (m4_ - 045) & R15;
            m8  = 0133;
        L14224: // *0336B
            if (m4_)
                goto L17015;
            lex();
        L14225: // *0337B
            a = K(015314);
            goto L14170;
        L14226: // *0340B
            lex();
            a = K(015315);
            goto L14170;
        L14230: // *0342B: 'PRINT'
            ws_[02052] = a;
            lex();
            a = K(015312);
        L14232: // *0344B
            emit(a);
            m1  = 0;
            m13 = 014234;
            goto L14762;
        L14234:
            m4_ = (m4_ - 047) & R15;
            if (!m4_)
                goto L14224;
            m4_ = (m4_ + 047) & R15;
            goto L14225;
        L14236: // *0350B
            ws_[02052] = a;
            lex();
            a = K(015270);
            goto L14232;
        L14240: // *0352B
            ws_[02052] = a;
            a          = K(015345);
        L14241: // *0353B
            push(a);
            lex();
            m4_ = (m4_ - 020) & R15;
            m13 = 014243;
            goto L14252;
        L14243:
            m4_ = (m4_ - 047) & R15;
            m13 = 014244;
            goto L14252;
        L14244:
            m4_ = (m4_ - 047) & R15;
            m13 = 014245;
            goto L14252;
        L14245:
            m4_ = (m4_ - 045) & R15;
            m8  = 0135;
            if (m4_)
                goto L17015;
            lex();
            a = pop();
            goto L14170;
        L14250: // *0362B
            ws_[02052] = a;
            a          = K(015341);
            goto L14241;
        L14252: // *0364B
            push(m13);
            m8 = 0135;
            if (m4_)
                goto L17015;
            lex();
            m1  = 0;
            m13 = 014256;
            goto L14762;
        L14256:
            pc = pop();
            goto dispatch;

            //
            // *0371B: a block with declarations.
            //
        L14257:
            push(m3_);
            emit(K(015406));
            m8 = 014504;
            setres(m8);
        L14262: // AG14262
            switch ((attr(m4_) & 0740000) >> 14) {
            case 0: // the statements
                m13 = 014301;
                goto L14020;
            case 1:
                a = K(015324);
                goto L14315;
            case 2:
                a = K(015326);
                goto L14315;
            case 3:
                a = K(015330);
                goto L14315;
            case 4: // 'PROCEDURE'
                a = K(015317);
                goto L14402;
            case 5: // 'SWITCH'
                a = K(015374);
                goto L14460;
            case 6:
                m8 = 050;
                goto L17015;
            case 7: // 'ARRAY'
                a = K(015375);
                goto L14323;
            case 010:
                a = K(015331);
                goto L14315;
            case 011:
                a = K(015327);
                goto L14315;
            default:
                throw std::logic_error("pass 1: declaration dispatch");
            }
        L14277: // *0411B
            drop(1);
            lex();
            goto L14262;
        L14301: // *0413B: the end of the block
            if (D0() == 051) {
                lex();
                m13 = 014301;
                goto L14020;
            }
            if (D0() != 037)
                goto L14313;
            lex();
            a = arx(pop(), K(015274));
            dpush(a);
            emit((Word(arx(m3_, K(015274))) << 12) | K(015412));
            setres(m8);
            goto L14066;
        L14313: // *0425B
            m8 = 014265;
            setres(m8);
            m8 = 056;
            goto L17015;
        L14265:
            m13 = 014301;
            goto L14020;
        L14315: // *0427B: a simple declaration
            push(a);
            lex();
            if (!m4_)
                goto L14410;
            if (D0() == 042)
                goto L14401;
            m8 = 057;
            if (D0() != 040)
                goto L17015;
            a = pop() | K(015352);
        L14323: // *0435B: 'ARRAY'
            push(a);
            S(2) |= K(015353);
        L14325: // *0437B: an array segment
            push(0);
            push(m3_);
            lex();
        L14327: // *0441B
            m8 = 060;
            if (m4_)
                goto L17015;
            dpush((D0() >> 12) | S(3));
            S(2) = arx(S(2), K(015262));
            lex();
            if (D0() == 047) {
                lex();
                goto L14327;
            }
            a = arx(D0(), K(015333));
            push(a);
            if (D0() != 021 && D0() != 020) {
                m8 = 061;
                goto L17015;
            }
        L14342: // *0454B
            m8 = 062;
            if (S(3) & K(015404))
                goto L17015;
            emit((Word(arx(m3_, K(015274))) << 12) | S(3) | K(015255));
            push(K(015262));
        L14350: // *0462B: bounds
            lex();
            m1  = 0;
            m13 = 014352;
            goto L14762;
        L14352:
            m8 = 063;
            if (D0() != 050)
                goto L17015;
            lex();
            m1  = 0;
            m13 = 014356;
            goto L14762;
        L14356:
            if (D0() == 047) {
                a = arx(pop(), K(015262));
                push(a);
                goto L14350;
            }
            // *0474B
            m8 = 064;
            if (S(2) != D0())
                goto L17015;
            emit(S(1) | K(015320));
            m14 = (S(4) >> 7) & R15;
            m8  = m3_;
            do { // *0502B: the dimensions into the D-list words
                a                   = ((S(1) << 8) & 07777777777777777) | ws_[(m8 + 1) & R15];
                ws_[(m8 + 1) & R15] = a;
                m8                  = (m8 + 1) & R15;
                m14                 = (m14 - 1) & R15;
            } while (m14);
            drop(4);
            lex();
            if (D0() == 047)
                goto L14325;
            if (D0() == 051)
                goto L14277;
            m8 = 065;
            goto L17015;
        L14401: // *0513B: a typed procedure
            a = pop() | K(015260);
        L14402: // *0514B
            push(a);
            push(K(015336));
            m13 = 014404;
            goto L14510;
        L14404:
            m8 = 014504;
            setres(m8);
            if (D0() == 051) {
                lex();
                goto L14262;
            }
            m8 = 066;
            goto L17015;
        L14410: // *0522B: a simple variable; in 'COMMON' an array [1:1]
            x14417 = D0() >> 12;
            m13    = 0;
            for (;;) {
                a = ws_[(027246 + m13) & R15];
                if (!a)
                    goto L14446;
                if ((((a >> 24) & 07777) ^ x14417) == 0)
                    break;
                m13 = (m13 + 3) & R15;
            }
            // *0532B
            S(2) |= K(015353);
            push(0);
            push(m3_);
            dpush((D0() >> 12) | S(3));
            S(2) = arx(S(2), K(015262));
            push(K(015340));
            emit((Word(arx(m3_, K(015274))) << 12) | S(3) | K(015255));
            push(K(015262));
            emit(K(015357));
            emit(K(015357));
            emit(S(1) | K(015320));
            m14                 = (S(4) >> 7) & R15;
            m8                  = m3_;
            a                   = ((S(1) << 8) & 07777777777777777) | ws_[(m8 + 1) & R15];
            ws_[(m8 + 1) & R15] = a;
            m8                  = (m8 + 1) & R15;
            drop(4);
            goto L14451;
        L14446: // *0560B
            a = pop() | K(015321);
            push(a);
            dpush((D0() >> 12) | S(1));
        L14451: // *0563B
            lex();
            if (D0() == 051)
                goto L14277;
            m8 = 067;
            if (D0() != 047)
                goto L17015;
            lex();
            if (!m4_)
                goto L14410;
            pop();
            m8 = 070;
            goto L17015;
        L14460: // *0572B: 'SWITCH'
            push(a);
            lex();
            m8 = 071;
            if (m4_)
                goto L17015;
            push(m3_);
            push(0);
            dpush((D0() >> 12) | S(3));
            lex();
            m8 = 072;
            if (D0() != 022)
                goto L17015;
        L14470: // *0602B
            lex();
            m1  = 0;
            m13 = 014472;
            goto L14762;
        L14472:
            a = arx(pop(), K(015262));
            push(a);
            if (D0() == 047)
                goto L14470;
            m8 = 073;
            if (D0() != 051)
                goto L17015;
            m8 = 074;
            if (S(1) & K(015404))
                goto L17015;
            a = (S(2) << 12) & 07777777777777777;
            a |= pop();
            emit(a | K(015277));
            drop(1);
            goto L14277;
        L14504: // AG14504: the point to resume at in the declarations
            m8 = 014504;
            setres(m8);
            if (D0() == 051)
                lex();
            goto L14262;

            //
            // AG14510: a procedure heading. The caller pushed the type word
            // and K(015336); the return is through the third word below them.
            //
        L14510:
            push(m13);
            lex();
            m8 = 0106;
            if (m4_)
                goto L17015;
            a = arx(m3_, K(015274));
            push(a);
            a |= K(015301);
            push(a);
            a = (D0() >> 12) | S(5);
            push(a);
            emit(K(015307));
            lex();
            if (D0() == 051)
                goto L14651;
            m8 = 0107;
            if (D0() != 020)
                goto L17015;
            lex();
            push(0);
        L14526: // *0022B: the formal parameters
            m8 = 0110;
            if (m4_)
                goto L17015;
            a = arx(pop(), K(015262));
            push(a);
            dpush((D0() >> 12) | S(6));
            lex();
            if (D0() == 047) {
                lex();
                goto L14526;
            }
            m8 = 0111;
            if (D0() != 045)
                goto L17015;
            lex();
            m8 = 0112;
            if (S(1) & K(015404))
                goto L17015;
            m8 = 0113;
            if (D0() != 051)
                goto L17015;
            lex();
            if (D0() != 061)
                goto L14563;
            lex();
        L14546: // *0042B: 'VALUE'
            m8 = 0114;
            if (m4_)
                goto L17015;
            m14 = find_formal();
            m8  = 0116;
            if (ws_[m14] & K(015261))
                goto L17015;
            ws_[m14] = (ws_[m14] & K(015376)) | K(015261);
            emit((Word(m14) << 12) | K(015302));
            lex();
            if (D0() == 047) {
                lex();
                goto L14546;
            }
            m8 = 0117;
            if (D0() != 051)
                goto L17015;
            lex();
        L14563: // *0057B: the specifications
            S(6) = 0;
        L14564: // *0060B
            switch ((attr(m4_) & 0740000) >> 14) {
            case 0:
                a = S(6);
                goto L14637;
            case 1:
                a = K(015263);
                goto L14601;
            case 2:
                a = K(015264);
                goto L14601;
            case 3:
                a = K(015266);
                goto L14601;
            case 4:
                a = K(015260);
                goto L14613;
            case 5:
                a = K(015352);
                goto L14613;
            case 6:
                a = K(015322);
                goto L14613;
            case 7:
                a = K(015355);
                goto L14613;
            case 010:
                a = K(015267);
                goto L14601;
            case 011:
                a = K(015265);
                goto L14601;
            default:
                throw std::logic_error("pass 1: specification dispatch");
            }
        L14601: // *0075B
            S(7) = a;
            lex();
            if (!m4_)
                goto L14610;
            if (D0() == 040) {
                a = K(015352) | S(7);
                goto L14613;
            }
            m8 = 0120;
            if (D0() != 042)
                goto L17015;
            a = K(015260) | S(7);
            goto L14613;
        L14610: // *0104B
            a    = K(015372) | S(7);
            S(7) = a;
            goto L14614;
        L14613: // *0107B
            S(7) = a;
            lex();
        L14614: // *0110B
            m8 = 0121;
            if (m4_)
                goto L17015;
            S(6) = arx(S(6), K(015262));
            m14  = find_formal();
            m8   = 0122;
            if (ws_[m14] & K(015377))
                goto L17015;
            ws_[m14] |= S(7);
            lex();
            if (D0() == 051) {
                lex();
                goto L14564;
            }
            if (D0() == 047) {
                lex();
                goto L14614;
            }
            m8 = 0123;
            goto L17015;
        L14637: // *0133B
            m8 = 0124;
            if (a ^ S(1))
                goto L17015;
        L14641: // *0135B: the body
            m13 = 014642;
            goto L14020;
        L14642:
            emit((Word(m3_) << 12) | K(015311));
            dpush(S(3));
            a = (pop() << 8) & 07777777777777777;
            a |= pop();
            dpush(a);
            pc = S(3);
            drop(5);
            goto dispatch;
        L14651: // *0145B: no parameters
            S(1) |= K(015323);
            push(0);
            lex();
            goto L14641;

            //
            // AG14655: the head of a FOR statement, up to DO.
            //
        L14655:
            push(m13);
            lex();
            m8 = 043;
            if (m4_)
                goto L17015;
            emit(K(015401));
            push(D0());
            emit(D0() | K(015365));
            lex();
            m8 = 044;
            if (D0() != 022)
                goto L17015;
        L14666: // *0162B: a FOR list element
            lex();
            m1  = 0;
            m13 = 014670;
            goto L14762;
        L14670:
            switch ((attr(m4_) & 030000) >> 12) {
            case 0:
                m8 = 045;
                goto L17015;
            case 1:
                m14 = 056;
                goto L14705;
            case 2: // 'WHILE'
                lex();
                m1  = 0;
                m13 = 014715;
                goto L14762;
            default: // 'STEP'
                lex();
                m1  = 0;
                m13 = 014700;
                goto L14762;
            }
        L14700:
            m8 = 046;
            if (D0() != 052)
                goto L17015;
            lex();
            m1  = 0;
            m13 = 014704;
            goto L14762;
        L14704:
            m14 = 034;
        L14705: // *0201B
            emit(m14);
            if (D0() == 047)
                goto L14666;
            m8 = 047;
            if (D0() != 033)
                goto L17015;
            lex();
            emit(K(015370) | pop());
            goto L13742;
        L14715: // *0210B
            m14 = 035;
            goto L14705;

            //
            // AG14716: an element of an I/O list.
            //
        L14716:
            push(m13);
            if (D0() == 020)
                goto L14724;
            if (D0() == 032)
                goto L14750;
            m1  = 0;
            m13 = 014722;
            goto L14762;
        L14722: // *0216B
            ws_[02117] = 0;
            pc         = pop();
            goto dispatch;
        L14724: // *0220B
            lex();
            m13 = 014726;
            goto L14716;
        L14726:
            if (ws_[02117])
                goto L14736;
            if (D0() != 045)
                goto L14746;
            lex();
            if (!(attr(m4_) & 0170))
                goto L14722;
            m1 = 0;
            push(014722);
            push(m1);
            goto L15016;
        L14736: // *0232B
            m8 = 0136;
            if (D0() == 047)
                goto L14743;
            if (D0() != 045)
                goto L17015;
            ws_[02117] = ws_[02052];
            lex();
            goto L13742;
        L14743: // *0237B
            lex();
            m13 = 014745;
            goto L14716;
        L14745:
            if (ws_[02117])
                goto L14736;
        L14746: // *0242B
            emit(ws_[02052]);
            goto L14736;
        L14750: // *0244B: FOR in an I/O list
            push(m3_);
            m13 = 014752;
            goto L14655;
        L14752:
            m13 = 014753;
            goto L14716;
        L14753:
            if (!ws_[02117]) {
                ws_[02117] = ws_[02052];
                emit(ws_[02117]);
            }
            // *0252B
            ws_[02054] = pop();
            push(ws_[D_RESSP]);
            push(ws_[D_RESPC]);
            push(ws_[02054]);
            goto L14050;

            //
            // AG14762: an expression; M1 is the priority of the context.
            //
        L14762:
            push(m13);
            push(m1);
            switch (attr(m4_) & 07) {
            case 0:
                m8 = 024;
                goto L17015;
            case 1: // 'IF'
                if (!m1)
                    goto L14775;
                m8 = 026;
                goto L17015;
            case 2: // '('
                lex();
                m1  = 0;
                m13 = 015013;
                goto L14762;
            case 3: // 'NOT'
                m1 = 5;
                a  = K(015325);
                goto L15032;
            case 4: // '+', '-'
                m1 = 7;
                a  = arx(D0(), K(015370));
                goto L15032;
            case 5: // identifier
                a = D0();
                goto L15036;
            case 6: // constant
                a = D0();
                goto L15072;
            default:
                goto L14775;
            }
        L14775: // *0271B: a conditional expression
            lex();
            m13 = 014777;
            goto L14762;
        L14777:
            if (D0() != 030) {
                m8 = 027;
                goto L17015;
            }
            lex();
            m1  = 1;
            m13 = 015004;
            goto L14762;
        L15004:
            m8 = 030;
            if (D0() != 031)
                goto L17015;
            lex();
            m1  = 0;
            m13 = 015010;
            goto L14762;
        L15010:
            emit(K(015334));
        L15011: // *0305B
            drop(1);
            goto L13742;
        L15013:
            m8 = 025;
            if (D0() != 045)
                goto L17015;
        L15015: // *0311B
            lex();
        L15016: // *0312B: operators by priority
            a = attr(m4_) & 0170;
            if (!a)
                goto L15011;
            m1 = (a >> 3) & R15;
            if (int(m1) - int(S(1) & R15) < 0)
                goto L15011;
            push(D0());
        L15024: // *0320B
            lex();
            m1  = (m1 + 1) & R15;
            m13 = 015026;
            goto L14762;
        L15026: // *0322B
            emit(pop());
            goto L15016;
        L15032: // *0326B: a unary operator
            push(a);
            m8 = 031;
            if (int(m1) - int(S(2) & R15) < 0)
                goto L17015;
            goto L15024;
        L15036: // *0332B: an identifier
            push(a);
            lex();
        L15037: // AG15037: a designator
            if (D0() == 020) {
                push(0);
                push(K(015334));
                goto L15043;
            }
            if (D0() == 021) {
                push(0);
                push(K(015340));
                lex();
                m1  = 0;
                m13 = 015063;
                goto L14762;
            }
            goto L15026;
        L15043: // *0337B: a call
            lex();
            m1  = 0;
            m13 = 015045;
            goto L14762;
        L15045:
            S(2) = arx(S(2), K(015262));
            if (D0() == 047)
                goto L15043;
            m8 = 032;
            if (D0() != S(1))
                goto L17015;
            if (S(2) & K(015404)) {
                m8 = 023;
                goto L17015;
            }
            a = pop();
            a |= pop();
            a |= pop();
            a -= K(015333);
        L15072: // *0366B
            emit(a);
            goto L15015;
        L15063: // a subscripted variable
            S(2) = arx(S(2), K(015262));
            if (D0() == 047)
                goto L15043;
            if (D0() == 050) {
                // *0370B
                drop(2);
                lex();
                m1  = 0;
                m13 = 015076;
                goto L14762;
            }
            if (D0() != 046) {
                m8 = 033;
                goto L17015;
            }
            drop(1);
            a = pop();
            a |= pop();
            a |= K(015303);
            goto L15072;
        L15076:
            emit(pop() | K(015347));
            if (D0() == 046)
                goto L15015;
            m8 = 033;
            goto L17015;

            //
            // AG17015: a syntax error with number M8.
            //
        L17015:
            if (++ctx_.flgex == 30)
                goto L17076;
            print_error(m8);
            if (D0() == 051 || D0() == 037)
                goto L17056;
            if (D0() == 075)
                goto L17066;
            if (ws_[D_SEMI])
                goto L17062;
            ws_[D_WARNED]   = 051;
            ws_[D_AFTEREND] = 051;
            ws_[D_KEYWORD]  = 0;
            lex();
        L17056: // *2352B: resume
            emit(044);
            ws_[D_WARNED] = 0;
            stk.resize(ws_[D_RESSP]);
            pc = ws_[D_RESPC];
            goto dispatch;
        L17062: // *2356B
            ws_[D_SEMI] = 0;
            lex();
            ws_[D_SYM] = 051;
            m4_        = 051;
            goto L17056;
        L17066: // AG17066: 'EOP' inside the program
            print_line("↑EOP↑ BEFORE END OF PROGRAM");
            ctx_.flgex = 1;
            throw Finish{};
        L17076: // *2372B: 30 errors
            ws_[D_RESPC] = 017103;
            print_line("30 ERRORS , COMPILATION INTERRUPTED");
        L17103:
            lex();
            if (D0() != 075)
                goto L17103;
            throw Finish{};
        } catch (const SyntaxError &e) {
            m8 = e.number;
            pc = 017015;
        }
    }
}

// *0124B: the formal parameter named by the symbol; its D-list address.
unsigned Pass1::find_formal()
{
    ws_[D_SYM]   = ws_[D_SYM] >> 12;
    unsigned m14 = stk_[stk_.size() - 4] & R15;
    for (;;) {
        m14    = (m14 - 1) & R15;
        Word w = ws_[m14];
        if (!w)
            throw SyntaxError{ 0115 };
        if (((w & 07777) ^ ws_[D_SYM]) == 0)
            return m14;
    }
}

// The line of a syntax error: ERROR nn and the class of the error (17166).
void Pass1::print_error(unsigned n)
{
    if (!ws_[D_LIST] && ws_[D_LINE] != ws_[D_PRINTED]) {
        print_card();
        ws_[D_PRINTED] = ws_[D_LINE];
    }
    unsigned cls = (tables::errclass.at(017166 + (n >> 3)) >> (6 * (n & 7))) & 077;
    std::string text;
    // A string of the pieces at 17121: ISO bytes, 0200+k inserts piece k.
    auto piece = [&](auto &&self, unsigned addr) -> void {
        for (;; addr++) {
            Word w = tables::syntext.at(addr);
            for (int i = 5; i >= 0; i--) {
                unsigned c = (w >> (8 * i)) & 0377;
                if (c == 0)
                    return;
                if (c & 0200)
                    self(self, 017121 + (c & 0177));
                else
                    text += char(c);
            }
        }
    };
    piece(piece, 017121 + cls);
    char num[16];
    std::snprintf(num, sizeof(num), "ERROR %02u    ", n);
    print_line(num + text);
}

Stream Pass1::run()
{
    try {
        parse();
    } catch (const NoEop &) {
        print_line("HET ′EOP′ !");
        ctx_.flgex    = 1;
        ctx_.finished = true;
    } catch (const Finish &) {
        ctx_.finished = true;
    }
    return out_;
}
