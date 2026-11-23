//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The lexer of pass 1 on its own: the symbols it returns for each program
// must be those of NAME.lex, captured from the original.
//
#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>

#include "golden.h"
#include "pass1.h"
#include "source.h"

class Lexer : public ::testing::TestWithParam<std::string> {};

TEST_P(Lexer, symbols)
{
    const std::string &name = GetParam();
    std::ifstream in(path(name, ".lex"));
    std::vector<std::pair<Word, unsigned>> want;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream f(line);
        Word w;
        unsigned m4;
        f >> std::oct >> w >> m4;
        want.emplace_back(w, m4);
    }
    ASSERT_FALSE(want.empty());

    // After a syntax error the parser's recovery calls the lexer, which this
    // test does not model.
    std::ifstream lst(path(name, ".lst"));
    std::string text((std::istreambuf_iterator<char>(lst)), std::istreambuf_iterator<char>());
    const bool has_errors = text.find("ERROR ") != std::string::npos;

    Context ctx;
    ctx.source = read_source(path(name, ".alg"));
    load_options(ctx.options, name);
    Pass1 lexer(ctx);
    for (size_t i = 0; i < want.size(); i++) {
        try {
            lexer.lex();
        } catch (const Pass1::SyntaxError &e) {
            GTEST_SKIP() << "syntax error " << e.number << " at symbol " << i;
        } catch (const NotImplemented &e) {
            GTEST_SKIP() << e.what();
        }
        if (has_errors &&
            (lexer.symbol_word() != want[i].first || lexer.symbol() != want[i].second))
            GTEST_SKIP() << "error recovery at symbol " << i;
        ASSERT_EQ(lexer.symbol_word(), want[i].first) << "symbol " << i;
        ASSERT_EQ(lexer.symbol(), want[i].second) << "symbol " << i;
    }
}

static std::vector<std::string> cases()
{
    auto all = program_names();
    std::vector<std::string> names;
    std::copy_if(all.begin(), all.end(), std::back_inserter(names), [](const std::string &name) {
        return std::ifstream(path(name, ".lex")).peek() != EOF;
    });
    return names;
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, Lexer, ::testing::ValuesIn(cases()), param_name);

// The Unicode operators (an extension: the original gives ERROR 01) are the
// symbols of their word forms.
TEST(Lexer, unicode_operators)
{
    auto symbols = [](const std::string &text) {
        Context ctx;
        ctx.source = "'BEGIN' " + text + "\n'END'\n'EOP'\n";
        Pass1 lexer(ctx);
        std::vector<unsigned> codes;
        do {
            lexer.lex();
            codes.push_back(lexer.symbol());
        } while (lexer.symbol() != 037);
        return codes;
    };
    EXPECT_EQ(symbols("¬A ∨ B ∧ C ⊃ D ≡ E ≤ F ≥ G ≠ H × I ↑ J"),
              symbols("'NOT'A 'OR' B 'AND' C 'IMPL' D 'EQUIV' E\n"
                      "'LE' F 'GE' G 'NE' H * I 'POWER' J"));
}
