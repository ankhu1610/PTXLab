#include <gtest/gtest.h>
#include "ptxlab/kc/ast.h"
#include "ptxlab/kc/ast_printer.h"

using namespace ptxlab;

TEST(ASTTest, TypeProperties) {
    Type i32Type = Type::getI32();
    EXPECT_TRUE(i32Type.isInt());
    EXPECT_FALSE(i32Type.isPtr());
    EXPECT_EQ(i32Type.toString(), "i32");

    Type f32PtrType = Type::getF32Ptr();
    EXPECT_FALSE(f32PtrType.isInt());
    EXPECT_TRUE(f32PtrType.isPtr());
    EXPECT_EQ(f32PtrType.toString(), "f32*");

    Type boolType = Type::getBool();
    EXPECT_TRUE(boolType.isBool());
    EXPECT_EQ(boolType.toString(), "bool");
}

TEST(ASTTest, ExpressionHierarchyAndPrinter) {
    // (a + 42)
    SourceLocation loc{1, 1};
    auto lhs = std::make_unique<IdentifierExpr>(loc, "a");
    auto rhs = std::make_unique<IntLiteralExpr>(loc, 42);
    BinaryExpr addExpr(loc, TokenKind::Plus, std::move(lhs), std::move(rhs));

    std::string dump = ASTPrinter::dump(addExpr);
    std::string expected = 
        "BinaryExpr '+'\n"
        "  Identifier 'a'\n"
        "  IntLiteral 42\n";
    EXPECT_EQ(dump, expected);
}

TEST(ASTTest, KernelDeclAndProgram) {
    SourceLocation loc{1, 1};
    std::vector<ParamDecl> params;
    params.emplace_back(loc, Type::getI32(), "n");
    params.emplace_back(loc, Type::getF32Ptr(), "x");

    std::vector<std::unique_ptr<Stmt>> stmts;
    stmts.push_back(std::make_unique<ReturnStmt>(loc));
    auto body = std::make_unique<CompoundStmt>(loc, std::move(stmts));

    auto kernel = std::make_unique<KernelDecl>(loc, "dummy", std::move(params), std::move(body));

    std::vector<std::unique_ptr<KernelDecl>> kernels;
    kernels.push_back(std::move(kernel));
    Program program(loc, std::move(kernels));

    std::string dump = ASTPrinter::dump(program);
    std::string expected =
        "Program\n"
        "  KernelDecl 'dummy'\n"
        "    ParamDecl 'n' type=i32\n"
        "    ParamDecl 'x' type=f32*\n"
        "    CompoundStmt\n"
        "      ReturnStmt\n";
    EXPECT_EQ(dump, expected);
}
