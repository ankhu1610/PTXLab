#include <gtest/gtest.h>
#include "ptxlab/ir/function.h"
#include "ptxlab/ir/instruction.h"
#include "ptxlab/ir/ir_builder.h"
#include "ptxlab/ir/ir_printer.h"
#include "ptxlab/ir/module.h"

using namespace ptxlab::ir;

TEST(IRTest, InstructionFormatting) {
    Module mod;
    auto* func = mod.createFunction("test_kernel");
    IRBuilder builder(func);

    auto v0 = builder.createConstInt(42);
    auto v1 = builder.createConstInt(10);
    auto v2 = builder.createAdd(v0, v1);
    (void)v2;
    builder.createRet();

    std::string dump = IRPrinter::dump(*func);
    EXPECT_NE(dump.find("kernel test_kernel()"), std::string::npos);
    EXPECT_NE(dump.find("%v0 = const i32 42"), std::string::npos);
    EXPECT_NE(dump.find("%v1 = const i32 10"), std::string::npos);
    EXPECT_NE(dump.find("%v2 = add %v0, %v1"), std::string::npos);
    EXPECT_NE(dump.find("ret"), std::string::npos);
}

TEST(IRTest, CFGConstructionAndEdgeTracking) {
    Module mod;
    auto* func = mod.createFunction("test_cfg");
    IRBuilder builder(func);

    auto* entry = func->entryBlock();
    auto* thenBB = func->createBlock("then");
    auto* elseBB = func->createBlock("else");
    auto* joinBB = func->createBlock("join");

    builder.setInsertPoint(entry);
    auto p0 = builder.createConstBool(true);
    builder.createCondBr(p0, thenBB, elseBB);

    builder.setInsertPoint(thenBB);
    builder.createBr(joinBB);

    builder.setInsertPoint(elseBB);
    builder.createBr(joinBB);

    builder.setInsertPoint(joinBB);
    builder.createRet();

    func->rebuildCFG();

    EXPECT_EQ(entry->successors().size(), 2);
    EXPECT_EQ(thenBB->predecessors().size(), 1);
    EXPECT_EQ(elseBB->predecessors().size(), 1);
    EXPECT_EQ(joinBB->predecessors().size(), 2);
    EXPECT_EQ(joinBB->successors().size(), 0);
}
