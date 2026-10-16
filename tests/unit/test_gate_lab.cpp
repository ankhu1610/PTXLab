#include <gtest/gtest.h>
#include "grammar.h"
#include "first_follow.h"
#include "ll1_table.h"

using namespace ptxlab::gate;

class GateLabTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup classic textbook expression grammar:
        // E  -> T E'
        // E' -> + T E' | ε
        // T  -> F T'
        // T' -> * F T' | ε
        // F  -> ( E ) | id
        E  = Symbol::nonTerminal("E");
        Ep = Symbol::nonTerminal("E'");
        T  = Symbol::nonTerminal("T");
        Tp = Symbol::nonTerminal("T'");
        F  = Symbol::nonTerminal("F");

        plus   = Symbol::terminal("+");
        star   = Symbol::terminal("*");
        lparen = Symbol::terminal("(");
        rparen = Symbol::terminal(")");
        id     = Symbol::terminal("id");

        exprGrammar.setStartSymbol(E);
        exprGrammar.addProduction(E, {T, Ep});
        exprGrammar.addProduction(Ep, {plus, T, Ep});
        exprGrammar.addProduction(Ep, {Symbol::epsilon()});
        exprGrammar.addProduction(T, {F, Tp});
        exprGrammar.addProduction(Tp, {star, F, Tp});
        exprGrammar.addProduction(Tp, {Symbol::epsilon()});
        exprGrammar.addProduction(F, {lparen, E, rparen});
        exprGrammar.addProduction(F, {id});
    }

    Grammar exprGrammar;
    Symbol E, Ep, T, Tp, F;
    Symbol plus, star, lparen, rparen, id;
};

TEST_F(GateLabTest, TextbookExpressionFirstSets) {
    auto first = computeFirstSets(exprGrammar);

    std::set<Symbol> expectedF{lparen, id};
    std::set<Symbol> expectedT{lparen, id};
    std::set<Symbol> expectedE{lparen, id};
    std::set<Symbol> expectedEp{plus, Symbol::epsilon()};
    std::set<Symbol> expectedTp{star, Symbol::epsilon()};

    EXPECT_EQ(first[F], expectedF);
    EXPECT_EQ(first[T], expectedT);
    EXPECT_EQ(first[E], expectedE);
    EXPECT_EQ(first[Ep], expectedEp);
    EXPECT_EQ(first[Tp], expectedTp);
}

TEST_F(GateLabTest, TextbookExpressionFollowSets) {
    auto first = computeFirstSets(exprGrammar);
    auto follow = computeFollowSets(exprGrammar, first);

    Symbol dollar = Symbol::endOfInput();

    std::set<Symbol> expectedE{rparen, dollar};
    std::set<Symbol> expectedEp{rparen, dollar};
    std::set<Symbol> expectedT{plus, rparen, dollar};
    std::set<Symbol> expectedTp{plus, rparen, dollar};
    std::set<Symbol> expectedF{plus, star, rparen, dollar};

    EXPECT_EQ(follow[E], expectedE);
    EXPECT_EQ(follow[Ep], expectedEp);
    EXPECT_EQ(follow[T], expectedT);
    EXPECT_EQ(follow[Tp], expectedTp);
    EXPECT_EQ(follow[F], expectedF);
}

TEST_F(GateLabTest, TextbookExpressionIsLL1) {
    auto first = computeFirstSets(exprGrammar);
    auto follow = computeFollowSets(exprGrammar, first);
    auto table = LL1Table::build(exprGrammar, first, follow);

    EXPECT_TRUE(table.isLL1());
    EXPECT_TRUE(table.getConflicts().empty());
}

TEST_F(GateLabTest, DanglingElseReportsLL1Conflict) {
    // S  -> i E t S S' | a
    // S' -> e S | ε
    // E  -> b
    Grammar g(Symbol::nonTerminal("S"));
    Symbol S  = Symbol::nonTerminal("S");
    Symbol Sp = Symbol::nonTerminal("S'");
    Symbol E  = Symbol::nonTerminal("E");

    Symbol i = Symbol::terminal("i");
    Symbol t = Symbol::terminal("t");
    Symbol e = Symbol::terminal("e");
    Symbol a = Symbol::terminal("a");
    Symbol b = Symbol::terminal("b");

    g.addProduction(S, {i, E, t, S, Sp});
    g.addProduction(S, {a});
    g.addProduction(Sp, {e, S});
    g.addProduction(Sp, {Symbol::epsilon()});
    g.addProduction(E, {b});

    auto first = computeFirstSets(g);
    auto follow = computeFollowSets(g, first);
    auto table = LL1Table::build(g, first, follow);

    EXPECT_FALSE(table.isLL1());
    ASSERT_EQ(table.getConflicts().size(), 1);
    EXPECT_EQ(table.getConflicts()[0].nonTerminal.name, "S'");
    EXPECT_EQ(table.getConflicts()[0].terminal.name, "e");
}
