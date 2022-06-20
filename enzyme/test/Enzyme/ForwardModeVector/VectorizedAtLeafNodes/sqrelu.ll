; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --function dsqrelu
; RUN: %opt < %s %loadEnzyme -enzyme -enzyme-preopt=false -enzyme-vectorize-at-leaf-nodes -mem2reg -simplifycfg -adce -S | FileCheck %s

; Function Attrs: nounwind
declare <2 x double> @__enzyme_fwddiff(double (double)*, ...)

; Function Attrs: nounwind readnone uwtable
define dso_local double @sqrelu(double %x) #0 {
entry:
  %cmp = fcmp fast ogt double %x, 0.000000e+00
  br i1 %cmp, label %cond.true, label %cond.end

cond.true:                                        ; preds = %entry
  %0 = tail call fast double @llvm.sin.f64(double %x)
  %mul = fmul fast double %0, %x
  %1 = tail call fast double @llvm.sqrt.f64(double %mul)
  br label %cond.end

cond.end:                                         ; preds = %entry, %cond.true
  %cond = phi double [ %1, %cond.true ], [ 0.000000e+00, %entry ]
  ret double %cond
}

; Function Attrs: nounwind readnone speculatable
declare double @llvm.sin.f64(double) #1

; Function Attrs: nounwind readnone speculatable
declare double @llvm.sqrt.f64(double) #1

; Function Attrs: nounwind uwtable
define dso_local <2 x double> @dsqrelu(double %x) local_unnamed_addr #2 {
entry:
  %0 = tail call <2 x double> (double (double)*, ...) @__enzyme_fwddiff(double (double)* nonnull @sqrelu, metadata !"enzyme_width", i64 2, double %x, <2 x double> <double 1.0, double 1.5>)
  ret <2 x double> %0
}

attributes #0 = { nounwind readnone uwtable }
attributes #1 = { nounwind readnone speculatable }
attributes #2 = { nounwind uwtable }
attributes #3 = { nounwind }


; CHECK: define dso_local <2 x double> @dsqrelu(double %x)
; CHECK-NEXT:  entry:
; CHECK-NEXT:   %cmp.i = fcmp fast ogt double %x, 0.000000e+00
; CHECK-NEXT:   br i1 %cmp.i, label %cond.true.i, label %fwddiffe2sqrelu.exit

; CHECK: cond.true.i:                                      ; preds = %entry
; CHECK-NEXT:   %0 = call fast double @llvm.sin.f64(double %x) #3
; CHECK-NEXT:   %1 = call fast double @llvm.cos.f64(double %x) #4
; CHECK-NEXT:   %.splatinsert.i = insertelement <2 x double> poison, double %1, i32 0
; CHECK-NEXT:   %.splat.i = shufflevector <2 x double> %.splatinsert.i, <2 x double> poison, <2 x i32> zeroinitializer
; CHECK-NEXT:   %2 = fmul fast <2 x double> <double 1.000000e+00, double 1.500000e+00>, %.splat.i
; CHECK-NEXT:   %mul.i = fmul fast double %0, %x
; CHECK-NEXT:   %.splatinsert2.i = insertelement <2 x double> poison, double %0, i32 0
; CHECK-NEXT:   %.splat3.i = shufflevector <2 x double> %.splatinsert2.i, <2 x double> poison, <2 x i32> zeroinitializer
; CHECK-NEXT:   %.splatinsert4.i = insertelement <2 x double> poison, double %x, i32 0
; CHECK-NEXT:   %.splat5.i = shufflevector <2 x double> %.splatinsert4.i, <2 x double> poison, <2 x i32> zeroinitializer
; CHECK-NEXT:   %3 = fmul fast <2 x double> %2, %.splat5.i
; CHECK-NEXT:   %4 = fmul fast <2 x double> <double 1.000000e+00, double 1.500000e+00>, %.splat3.i
; CHECK-NEXT:   %5 = fadd fast <2 x double> %3, %4
; CHECK-NEXT:   %6 = fcmp fast oeq double %mul.i, 0.000000e+00
; CHECK-NEXT:   %7 = extractelement <2 x double> %5, i64 0
; CHECK-NEXT:   %8 = call fast double @llvm.sqrt.f64(double %mul.i) #4
; CHECK-NEXT:   %9 = fmul fast double 5.000000e-01, %7
; CHECK-NEXT:   %10 = fdiv fast double %9, %8
; CHECK-NEXT:   %11 = select fast i1 %6, double 0.000000e+00, double %10
; CHECK-NEXT:   %12 = insertelement <2 x double> undef, double %11, i32 0
; CHECK-NEXT:   %13 = extractelement <2 x double> %5, i64 1
; CHECK-NEXT:   %14 = call fast double @llvm.sqrt.f64(double %mul.i) #4
; CHECK-NEXT:   %15 = fmul fast double 5.000000e-01, %13
; CHECK-NEXT:   %16 = fdiv fast double %15, %14
; CHECK-NEXT:   %17 = select fast i1 %6, double 0.000000e+00, double %16
; CHECK-NEXT:   %18 = insertelement <2 x double> %12, double %17, i32 1
; CHECK-NEXT:   br label %fwddiffe2sqrelu.exit

; CHECK: fwddiffe2sqrelu.exit:                             ; preds = %entry, %cond.true.i
; CHECK-NEXT:   %19 = phi fast <2 x double> [ %18, %cond.true.i ], [ zeroinitializer, %entry ]
; CHECK-NEXT:   ret <2 x double> %19
; CHECK-NEXT: }
