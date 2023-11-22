; RUN: if [ %llvmver -lt 16 ]; then %opt < %s %loadEnzyme -enzyme -enzyme-preopt=false -adce -S | FileCheck %s; fi
; RUN: %opt < %s %newLoadEnzyme -passes="enzyme,function(adce)" -enzyme-preopt=false -S | FileCheck %s

%struct.ompi_predefined_datatype_t = type opaque
%struct.ompi_predefined_communicator_t = type opaque
%struct.ompi_datatype_t = type opaque
%struct.ompi_communicator_t = type opaque

@random_datatype = external dso_local global %struct.ompi_predefined_datatype_t, align 1
@ompi_mpi_comm_world = external dso_local global %struct.ompi_predefined_communicator_t, align 1

define void @mpi_gather_test(double* %b, i8* %recv_buf) {
entry:
  %i8buf = bitcast double* %b to i8*
  call i32 @MPI_Gather(i8* nonnull %i8buf, i32 1, %struct.ompi_datatype_t* bitcast (%struct.ompi_predefined_datatype_t* @random_datatype to %struct.ompi_datatype_t*), i8* %recv_buf, i32 1, %struct.ompi_datatype_t* bitcast (%struct.ompi_predefined_datatype_t* @random_datatype to %struct.ompi_datatype_t*), i32 0, %struct.ompi_communicator_t* bitcast (%struct.ompi_predefined_communicator_t* @ompi_mpi_comm_world to %struct.ompi_communicator_t*))
  ret void
}

declare i32 @MPI_Gather(i8*, i32, %struct.ompi_datatype_t*, i8*, i32, %struct.ompi_datatype_t*, i32, %struct.ompi_communicator_t*) local_unnamed_addr

; Function Attrs: nounwind uwtable
define void @caller(double* %b, double* %db, double* %vla, double* %vla3) local_unnamed_addr  {
entry:
  call void (i8*, ...) @__enzyme_fwddiff(i8* bitcast (void (double*, i8*)* @mpi_gather_test to i8*), metadata !"enzyme_width", i64 2, metadata !"enzyme_dup", double* %b, double* %db, double* %db, metadata !"enzyme_dup", double* %vla, double* %vla3, double* %vla3)
  ret void
}

declare void @__enzyme_fwddiff(i8*, ...)


; CHECK: define internal void @fwddiffe2mpi_gather_test(double* %b, [2 x double*] %"b'", i8* %recv_buf, [2 x i8*] %"recv_buf'") #0 {
; CHECK-NEXT: entry:
; CHECK-NEXT:   %0 = alloca i32, align 4
; CHECK-NEXT:   %1 = alloca i32, align 4
; CHECK-NEXT:   %2 = extractvalue [2 x double*] %"b'", 0
; CHECK-NEXT:   %"i8buf'ipc" = bitcast double* %2 to i8*
; CHECK-NEXT:   %3 = extractvalue [2 x double*] %"b'", 1
; CHECK-NEXT:   %"i8buf'ipc1" = bitcast double* %3 to i8*
; CHECK-NEXT:   %i8buf = bitcast double* %b to i8*
; CHECK-NEXT:   %4 = call i32 @MPI_Gather(i8* nonnull %i8buf, i32 1, %struct.ompi_datatype_t* bitcast (%struct.ompi_predefined_datatype_t* @random_datatype to %struct.ompi_datatype_t*), i8* %recv_buf, i32 1, %struct.ompi_datatype_t* bitcast (%struct.ompi_predefined_datatype_t* @random_datatype to %struct.ompi_datatype_t*), i32 0, %struct.ompi_communicator_t* bitcast (%struct.ompi_predefined_communicator_t* @ompi_mpi_comm_world to %struct.ompi_communicator_t*))
; CHECK-NEXT:   %5 = call i32 @MPI_Comm_rank(%struct.ompi_communicator_t* bitcast (%struct.ompi_predefined_communicator_t* @ompi_mpi_comm_world to %struct.ompi_communicator_t*), i32* %0)
; CHECK-NEXT:   %6 = call i32 @MPI_Type_size(i8* bitcast (%struct.ompi_predefined_datatype_t* @random_datatype to i8*), i32* %1) #2
; CHECK-NEXT:   %7 = extractvalue [2 x i8*] %"recv_buf'", 0
; CHECK-NEXT:   %8 = call i32 @MPI_Gather(i8* %"i8buf'ipc", i32 1, %struct.ompi_datatype_t* bitcast (%struct.ompi_predefined_datatype_t* @random_datatype to %struct.ompi_datatype_t*), i8* %7, i32 1, %struct.ompi_datatype_t* bitcast (%struct.ompi_predefined_datatype_t* @random_datatype to %struct.ompi_datatype_t*), i32 0, %struct.ompi_communicator_t* bitcast (%struct.ompi_predefined_communicator_t* @ompi_mpi_comm_world to %struct.ompi_communicator_t*))
; CHECK-NEXT:   %9 = extractvalue [2 x i8*] %"recv_buf'", 1
; CHECK-NEXT:   %10 = call i32 @MPI_Gather(i8* %"i8buf'ipc1", i32 1, %struct.ompi_datatype_t* bitcast (%struct.ompi_predefined_datatype_t* @random_datatype to %struct.ompi_datatype_t*), i8* %9, i32 1, %struct.ompi_datatype_t* bitcast (%struct.ompi_predefined_datatype_t* @random_datatype to %struct.ompi_datatype_t*), i32 0, %struct.ompi_communicator_t* bitcast (%struct.ompi_predefined_communicator_t* @ompi_mpi_comm_world to %struct.ompi_communicator_t*))
; CHECK-NEXT:   ret void
; CHECK-NEXT: }