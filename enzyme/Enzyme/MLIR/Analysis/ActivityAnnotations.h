#ifndef ENZYME_MLIR_ANALYSIS_ACTIVITYANNOTATIONS_H
#define ENZYME_MLIR_ANALYSIS_ACTIVITYANNOTATIONS_H

#include "AliasAnalysis.h"
#include "Dialect/Ops.h"
#include "Lattice.h"

#include "mlir/Analysis/DataFlow/DenseAnalysis.h"
#include "mlir/Analysis/DataFlow/SparseAnalysis.h"
#include "mlir/Analysis/DataFlowFramework.h"

namespace mlir {
class FunctionOpInterface;

namespace enzyme {

using ValueOriginSet = SetLattice<OriginAttr>;

//===----------------------------------------------------------------------===//
// ForwardOriginsLattice
//===----------------------------------------------------------------------===//

// TODO: specialize this to only arguments
class ForwardOriginsLattice : public SparseSetLattice<OriginAttr> {
public:
  using SparseSetLattice::SparseSetLattice;

  static ForwardOriginsLattice single(Value point, OriginAttr value) {
    return ForwardOriginsLattice(point, SetLattice<OriginAttr>(value));
  }

  void print(raw_ostream &os) const override;

  ChangeResult join(const AbstractSparseLattice &other) override;

  const DenseSet<OriginAttr> &getOrigins() const {
    return elements.getElements();
  }

  const SetLattice<OriginAttr> &getOriginsObject() const { return elements; }
};

class BackwardOriginsLattice : public SparseSetLattice<OriginAttr> {
public:
  using SparseSetLattice::SparseSetLattice;

  static BackwardOriginsLattice single(Value point, OriginAttr value) {
    return BackwardOriginsLattice(point, SetLattice<OriginAttr>(value));
  }

  void print(raw_ostream &os) const override;

  ChangeResult meet(const AbstractSparseLattice &other) override {
    // MLIR framework again misusing terminology
    const auto *otherValueOrigins =
        static_cast<const BackwardOriginsLattice *>(&other);
    return elements.join(otherValueOrigins->elements);
  }

  const DenseSet<OriginAttr> &getOrigins() const {
    return elements.getElements();
  }

  const SetLattice<OriginAttr> &getOriginsObject() const { return elements; }
};

class ForwardActivityAnnotationAnalysis
    : public dataflow::SparseForwardDataFlowAnalysis<ForwardOriginsLattice> {
public:
  ForwardActivityAnnotationAnalysis(DataFlowSolver &solver)
      : SparseForwardDataFlowAnalysis(solver) {
    assert(!solver.getConfig().isInterprocedural());
  }

  void setToEntryState(ForwardOriginsLattice *lattice) override;

  void visitOperation(Operation *op,
                      ArrayRef<const ForwardOriginsLattice *> operands,
                      ArrayRef<ForwardOriginsLattice *> results) override;

  void visitExternalCall(CallOpInterface call,
                         ArrayRef<const ForwardOriginsLattice *> operands,
                         ArrayRef<ForwardOriginsLattice *> results) override;

private:
  void processMemoryRead(Operation *op, Value address,
                         ArrayRef<ForwardOriginsLattice *> results);

  void
  processCallToSummarizedFunc(CallOpInterface call,
                              ArrayRef<ValueOriginSet> summary,
                              ArrayRef<const ForwardOriginsLattice *> operands,
                              ArrayRef<ForwardOriginsLattice *> results);
};

class BackwardActivityAnnotationAnalysis
    : public dataflow::SparseBackwardDataFlowAnalysis<BackwardOriginsLattice> {
public:
  BackwardActivityAnnotationAnalysis(DataFlowSolver &solver,
                                     SymbolTableCollection &symbolTable)
      : SparseBackwardDataFlowAnalysis(solver, symbolTable) {
    assert(!solver.getConfig().isInterprocedural());
  }

  void visitBranchOperand(OpOperand &operand) override {}

  void visitCallOperand(OpOperand &operand) override {}

  void setToExitState(BackwardOriginsLattice *lattice) override;

  void
  visitOperation(Operation *op, ArrayRef<BackwardOriginsLattice *> operands,
                 ArrayRef<const BackwardOriginsLattice *> results) override;

  void
  visitExternalCall(CallOpInterface call,
                    ArrayRef<BackwardOriginsLattice *> operands,
                    ArrayRef<const BackwardOriginsLattice *> results) override;

private:
  void
  processCallToSummarizedFunc(CallOpInterface call,
                              ArrayRef<ValueOriginSet> summary,
                              ArrayRef<BackwardOriginsLattice *> operands,
                              ArrayRef<const BackwardOriginsLattice *> results);
};

//===----------------------------------------------------------------------===//
// ForwardOriginsMap
//===----------------------------------------------------------------------===//

class ForwardOriginsMap : public MapOfSetsLattice<DistinctAttr, OriginAttr> {
public:
  using MapOfSetsLattice::MapOfSetsLattice;

  void print(raw_ostream &os) const override;

  ChangeResult markAllOriginsUnknown() { return markAllUnknown(); }

  const ValueOriginSet &getOrigins(DistinctAttr id) const { return lookup(id); }
};

class BackwardOriginsMap : public MapOfSetsLattice<DistinctAttr, OriginAttr> {
public:
  using MapOfSetsLattice::MapOfSetsLattice;

  void print(raw_ostream &os) const override;

  ChangeResult markAllOriginsUnknown() { return markAllUnknown(); }

  const ValueOriginSet &getOrigins(DistinctAttr id) const { return lookup(id); }

  ChangeResult meet(const AbstractDenseLattice &other) override {
    return join(other);
  }
};

//===----------------------------------------------------------------------===//
// DenseActivityAnnotationAnalysis
//===----------------------------------------------------------------------===//

class DenseActivityAnnotationAnalysis
    : public dataflow::DenseForwardDataFlowAnalysis<ForwardOriginsMap> {
public:
  using DenseForwardDataFlowAnalysis::DenseForwardDataFlowAnalysis;

  void setToEntryState(ForwardOriginsMap *lattice) override;

  void visitOperation(Operation *op, const ForwardOriginsMap &before,
                      ForwardOriginsMap *after) override;

  void visitCallControlFlowTransfer(CallOpInterface call,
                                    dataflow::CallControlFlowAction action,
                                    const ForwardOriginsMap &before,
                                    ForwardOriginsMap *after) override;

private:
  void processCallToSummarizedFunc(
      CallOpInterface call,
      const DenseMap<DistinctAttr, ValueOriginSet> &summary,
      const ForwardOriginsMap &before, ForwardOriginsMap *after);

  void processCopy(Operation *op, Value copySource, Value copyDest,
                   const ForwardOriginsMap &before, ForwardOriginsMap *after);

  OriginalClasses originalClasses;
};

class DenseBackwardActivityAnnotationAnalysis
    : public dataflow::DenseBackwardDataFlowAnalysis<BackwardOriginsMap> {
public:
  using DenseBackwardDataFlowAnalysis::DenseBackwardDataFlowAnalysis;

  void visitOperation(Operation *op, const BackwardOriginsMap &after,
                      BackwardOriginsMap *before) override;

  void visitCallControlFlowTransfer(CallOpInterface call,
                                    dataflow::CallControlFlowAction action,
                                    const BackwardOriginsMap &after,
                                    BackwardOriginsMap *before) override;

  void setToExitState(BackwardOriginsMap *lattice) override;

private:
  void processCallToSummarizedFunc(
      CallOpInterface call,
      const DenseMap<DistinctAttr, ValueOriginSet> &summary,
      const BackwardOriginsMap &after, BackwardOriginsMap *before);

  void processCopy(Operation *op, Value copySource, Value copyDest,
                   const BackwardOriginsMap &after, BackwardOriginsMap *before);
};

void runActivityAnnotations(FunctionOpInterface callee);

} // namespace enzyme
} // namespace mlir

#endif // ENZYME_MLIR_ANALYSIS_ACTIVITYANNOTATIONS_H
