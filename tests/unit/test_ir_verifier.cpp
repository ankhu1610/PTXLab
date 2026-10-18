#include <gtest/gtest.h>
#include "ptxlab/ir/ir_builder.h"
#include "ptxlab/ir/ir_verifier.h"
#include "ptxlab/ir/lower.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/kc/sema.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;
using namespace ptxlab::ir;

class IRVerifierTest : public ::testing::Test {
protected:
    SourceManager sm;
    DiagnosticEngine diag;
};

TEST_F(IRVerifierTest, ValidLoweredSaxpyPassesVerification) {
    sm.setSource("saxpy.kc", R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 tid = tid_x();
            if (tid < n) {
                y[tid] = a * x[tid] + y[tid];
            }
        }
    )");

    Lexer lexer(sm, diag);
    Parser parser(lexer, sm, diag);
    auto prog = parser.parseProgram();
    ASSERT_NE(prog, nullptr);

    Sema sema(sm, diag);
    ASSERT_TRUE(sema.analyze(*prog));

    Lowering lowerer;
    auto mod = lowerer.lower(*prog);
    ASSERT_NE(mod, nullptr);

    IRVerifier verifier;
    EXPECT_TRUE(verifier.verify(*mod));
    EXPECT_FALSE(verifier.hasErrors());
}

TEST_F(IRVerifierTest, RejectMissingTerminator) {
    Module mod;
    auto* func = mod.createFunction("bad_func");
    IRBuilder builder(func);

    // Entry block with const but NO terminator
    builder.createConstInt(10);

    IRVerifier verifier;
    EXPECT_FALSE(verifier.verify(mod));
    EXPECT_TRUE(verifier.hasErrors());

    bool foundMsg = false;
    for (const auto& err : verifier.errors()) {
        if (err.message.find("does not end with a terminator") != std::string::npos) {
            foundMsg = true;
            break;
        }
    }
    EXPECT_TRUE(foundMsg);
}

TEST_F(IRVerifierTest, RejectTerminatorInMiddle) {
    Module mod;
    auto* func = mod.createFunction("bad_func");
    IRBuilder builder(func);

    builder.createRet();       // Terminator in middle
    builder.createConstInt(10); // Instruction after terminator

    IRVerifier verifier;
    EXPECT_FALSE(verifier.verify(mod));
    EXPECT_TRUE(verifier.hasErrors());

    bool foundMsg = false;
    for (const auto& err : verifier.errors()) {
        if (err.message.find("terminator found before the end") != std::string::npos) {
            foundMsg = true;
            break;
        }
    }
    EXPECT_TRUE(foundMsg);
}

TEST_F(IRVerifierTest, RejectInvalidBranchTarget) {
    Module mod;
    auto* func = mod.createFunction("bad_func");
    auto* entry = func->entryBlock();

    auto inst = std::make_unique<Instruction>(Opcode::Br);
    inst->setBranchTargets("non_existent_block");
    entry->appendInstruction(std::move(inst));

    IRVerifier verifier;
    EXPECT_FALSE(verifier.verify(mod));
    EXPECT_TRUE(verifier.hasErrors());

    bool foundMsg = false;
    for (const auto& err : verifier.errors()) {
        if (err.message.find("does not exist in function") != std::string::npos) {
            foundMsg = true;
            break;
        }
    }
    EXPECT_TRUE(foundMsg);
}

TEST_F(IRVerifierTest, RejectMismatchedBinaryTypes) {
    Module mod;
    auto* func = mod.createFunction("bad_func");
    IRBuilder builder(func);

    auto intOp = builder.createConstInt(10);
    auto floatOp = builder.createConstFloat(2.5f);

    // Attempt integer add on float operand
    auto badAdd = std::make_unique<Instruction>(Opcode::Add);
    badAdd->setDst(IROperand::reg(func->nextRegId(), IRType::i32()));
    badAdd->addOperand(intOp);
    badAdd->addOperand(floatOp);
    func->entryBlock()->appendInstruction(std::move(badAdd));

    builder.createRet();

    IRVerifier verifier;
    EXPECT_FALSE(verifier.verify(mod));
    EXPECT_TRUE(verifier.hasErrors());

    bool foundMsg = false;
    for (const auto& err : verifier.errors()) {
        if (err.message.find("requires i32 operands") != std::string::npos) {
            foundMsg = true;
            break;
        }
    }
    EXPECT_TRUE(foundMsg);
}

TEST_F(IRVerifierTest, RejectEntryBlockWithPredecessors) {
    Module mod;
    auto* func = mod.createFunction("bad_func");
    IRBuilder builder(func);

    auto* entry = func->entryBlock();
    auto* loopBB = func->createBlock("loop");

    builder.setInsertPoint(entry);
    builder.createBr(loopBB);

    builder.setInsertPoint(loopBB);
    builder.createBr(entry); // ILLEGAL: jumps back to entry block!

    func->rebuildCFG();

    IRVerifier verifier;
    EXPECT_FALSE(verifier.verify(mod));
    EXPECT_TRUE(verifier.hasErrors());

    bool foundMsg = false;
    for (const auto& err : verifier.errors()) {
        if (err.message.find("entry block 'entry' must have zero predecessors") != std::string::npos) {
            foundMsg = true;
            break;
        }
    }
    EXPECT_TRUE(foundMsg);
}
