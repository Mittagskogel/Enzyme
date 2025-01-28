#include <iostream>

#include <enzyme/fprt/fprt.h>
#include <enzyme/fprt/mpfr.h>

extern "C" {
void enzyme_fprt_op_dump_status() {
  std::cerr << "Information about " << opdata.size()
            << " operations." << std::endl;
  for (const auto& [loc, value] : opdata) {
    std::cout << loc << ": " << value.count << " x " << value.op
              << "   Operation error: "
              << value.mean_op << " avg "
              << value.sqmean_op - value.mean_op*value.mean_op << " var"
              << "   Accumulated error: "
              << value.mean_ac << " avg "
              << value.sqmean_ac - value.mean_ac*value.mean_ac << " var"
              << "   Truncation error: "
              << value.mean_tr << " avg "
              << value.sqmean_tr - value.mean_tr*value.mean_tr << " var"
              << std::endl;
  }
}
}
