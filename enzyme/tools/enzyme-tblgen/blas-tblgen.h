#include <llvm/TableGen/Record.h>

void emitBlasDerivatives(const llvm::RecordKeeper &RK, llvm::raw_ostream &os);
bool hasDiffeRet(const llvm::Init *resultTree);
bool hasAdjoint(const llvm::Init *resultTree, llvm::StringRef argName);
