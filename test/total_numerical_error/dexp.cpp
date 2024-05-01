#include "stdio.h"
#include "math.h"
#include "stdbool.h"
#include "stdlib.h"
#include <iostream>

#include "enzyme/fprt/fprt.h"

#define FROM 64
#define TO 32

template <typename fty> fty *__enzyme_truncate_mem_func(fty *, int, int);
template <typename fty> fty *__enzyme_truncate_op_func(fty *, int, int, int);

void dexp() {
    double h = 0.1;
    double x = 0;
    double df = 0;
    
    for (int i = 1; i <= 16; ++i) {
        df = (exp(x + h) - exp(x)) / h;

	//printf("%d %.20f\n", -i, __enzyme_expand_mem_value_d(fabs(df - 1.0), FROM, TO));
	printf("%d %.20f\n", -i, fabs(df - 1.0));
	
	h /= 10;
    }
}

int main() {
    printf("log_10(h) rel_err\n");
    
    __enzyme_truncate_op_func(dexp, FROM, 0, 16)();
    
    return 0;
}
