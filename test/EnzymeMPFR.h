//===- EnzymeMPFR.h - MPFR wrappers ---------------------------------------===//
//
//                             Enzyme Project
//
// Part of the Enzyme Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// If using this code in an academic setting, please cite the following:
// @incollection{enzymeNeurips,
// title = {Instead of Rewriting Foreign Code for Machine Learning,
//          Automatically Synthesize Fast Gradients},
// author = {Moses, William S. and Churavy, Valentin},
// booktitle = {Advances in Neural Information Processing Systems 33},
// year = {2020},
// note = {To appear in},
// }
//
//===----------------------------------------------------------------------===//
//
// This file contains easy to use wrappers around MPFR functions.
//
//===----------------------------------------------------------------------===//
#ifndef __ENZYME_RUNTIME_ENZYME_MPFR__
#define __ENZYME_RUNTIME_ENZYME_MPFR__

#include <mpfr.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

    // TODO s
    //
    // (for MPFR ver. 2.1)
    //
    // We need to set the range of the allowed exponent using `mpfr_set_emin` and
    // `mpfr_set_emax`. (This means we can also play with whether the range is
    // centered around 0 (1?) or somewhere else)
    //
    // (also these need to be mutex'ed as the exponent change is global in mpfr and
    // not float-specific) ... (mpfr seems to have thread safe mode - check if it is
    // enabled or if it is enabled by default)
    //
    // For that we need to do this check:
    //   If the user changes the exponent range, it is her/his responsibility to
    //   check that all current floating-point variables are in the new allowed
    //   range (for example using mpfr_check_range), otherwise the subsequent
    //   behavior will be undefined, in the sense of the ISO C standard.
    //
    // MPFR docs state the following:
    //   Note: Overflow handling is still experimental and currently implemented
    //   partially. If an overflow occurs internally at the wrong place, anything
    //   can happen (crash, wrong results, etc).
    //
    // Which we would like to avoid somehow.
    //
    // MPFR also has this limitation that we need to address for accurate
    // simulation:
    //   [...] subnormal numbers are not implemented.
    //
    // TODO maybe take debug info as parameter - then we can emit warnings or tie
    // operations to source location

#define __ENZYME_MPFR_ATTRIBUTES __attribute__((weak))
#define __ENZYME_MPFR_DEFAULT_ROUNDING_MODE GMP_RNDN

    double total_err = 0.0;
    
    bool __enzyme_fprt_is_mem_mode(int64_t mode) { return mode & 0b0001; }
    bool __enzyme_fprt_is_op_mode(int64_t mode) { return mode & 0b0010; }

    typedef struct {
	mpfr_t fp;
    } __enzyme_fp;

    double __enzyme_fprt_ptr_to_double(__enzyme_fp *p) { return *((double *)(&p)); }
    __enzyme_fp *__enzyme_fprt_double_to_ptr(double d) {
	return *((__enzyme_fp **)(&d));
    }

    __ENZYME_MPFR_ATTRIBUTES
    double __enzyme_fprt_64_52_get(double _a, int64_t exponent, int64_t significand,
				   int64_t mode) {
	__enzyme_fp *a = __enzyme_fprt_double_to_ptr(_a);
	return mpfr_get_d(a->fp, __ENZYME_MPFR_DEFAULT_ROUNDING_MODE);
    }

    __ENZYME_MPFR_ATTRIBUTES
    double __enzyme_fprt_64_52_new(double _a, int64_t exponent, int64_t significand,
				   int64_t mode) {
	__enzyme_fp *a = (__enzyme_fp *)malloc(sizeof(__enzyme_fp));
	mpfr_init2(a->fp, significand);
	mpfr_set_d(a->fp, _a, __ENZYME_MPFR_DEFAULT_ROUNDING_MODE);
	return __enzyme_fprt_ptr_to_double(a);
    }

    __ENZYME_MPFR_ATTRIBUTES
    __enzyme_fp *__enzyme_fprt_64_52_new_intermediate(int64_t exponent,
						      int64_t significand,
						      int64_t mode) {
	__enzyme_fp *a = (__enzyme_fp *)malloc(sizeof(__enzyme_fp));
	mpfr_init2(a->fp, significand);
	return a;
    }

    __ENZYME_MPFR_ATTRIBUTES
    void __enzyme_fprt_64_52_delete(double a, int64_t exponent, int64_t significand,
				    int64_t mode) {
	free(__enzyme_fprt_double_to_ptr(a));
    }

#define __ENZYME_MPFR_SINGOP(OP_TYPE, LLVM_OP_NAME, MPFR_FUNC_NAME, FROM_TYPE, \
                             RET, MPFR_GET, ARG1, MPFR_SET_ARG1,	\
                             ROUNDING_MODE)				\
    __ENZYME_MPFR_ATTRIBUTES						\
    RET __enzyme_fprt_##FROM_TYPE##_##OP_TYPE##_##LLVM_OP_NAME(		\
							       ARG1 a, int64_t exponent, int64_t significand, int64_t mode) { \
	if (__enzyme_fprt_is_op_mode(mode)) {				\
	    mpfr_t ma, mc;						\
	    mpfr_init2(ma, significand);				\
	    mpfr_init2(mc, significand);				\
	    mpfr_set_##MPFR_SET_ARG1(ma, a, ROUNDING_MODE);		\
	    mpfr_##MPFR_FUNC_NAME(mc, ma, ROUNDING_MODE);		\
	    RET c = mpfr_get_##MPFR_GET(mc, ROUNDING_MODE);		\
	    mpfr_t pa, pc, err, mtot_err;				\
	    mpfr_init2(pa, 53);						\
	    mpfr_init2(pc, 53);						\
	    mpfr_init2(err, 53);					\
	    mpfr_init2(mtot_err, 53);					\
	    mpfr_set_##MPFR_SET_ARG1(pa, a, ROUNDING_MODE);		\
	    mpfr_##MPFR_FUNC_NAME(pc, pa, ROUNDING_MODE);		\
	    mpfr_sub(err, mc, pc, ROUNDING_MODE);			\
	    mpfr_abs(pa, err, ROUNDING_MODE);				\
	    mpfr_set_d(err, total_err, ROUNDING_MODE);			\
	    mpfr_add(mtot_err, pa, err, ROUNDING_MODE);			\
	    total_err = mpfr_get_##MPFR_GET(mtot_err, ROUNDING_MODE);	\
	    RET fp_err = mpfr_get_##MPFR_GET(err, ROUNDING_MODE);	\
	    fprintf(stderr, "%s, %f\n", #MPFR_FUNC_NAME, fp_err);	\
	    mpfr_init2(mtot_err, 53);					\
	    mpfr_clear(ma);						\
	    mpfr_clear(mc);						\
	    mpfr_clear(pa);						\
	    mpfr_clear(pc);						\
	    mpfr_clear(err);						\
	    mpfr_clear(mtot_err);					\
	    return c;							\
	} else if (__enzyme_fprt_is_mem_mode(mode)) {			\
	    __enzyme_fp *ma = __enzyme_fprt_double_to_ptr(a);		\
	    __enzyme_fp *mc =						\
		__enzyme_fprt_64_52_new_intermediate(exponent, significand, mode); \
	    mpfr_##MPFR_FUNC_NAME(mc->fp, ma->fp, ROUNDING_MODE);	\
	    return __enzyme_fprt_ptr_to_double(mc);			\
	} else {							\
	    abort();							\
	}								\
    }

    // TODO this is a bit sketchy if the user cast their float to int before calling
    // this. We need to detect these patterns
#define __ENZYME_MPFR_BIN_INT(OP_TYPE, LLVM_OP_NAME, MPFR_FUNC_NAME,	\
                              FROM_TYPE, RET, MPFR_GET, ARG1, MPFR_SET_ARG1, \
                              ARG2, ROUNDING_MODE)			\
    __ENZYME_MPFR_ATTRIBUTES						\
    RET __enzyme_fprt_##FROM_TYPE##_##OP_TYPE##_##LLVM_OP_NAME(		\
							       ARG1 a, ARG2 b, int64_t exponent, int64_t significand, int64_t mode) { \
	if (__enzyme_fprt_is_op_mode(mode)) {				\
	    mpfr_t ma, mc;						\
	    mpfr_init2(ma, significand);				\
	    mpfr_init2(mc, significand);				\
	    mpfr_set_##MPFR_SET_ARG1(ma, a, ROUNDING_MODE);		\
	    mpfr_##MPFR_FUNC_NAME(mc, ma, b, ROUNDING_MODE);		\
	    RET c = mpfr_get_##MPFR_GET(mc, ROUNDING_MODE);		\
	    mpfr_clear(ma);						\
	    mpfr_clear(mc);						\
	    return c;							\
	} else if (__enzyme_fprt_is_mem_mode(mode)) {			\
	    __enzyme_fp *ma = __enzyme_fprt_double_to_ptr(a);		\
	    __enzyme_fp *mc =						\
		__enzyme_fprt_64_52_new_intermediate(exponent, significand, mode); \
	    mpfr_##MPFR_FUNC_NAME(mc->fp, ma->fp, b, ROUNDING_MODE);	\
	    return __enzyme_fprt_ptr_to_double(mc);			\
	} else {							\
	    abort();							\
	}								\
    }

#define __ENZYME_MPFR_BIN(OP_TYPE, LLVM_OP_NAME, MPFR_FUNC_NAME, FROM_TYPE, \
                          RET, MPFR_GET, ARG1, MPFR_SET_ARG1, ARG2,	\
                          MPFR_SET_ARG2, ROUNDING_MODE)			\
    __ENZYME_MPFR_ATTRIBUTES						\
    RET __enzyme_fprt_##FROM_TYPE##_##OP_TYPE##_##LLVM_OP_NAME(		\
							       ARG1 a, ARG2 b, int64_t exponent, int64_t significand, int64_t mode) { \
	if (__enzyme_fprt_is_op_mode(mode)) {				\
	    mpfr_t ma, mb, mc;						\
	    mpfr_init2(ma, significand);				\
	    mpfr_init2(mb, significand);				\
	    mpfr_init2(mc, significand);				\
	    mpfr_set_##MPFR_SET_ARG1(ma, a, ROUNDING_MODE);		\
	    mpfr_set_##MPFR_SET_ARG2(mb, b, ROUNDING_MODE);		\
	    mpfr_##MPFR_FUNC_NAME(mc, ma, mb, ROUNDING_MODE);		\
	    RET c = mpfr_get_##MPFR_GET(mc, ROUNDING_MODE);		\
	    mpfr_t pa, pb, pc, err, mtot_err;				\
	    mpfr_init2(pa, 53);						\
	    mpfr_init2(pb, 53);						\
	    mpfr_init2(pc, 53);						\
	    mpfr_init2(err, 53);					\
	    mpfr_init2(mtot_err, 53);					\
	    mpfr_set_##MPFR_SET_ARG1(pa, a, ROUNDING_MODE);		\
	    mpfr_set_##MPFR_SET_ARG2(pb, b, ROUNDING_MODE);		\
	    mpfr_##MPFR_FUNC_NAME(pc, pa, pb, ROUNDING_MODE);		\
	    mpfr_sub(err, mc, pc, ROUNDING_MODE);			\
	    mpfr_abs(pa, err, ROUNDING_MODE);				\
	    mpfr_set_d(err, total_err, ROUNDING_MODE);			\
	    mpfr_add(mtot_err, pa, err, ROUNDING_MODE);			\
	    total_err = mpfr_get_##MPFR_GET(mtot_err, ROUNDING_MODE);	\
	    RET fp_err = mpfr_get_##MPFR_GET(err, ROUNDING_MODE);	\
	    fprintf(stderr, "%s, %f\n", #MPFR_FUNC_NAME, fp_err);	\
	    mpfr_clear(ma);						\
	    mpfr_clear(mb);						\
	    mpfr_clear(mc);						\
	    mpfr_clear(pa);						\
	    mpfr_clear(pb);						\
	    mpfr_clear(pc);						\
	    mpfr_clear(err);						\
	    mpfr_clear(mtot_err);					\
	    return c;							\
	} else if (__enzyme_fprt_is_mem_mode(mode)) {			\
	    __enzyme_fp *ma = __enzyme_fprt_double_to_ptr(a);		\
	    __enzyme_fp *mb = __enzyme_fprt_double_to_ptr(b);		\
	    __enzyme_fp *mc =						\
		__enzyme_fprt_64_52_new_intermediate(exponent, significand, mode); \
	    mpfr_##MPFR_FUNC_NAME(mc->fp, ma->fp, mb->fp, ROUNDING_MODE); \
	    return __enzyme_fprt_ptr_to_double(mc);			\
	} else {							\
	    abort();							\
	}								\
    }

#define __ENZYME_MPFR_FMULADD(LLVM_OP_NAME, FROM_TYPE, TYPE, MPFR_TYPE,	\
                              LLVM_TYPE, ROUNDING_MODE)			\
    __ENZYME_MPFR_ATTRIBUTES						\
    TYPE __enzyme_fprt_##FROM_TYPE##_intr_##LLVM_OP_NAME##_##LLVM_TYPE(	\
								       TYPE a, TYPE b, TYPE c, int64_t exponent, int64_t significand, \
								       int64_t mode) { \
	if (__enzyme_fprt_is_op_mode(mode)) {				\
	    mpfr_t ma, mb, mc, mmul, madd;				\
	    mpfr_init2(ma, significand);				\
	    mpfr_init2(mb, significand);				\
	    mpfr_init2(mc, significand);				\
	    mpfr_init2(mmul, significand);				\
	    mpfr_init2(madd, significand);				\
	    mpfr_set_##MPFR_TYPE(ma, a, ROUNDING_MODE);			\
	    mpfr_set_##MPFR_TYPE(mb, b, ROUNDING_MODE);			\
	    mpfr_set_##MPFR_TYPE(mc, c, ROUNDING_MODE);			\
	    mpfr_mul(mmul, ma, mb, ROUNDING_MODE);			\
	    mpfr_add(madd, mmul, mc, ROUNDING_MODE);			\
	    TYPE res = mpfr_get_##MPFR_TYPE(madd, ROUNDING_MODE);	\
	    mpfr_clear(ma);						\
	    mpfr_clear(mb);						\
	    mpfr_clear(mc);						\
	    mpfr_clear(mmul);						\
	    mpfr_clear(madd);						\
	    return res;							\
	} else if (__enzyme_fprt_is_mem_mode(mode)) {			\
	    __enzyme_fp *ma = __enzyme_fprt_double_to_ptr(a);		\
	    __enzyme_fp *mb = __enzyme_fprt_double_to_ptr(b);		\
	    __enzyme_fp *mc = __enzyme_fprt_double_to_ptr(c);		\
	    double mmul = __enzyme_fprt_##FROM_TYPE##_binop_fmul(	\
								 __enzyme_fprt_ptr_to_double(ma), __enzyme_fprt_ptr_to_double(mb), \
								 exponent, significand, mode); \
	    double madd = __enzyme_fprt_##FROM_TYPE##_binop_fadd(	\
								 mmul, __enzyme_fprt_ptr_to_double(mc), exponent, significand, mode); \
	    return madd;						\
	} else {							\
	    abort();							\
	}								\
    }

#define __ENZYME_MPFR_DOUBLE_BINOP(LLVM_OP_NAME, MPFR_FUNC_NAME,	\
                                   ROUNDING_MODE)			\
    __ENZYME_MPFR_BIN(binop, LLVM_OP_NAME, MPFR_FUNC_NAME, 64_52, double, d, \
		      double, d, double, d, ROUNDING_MODE)
#define __ENZYME_MPFR_DOUBLE_BINFUNCINTR(LLVM_OP_NAME, MPFR_FUNC_NAME,	\
                                         ROUNDING_MODE)			\
    __ENZYME_MPFR_BIN(intr, LLVM_OP_NAME, MPFR_FUNC_NAME, 64_52, double, d, \
		      double, d, double, d, ROUNDING_MODE)		\
	__ENZYME_MPFR_BIN(intr, llvm_##LLVM_OP_NAME##_f64, MPFR_FUNC_NAME, 64_52, \
			  double, d, double, d, double, d, ROUNDING_MODE) \
	__ENZYME_MPFR_BIN(func, LLVM_OP_NAME, MPFR_FUNC_NAME, 64_52, double, d,	\
			  double, d, double, d, ROUNDING_MODE)
#define __ENZYME_MPFR_DOUBLE_BINOP_DEFAULT_ROUNDING(LLVM_OP_NAME,	\
                                                    MPFR_FUNC_NAME)	\
    __ENZYME_MPFR_DOUBLE_BINOP(LLVM_OP_NAME, MPFR_FUNC_NAME,		\
			       __ENZYME_MPFR_DEFAULT_ROUNDING_MODE)
#define __ENZYME_MPFR_DOUBLE_BINFUNCINTR_DEFAULT_ROUNDING(LLVM_OP_NAME,	\
                                                          MPFR_FUNC_NAME) \
    __ENZYME_MPFR_DOUBLE_BINFUNCINTR(LLVM_OP_NAME, MPFR_FUNC_NAME,	\
				     __ENZYME_MPFR_DEFAULT_ROUNDING_MODE)

#define __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(LLVM_NAME, MPFR_NAME)		\
    __ENZYME_MPFR_SINGOP(intr, LLVM_NAME, MPFR_NAME, 64_52, double, d, double, \
			 d, __ENZYME_MPFR_DEFAULT_ROUNDING_MODE)	\
	__ENZYME_MPFR_SINGOP(intr, llvm_##LLVM_NAME##_f64, MPFR_NAME, 64_52, double, \
			     d, double, d, __ENZYME_MPFR_DEFAULT_ROUNDING_MODE)	\
	__ENZYME_MPFR_SINGOP(func, LLVM_NAME, MPFR_NAME, 64_52, double, d, double, \
			     d, __ENZYME_MPFR_DEFAULT_ROUNDING_MODE)	\
	__ENZYME_MPFR_SINGOP(TYPE, LLVM_NAME, MPFR_NAME, 32_23, float, d, float, d, \
			     __ENZYME_MPFR_DEFAULT_ROUNDING_MODE)	\
	__ENZYME_MPFR_SINGOP(intr, LLVM_NAME, MPFR_NAME, 32_23, float, d, float, d, \
			     __ENZYME_MPFR_DEFAULT_ROUNDING_MODE)	\
	__ENZYME_MPFR_SINGOP(intr, llvm_##LLVM_NAME##_f32, MPFR_NAME, 32_23, float, \
			     d, float, d, __ENZYME_MPFR_DEFAULT_ROUNDING_MODE) \
	__ENZYME_MPFR_SINGOP(func, LLVM_NAME, MPFR_NAME, 32_23, float, d, float, d, \
			     __ENZYME_MPFR_DEFAULT_ROUNDING_MODE)

    // clang-format off

    // Binary operations
	__ENZYME_MPFR_DOUBLE_BINOP_DEFAULT_ROUNDING(fmul, mul);
    __ENZYME_MPFR_DOUBLE_BINOP_DEFAULT_ROUNDING(fadd, add);
    __ENZYME_MPFR_DOUBLE_BINOP_DEFAULT_ROUNDING(fsub, sub);
    __ENZYME_MPFR_DOUBLE_BINOP_DEFAULT_ROUNDING(fdiv, div);
    __ENZYME_MPFR_DOUBLE_BINOP_DEFAULT_ROUNDING(frem, remainder);

    /* __ENZYME_MPFR_DOUBLE_BINOP_DEFAULT_ROUNDING(fsqrt, sqrt); */

    __ENZYME_MPFR_DOUBLE_BINFUNCINTR_DEFAULT_ROUNDING(pow, pow);
    __ENZYME_MPFR_DOUBLE_BINFUNCINTR_DEFAULT_ROUNDING(copysign, copysign);
    __ENZYME_MPFR_DOUBLE_BINFUNCINTR_DEFAULT_ROUNDING(fdim, dim);
    __ENZYME_MPFR_DOUBLE_BINFUNCINTR_DEFAULT_ROUNDING(remainder, remainder);
    __ENZYME_MPFR_DOUBLE_BINFUNCINTR_DEFAULT_ROUNDING(atan2, atan2);
    __ENZYME_MPFR_DOUBLE_BINFUNCINTR_DEFAULT_ROUNDING(hypot, hypot);
    __ENZYME_MPFR_DOUBLE_BINFUNCINTR_DEFAULT_ROUNDING(fmod, fmod);
    __ENZYME_MPFR_DOUBLE_BINFUNCINTR_DEFAULT_ROUNDING(maxnum, max);
    __ENZYME_MPFR_DOUBLE_BINFUNCINTR_DEFAULT_ROUNDING(minnum, min);

    __ENZYME_MPFR_BIN_INT(intr, llvm_powi_f64_i32, pow_si, 64_52, double, d, double, d, int32_t, __ENZYME_MPFR_DEFAULT_ROUNDING_MODE);

    // Unary operations
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(sqrt, sqrt);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(__sqrt_finite, sqrt);

    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(atanh, atanh);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(acosh, acosh);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(asinh, asinh);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(atan, atan);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(acos, acos);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(asin, asin);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(tanh, tanh);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(cosh, cosh);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(sinh, sinh);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(tan, tan);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(cos, cos);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(sin, sin);

    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(exp, exp);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(exp2, exp2);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(expm1, expm1);

    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(log, log);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(log2, log2);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(log10, log10);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(log1p, log1p);

    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(fabs, abs);

    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(trunc, abs);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(round, abs);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(floor, abs);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(ceil, abs);

    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(erf, erf);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(erfc, erfc);

    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(cbrt, cbrt);

    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(tgamma, gamma);
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(lgamma, lngamma);

    // TODO This is not accurate (I think we cast int to double)
    __ENZYME_MPFR_SINGOP_DOUBLE_FLOAT(nearbyint, rint);

    // Ternary operation
    __ENZYME_MPFR_FMULADD(llvm_fmuladd, 64_52, double, d, f64, __ENZYME_MPFR_DEFAULT_ROUNDING_MODE);
    __ENZYME_MPFR_FMULADD(llvm_fma, 64_52, double, d, f64, __ENZYME_MPFR_DEFAULT_ROUNDING_MODE);

    // clang-format on

#ifdef __cplusplus
}
#endif

#endif // #ifndef __ENZYME_RUNTIME_ENZYME_MPFR__