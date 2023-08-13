//===- EnzymeToMemRef.cpp - Lower custom Enzyme operations ------------------ //
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass to lower custom ops generated by the Enzyme AD
// procedure to the MemRef dialect.
//===----------------------------------------------------------------------===//

#include "Dialect/Dialect.h"
#include "Dialect/Ops.h"
#include "PassDetails.h"
#include "Passes/Passes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;
using llvm::errs;
namespace {
struct LoweredCache {
  Value elements, size, capacity;
  Type elementType;

  void allocateCache(Location loc, OpBuilder &b) {}

  FlatSymbolRefAttr getOrInsertPushFunction(Location loc, ModuleOp moduleOp,
                                            OpBuilder &b) const {
    MLIRContext *context = b.getContext();
    std::string funcName = "__enzyme_push_";
    llvm::raw_string_ostream funcStream{funcName};
    funcStream << elementType;
    if (moduleOp.lookupSymbol<func::FuncOp>(funcName)) {
      return SymbolRefAttr::get(context, funcName);
    }

    OpBuilder::InsertionGuard insertionGuard(b);
    b.setInsertionPointToStart(moduleOp.getBody());

    auto pushFnType = FunctionType::get(
        context, /*inputs=*/
        {elements.getType(), size.getType(), capacity.getType(), elementType},
        /*outputs=*/{});
    auto pushFn = b.create<func::FuncOp>(loc, funcName, pushFnType);
    pushFn.setPrivate();
    Block *entryBlock = pushFn.addEntryBlock();
    b.setInsertionPointToStart(entryBlock);
    BlockArgument elementsField = pushFn.getArgument(0);
    BlockArgument sizeField = pushFn.getArgument(1);
    BlockArgument capacityField = pushFn.getArgument(2);
    BlockArgument value = pushFn.getArgument(3);

    Value sizeVal = b.create<memref::LoadOp>(loc, sizeField);
    Value capacityVal = b.create<memref::LoadOp>(loc, capacityField);

    Value predicate = b.create<arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                              sizeVal, capacityVal);
    b.create<scf::IfOp>(
        loc, predicate, [&](OpBuilder &thenBuilder, Location loc) {
          Value two = thenBuilder.create<arith::ConstantIndexOp>(loc, 2);
          Value newCapacity =
              thenBuilder.create<arith::MulIOp>(loc, capacityVal, two);
          Value oldElements =
              thenBuilder.create<memref::LoadOp>(loc, elementsField);
          Value newElements = thenBuilder.create<memref::AllocOp>(
              loc, oldElements.getType().cast<MemRefType>(), newCapacity);
          thenBuilder.create<memref::CopyOp>(loc, oldElements, newElements);
          thenBuilder.create<memref::DeallocOp>(loc, oldElements);
          thenBuilder.create<memref::StoreOp>(loc, newElements, elementsField);
          thenBuilder.create<memref::StoreOp>(loc, newCapacity, capacityField);
          thenBuilder.create<scf::YieldOp>(loc);
        });

    Value elementsVal = b.create<memref::LoadOp>(loc, elementsField);
    b.create<memref::StoreOp>(loc, value, elementsVal,
                              /*indices=*/sizeVal);

    Value one = b.create<arith::ConstantIndexOp>(loc, 1);
    Value newSize = b.create<arith::AddIOp>(loc, sizeVal, one);

    b.create<memref::StoreOp>(loc, newSize, sizeField);
    b.create<func::ReturnOp>(loc);

    return SymbolRefAttr::get(context, funcName);
  }

  FlatSymbolRefAttr getOrInsertPopFunction(Location loc, ModuleOp moduleOp,
                                           OpBuilder &b) {
    MLIRContext *context = b.getContext();
    std::string funcName = "__enzyme_pop_";
    llvm::raw_string_ostream funcStream{funcName};
    funcStream << elementType;
    if (moduleOp.lookupSymbol<func::FuncOp>(funcName)) {
      return SymbolRefAttr::get(context, funcName);
    }

    OpBuilder::InsertionGuard insertionGuard(b);
    b.setInsertionPointToStart(moduleOp.getBody());
    auto popFnType = FunctionType::get(
        context,
        /*inputs=*/{elements.getType(), size.getType(), capacity.getType()},
        /*outputs=*/elementType);
    auto popFn = b.create<func::FuncOp>(loc, funcName, popFnType);
    popFn.setPrivate();
    Block *entryBlock = popFn.addEntryBlock();
    b.setInsertionPointToStart(entryBlock);
    BlockArgument elementsField = popFn.getArgument(0);
    BlockArgument sizeField = popFn.getArgument(1);

    Value elementsVal = b.create<memref::LoadOp>(loc, elementsField);
    Value sizeVal = b.create<memref::LoadOp>(loc, sizeField);
    Value zero = b.create<arith::ConstantIndexOp>(loc, 0);
    Value pred =
        b.create<arith::CmpIOp>(loc, arith::CmpIPredicate::sgt, sizeVal, zero);
    b.create<cf::AssertOp>(loc, pred, "pop on empty cache");

    Value one = b.create<arith::ConstantIndexOp>(loc, 1);
    Value newSize = b.create<arith::SubIOp>(loc, sizeVal, one);
    b.create<memref::StoreOp>(loc, newSize, sizeField);

    Value result = b.create<memref::LoadOp>(loc, elementsVal, newSize);
    b.create<func::ReturnOp>(loc, result);
    return SymbolRefAttr::get(context, funcName);
  }

  void emitPush(Location loc, Value value, OpBuilder &b,
                FlatSymbolRefAttr pushFn) const {
    b.create<func::CallOp>(
        loc, pushFn, /*results=*/TypeRange{},
        /*operands=*/ValueRange{elements, size, capacity, value});
  }

  Value emitPop(Location loc, OpBuilder &b, FlatSymbolRefAttr popFn) const {
    return b
        .create<func::CallOp>(loc, popFn, /*results=*/
                              elements.getType()
                                  .cast<ShapedType>()
                                  .getElementType()
                                  .cast<ShapedType>()
                                  .getElementType(),
                              ValueRange{elements, size, capacity})
        .getResult(0);
  }

  FlatSymbolRefAttr getOrInsertGetFunction(Location loc, ModuleOp moduleOp,
                                           OpBuilder &b) {
    MLIRContext *context = b.getContext();
    std::string funcName = "__enzyme_get_";
    llvm::raw_string_ostream funcStream{funcName};
    funcStream << elementType;
    if (moduleOp.lookupSymbol<func::FuncOp>(funcName)) {
      return SymbolRefAttr::get(context, funcName);
    }

    OpBuilder::InsertionGuard insertionGuard(b);
    b.setInsertionPointToStart(moduleOp.getBody());
    auto popFnType = FunctionType::get(
        context,
        /*inputs=*/{elements.getType(), size.getType(), capacity.getType()},
        /*outputs=*/elementType);
    auto popFn = b.create<func::FuncOp>(loc, funcName, popFnType);
    popFn.setPrivate();
    Block *entryBlock = popFn.addEntryBlock();
    b.setInsertionPointToStart(entryBlock);
    BlockArgument elementsField = popFn.getArgument(0);
    BlockArgument sizeField = popFn.getArgument(1);

    Value elementsVal = b.create<memref::LoadOp>(loc, elementsField);
    Value sizeVal = b.create<memref::LoadOp>(loc, sizeField);
    Value zero = b.create<arith::ConstantIndexOp>(loc, 0);
    Value one = b.create<arith::ConstantIndexOp>(loc, 1);
    Value pred =
        b.create<arith::CmpIOp>(loc, arith::CmpIPredicate::sgt, sizeVal, zero);
    b.create<cf::AssertOp>(loc, pred, "get on empty cache");

    Value lastIndex = b.create<arith::SubIOp>(loc, sizeVal, one);
    Value result = b.create<memref::LoadOp>(loc, elementsVal, lastIndex);
    b.create<func::ReturnOp>(loc, result);
    return SymbolRefAttr::get(context, funcName);
  }

  Value emitGet(Location loc, OpBuilder &b, FlatSymbolRefAttr getFn) const {
    return b
        .create<func::CallOp>(loc, getFn, /*results=*/
                              elements.getType()
                                  .cast<ShapedType>()
                                  .getElementType()
                                  .cast<ShapedType>()
                                  .getElementType(),
                              ValueRange{elements, size, capacity})
        .getResult(0);
  }
  static std::optional<LoweredCache>
  getFromEnzymeCache(Location loc, TypeConverter *typeConverter,
                     Value enzymeCache, OpBuilder &b) {
    assert(enzymeCache.getType().isa<enzyme::CacheType>());
    auto cacheType = enzymeCache.getType().cast<enzyme::CacheType>();
    SmallVector<Type> resultTypes;
    if (failed(typeConverter->convertType(cacheType, resultTypes))) {
      return {};
    }
    auto unpackedCache =
        b.create<UnrealizedConversionCastOp>(loc, resultTypes, enzymeCache);
    return LoweredCache{.elements = unpackedCache.getResult(0),
                        .size = unpackedCache.getResult(1),
                        .capacity = unpackedCache.getResult(2),
                        .elementType = cacheType.getType()};
  }
};

struct InitOpConversion : public OpConversionPattern<enzyme::InitOp> {
  using OpConversionPattern::OpConversionPattern;

  LogicalResult
  matchAndRewrite(enzyme::InitOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    // `enzyme.init` is overloaded to initialize both gradients and caches.
    // Gradients lower to single element MemRefs, while caches lower to
    // variable-sized MemRefs.

    if (op.getType().isa<enzyme::GradientType>()) {
      auto memrefType =
          getTypeConverter()->convertType(op.getType()).cast<MemRefType>();
      Value buffer = rewriter.create<memref::AllocOp>(op.getLoc(), memrefType);
      rewriter.replaceOpWithNewOp<UnrealizedConversionCastOp>(op, op.getType(),
                                                              buffer);
      return success();
    }

    if (op.getType().isa<enzyme::CacheType>()) {
      SmallVector<Type> resultTypes;
      if (failed(getTypeConverter()->convertType(op.getType(), resultTypes))) {
        op.emitError() << "Failed to convert type " << op.getType();
        return failure();
      }

      Value capacity = rewriter.create<arith::ConstantIndexOp>(op.getLoc(), 1);
      Value initialSize =
          rewriter.create<arith::ConstantIndexOp>(op.getLoc(), 0);
      auto dataType = resultTypes[0].cast<MemRefType>();
      auto sizeType = resultTypes[1].cast<MemRefType>();
      auto capacityType = resultTypes[2].cast<MemRefType>();
      Value buffer = rewriter.create<memref::AllocOp>(
          op.getLoc(), dataType.getElementType().cast<MemRefType>(),
          /*dynamicSize=*/capacity);
      Value bufferField =
          rewriter.create<memref::AllocaOp>(op.getLoc(), dataType);
      Value sizeField =
          rewriter.create<memref::AllocaOp>(op.getLoc(), sizeType);
      Value capacityField =
          rewriter.create<memref::AllocaOp>(op.getLoc(), capacityType);
      rewriter.create<memref::StoreOp>(op.getLoc(), buffer, bufferField);
      rewriter.create<memref::StoreOp>(op.getLoc(), initialSize, sizeField);
      rewriter.create<memref::StoreOp>(op.getLoc(), capacity, capacityField);
      rewriter.replaceOpWithNewOp<UnrealizedConversionCastOp>(
          op, op.getType(), ValueRange{bufferField, sizeField, capacityField});
      return success();
    }

    // TODO: Add verification to the init op to verify the valid types,
    // or break out init gradient semantics from init cache semantics
    op.emitError() << "Expected cache or gradient type but got: "
                   << op.getType();
    return failure();
  }
};

struct PushOpConversion : public OpConversionPattern<enzyme::PushOp> {
  using OpConversionPattern::OpConversionPattern;

  LogicalResult
  matchAndRewrite(enzyme::PushOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    Location loc = op.getLoc();
    auto loweredCache = LoweredCache::getFromEnzymeCache(
        loc, getTypeConverter(), op.getCache(), rewriter);
    if (!loweredCache.has_value()) {
      return failure();
    }

    FlatSymbolRefAttr pushFn = loweredCache.value().getOrInsertPushFunction(
        loc, op->getParentOfType<ModuleOp>(), rewriter);
    loweredCache.value().emitPush(loc, op.getValue(), rewriter, pushFn);
    rewriter.eraseOp(op);
    return success();
  }
};

struct PopOpConversion : public OpConversionPattern<enzyme::PopOp> {
  using OpConversionPattern::OpConversionPattern;

  LogicalResult
  matchAndRewrite(enzyme::PopOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    Location loc = op.getLoc();
    auto loweredCache = LoweredCache::getFromEnzymeCache(
        loc, getTypeConverter(), op.getCache(), rewriter);
    if (!loweredCache.has_value()) {
      return failure();
    }

    FlatSymbolRefAttr popFn = loweredCache.value().getOrInsertPopFunction(
        loc, op->getParentOfType<ModuleOp>(), rewriter);
    rewriter.replaceOp(op, loweredCache.value().emitPop(loc, rewriter, popFn));
    return success();
  }
};

struct SetOpConversion : public OpConversionPattern<enzyme::SetOp> {
  using OpConversionPattern::OpConversionPattern;

  LogicalResult
  matchAndRewrite(enzyme::SetOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    if (auto type = dyn_cast<enzyme::CacheType>(op.getGradient().getType())) {
      op.emitError() << "set for CacheType not implemented";
      return failure();
    } else if (auto type =
                   dyn_cast<enzyme::GradientType>(op.getGradient().getType())) {
      auto memrefType =
          getTypeConverter()->convertType(type).cast<MemRefType>();
      auto castedGradient = rewriter.create<UnrealizedConversionCastOp>(
          op.getLoc(), memrefType, op.getGradient());
      rewriter.replaceOpWithNewOp<memref::StoreOp>(op, op.getValue(),
                                                   castedGradient.getResult(0));
    }
    return success();
  }
};

struct GetOpConversion : public OpConversionPattern<enzyme::GetOp> {
  using OpConversionPattern<enzyme::GetOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(enzyme::GetOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {

    if (auto type = dyn_cast<enzyme::CacheType>(op.getGradient().getType())) {
      Location loc = op.getLoc();
      auto loweredCache = LoweredCache::getFromEnzymeCache(
          loc, getTypeConverter(), op.getGradient(), rewriter);
      if (!loweredCache.has_value()) {
        return failure();
      }

      FlatSymbolRefAttr getFn = loweredCache.value().getOrInsertGetFunction(
          loc, op->getParentOfType<ModuleOp>(), rewriter);
      rewriter.replaceOp(op,
                         loweredCache.value().emitGet(loc, rewriter, getFn));
    } else if (auto type =
                   dyn_cast<enzyme::GradientType>(op.getGradient().getType())) {
      auto memrefType =
          getTypeConverter()->convertType(type).cast<MemRefType>();
      auto castedGradient = rewriter.create<UnrealizedConversionCastOp>(
          op.getLoc(), memrefType, op.getGradient());
      rewriter.replaceOpWithNewOp<memref::LoadOp>(op,
                                                  castedGradient.getResult(0));
    }
    return success();
  }
};

struct EnzymeToMemRefPass
    : public enzyme::EnzymeOpsToMemRefPassBase<EnzymeToMemRefPass> {
  void runOnOperation() override {
    MLIRContext *context = &getContext();
    RewritePatternSet patterns(context);
    TypeConverter typeConverter;
    typeConverter.addConversion([](Type type) -> std::optional<Type> {
      if (type.isIntOrIndexOrFloat() || type.isa<MemRefType>())
        return type;
      return {};
    });
    typeConverter.addConversion(
        [](enzyme::GradientType type) -> std::optional<Type> {
          return MemRefType::get({}, type.getBasetype());
        });
    typeConverter.addConversion(
        [](enzyme::CacheType type, SmallVectorImpl<Type> &resultTypes) {
          // Data
          resultTypes.push_back(MemRefType::get(
              {}, MemRefType::get({ShapedType::kDynamic}, type.getType())));
          auto indexMemRefType =
              MemRefType::get({}, IndexType::get(type.getContext()));
          // Size
          resultTypes.push_back(indexMemRefType);
          // Capacity
          resultTypes.push_back(indexMemRefType);
          return success();
        });

    patterns.add<InitOpConversion>(typeConverter, context);
    patterns.add<PushOpConversion>(typeConverter, context);
    patterns.add<PopOpConversion>(typeConverter, context);
    patterns.add<SetOpConversion>(typeConverter, context);
    patterns.add<GetOpConversion>(typeConverter, context);

    ConversionTarget target(*context);
    target.addLegalDialect<memref::MemRefDialect>();
    target.addLegalDialect<arith::ArithDialect>();
    target.addLegalDialect<scf::SCFDialect>();
    target.addLegalDialect<cf::ControlFlowDialect>();
    target.addLegalDialect<func::FuncDialect>();
    target.addLegalOp<UnrealizedConversionCastOp>();
    target.addIllegalDialect<enzyme::EnzymeDialect>();

    if (failed(applyPartialConversion(getOperation(), target,
                                      std::move(patterns))))
      signalPassFailure();
  };
};
} // end anonymous namespace

namespace mlir {
namespace enzyme {
std::unique_ptr<Pass> createEnzymeToMemRefPass() {
  return std::make_unique<EnzymeToMemRefPass>();
}
} // namespace enzyme
} // namespace mlir
