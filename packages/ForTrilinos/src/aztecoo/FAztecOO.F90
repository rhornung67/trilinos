!*********************************************************************
! ForTrilinos: Object-Oriented Fortran 2003 interface to Trilinos
!                Copyright 2010 Sandia Corporation
!
! Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
! the U.S. Government retains certain rights in this software.
!
! Redistribution and use in source and binary forms, with or without
! modification, are permitted provided that the following conditions are met:
!
! 1. Redistributions of source code must retain the above copyright
!    notice, this list of conditions and the following disclaimer.
!
! 2. Redistributions in binary form must reproduce the above copyright
!    notice, this list of conditions and the following disclaimer in the
!    documentation and/or other materials provided with the distribution.
!
! 3. Neither the name of the Corporation nor the names of the
!    contributors may be used to endorse or promote products derived from
!    this software without specific prior written permission.
!
! THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
! EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
! IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
! PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
! CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
! EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
! PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
! PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
! LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
! NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
! SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
!
! Questions? Contact Karla Morris  (knmorri@sandia.gov) or 
!                    Damian Rouson (rouson@sandia.gov)
!*********************************************************************


module FAztecOO
  use ForTrilinos_enums ,only: FT_AztecOO_ID_t,ForTrilinos_Universal_ID_t
  use ForTrilinos_table_man
  use ForTrilinos_universal,only:universal
  use ForTrilinos_error
  use FEpetra_MultiVector, only: Epetra_MultiVector
  use FEpetra_RowMatrix, only: Epetra_RowMatrix
  use iso_c_binding     ,only: c_int,c_double,c_char
  use foraztecoo
  implicit none
  private                      ! Hide everything by default
  public :: AztecOO ! Expose type/constructors/methods

  type ,extends(universal)                    :: AztecOO !"shell"
    private
    type(FT_AztecOO_ID_t) :: AztecOO_id 
  contains
     ! Developers only
     procedure         :: invalidate_id => invalidate_AztecOO_ID
     procedure         :: ctrilinos_delete => ctrilinos_delete_AztecOO
     procedure         :: get_AztecOO_ID 
     procedure ,nopass :: alias_AztecOO_ID
     procedure         :: generalize 
     ! Standard AztecOO solve methods
     procedure         :: iterate_current
     procedure         :: iterate_RowMatrix
     generic :: iterate => iterate_current, iterate_RowMatrix
  end type

   interface AztecOO ! constructors
     module procedure duplicate,from_struct,from_RowMatrix
   end interface

contains
  type(AztecOO) function from_struct(id)
     type(FT_AztecOO_ID_t) ,intent(in) :: id
     from_struct%AztecOO_id = id
     call from_struct%register_self
   print *,'Inside AztecOO constructor'
  end function
  
  !> <BR> Original C++ prototype:
  !! AztecOO(Epetra_RowMatrix * A, Epetra_MultiVector * X, Epetra_MultiVector * B);
  !> <BR> <BR> CTrilinos prototype:
  !! CT_AztecOO_ID_t AztecOO_Create_FromRowMatrix ( CT_Epetra_RowMatrix_ID_t AID, CT_Epetra_MultiVector_ID_t XID, CT_Epetra_MultiVector_ID_t BID );
  !> <BR> <BR> ForTrilinos prototype:
  !! AztecOO (Epetra_RowMatrix A, Epetra_MultiVector x, Epetra_MultiVector b);

  type(AztecOO) function from_RowMatrix(A,x,b)
   class(Epetra_RowMatrix) ,intent(in) :: A 
   class(Epetra_MultiVector) ,intent(in) :: x,b 
   type(FT_AztecOO_ID_t) :: from_RowMatrix_id
   from_RowMatrix_id = AztecOO_Create_FromRowMatrix(A%get_EpetraRowMatrix_ID(),x%get_EpetraMultiVector_ID(),b%get_EpetraMultiVector_ID())
   from_RowMatrix = from_struct(from_RowMatrix_id)
  end function

  type(AztecOO) function duplicate(this)
    type(AztecOO) ,intent(in) :: this
    type(FT_AztecOO_ID_t) :: duplicate_id
    duplicate_id = AztecOO_Duplicate(this%AztecOO_id)
    duplicate = from_struct(duplicate_id)
  end function

  type(FT_AztecOO_ID_t) function get_AztecOO_ID(this)
    class(AztecOO) ,intent(in) :: this 
    get_AztecOO_ID=this%AztecOO_id
  end function
  
  type(FT_AztecOO_ID_t) function alias_AztecOO_ID(generic_id)
    use ForTrilinos_table_man,only: CT_Alias
    use iso_c_binding        ,only: c_loc,c_int
    use ForTrilinos_enums    ,only: ForTrilinos_Universal_ID_t,FT_AztecOO_ID
    type(ForTrilinos_Universal_ID_t) ,intent(in) :: generic_id
    type(ForTrilinos_Universal_ID_t) ,pointer    :: alias_id
    integer(c_int) :: status
    type(error) :: ierr
    if (.not.associated(alias_id)) then
      allocate(alias_id,source=CT_Alias(generic_id,FT_AztecOO_ID),stat=status)
      ierr=error(status,'FAztecOO:alias_AztecOO_ID')
      call ierr%check_success()
    endif
    alias_AztecOO_ID=degeneralize_AztecOO(c_loc(alias_id))
    call deallocate_and_check_error(alias_id,'FAztecOO:alias_AztecOO_ID')
  end function

  type(ForTrilinos_Universal_ID_t) function generalize(this)
   ! ____ Use for ForTrilinos function implementation ______
   use ForTrilinos_utils ,only: generalize_all
   use iso_c_binding     ,only : c_loc
   class(AztecOO) ,intent(in) ,target :: this
   generalize = generalize_all(c_loc(this%AztecOO_id))
   ! ____ Use for ForTrilinos function implementation ______

   ! ____ Use for CTrilinos function implementation ______
   ! class(AztecOO) ,intent(in) ,target :: this
   ! generalize = AztecOO_Generalize ( this%AztecOO_id)
   ! ____ Use for CTrilinos function implementation ______
  end function

 type(FT_AztecOO_ID_t) function degeneralize_AztecOO(generic_id) bind(C)
  ! ____ Use for ForTrilinos function implementation ______
    use ForTrilinos_enums ,only : ForTrilinos_Universal_ID_t,FT_AztecOO_ID_t
    use ,intrinsic :: iso_c_binding ,only: c_ptr,c_f_pointer
    type(c_ptr)                      ,value   :: generic_id
    type(FT_AztecOO_ID_t) ,pointer :: local_ptr
    call c_f_pointer (generic_id, local_ptr)
    degeneralize_AztecOO = local_ptr
  ! ____ Use for ForTrilinos function implementation ______

  ! ____ Use for CTrilinos function implementation ______
  !type(ForTrilinos_Universal_ID_t) ,intent(in) :: generic_id
  !degeneralize_AztecOO = AztecOO_Degeneralize(generic_id)
  ! ____ Use for CTrilinos function implementation ______
  end function

  !> <BR> Original C++ prototype:
  !! int Iterate(int MaxIters, double Tolerance);
  !> <BR> <BR> CTrilinos prototype:
  !! int AztecOO_Iterate_Current ( CT_AztecOO_ID_t selfID, int MaxIters, double Tolerance );
  !> <BR> <BR> <BR> ForTrilinos prototype:
  !! iterate ( AztecOO this, int MaxIters, double Tolerance ,ForTrilinos_error err);

  subroutine iterate_current(this,MaxIters,tolerance,err) 
    class(AztecOO)   ,intent(in) :: this
    integer(c_int)   ,intent(in) :: MaxIters
    real(c_double)   ,intent(in) :: tolerance
    type(error) ,optional    ,intent(out) :: err
    integer(c_int)               :: error_out
    error_out = AztecOO_Iterate_Current(this%AztecOO_id,MaxIters,tolerance)
    if (present(err)) err=error(error_out)
  end subroutine

  !> <BR> Original C++ prototype:
  !! int Iterate(Epetra_RowMatrix * A, Epetra_MultiVector * X, Epetra_MultiVector * B, int MaxIters, double Tolerance);
  !> <BR> <BR> CTrilinos prototype:
  !! int AztecOO_Iterate ( CT_AztecOO_ID_t selfID, CT_Epetra_RowMatrix_ID_t AID, CT_Epetra_MultiVector_ID_t XID, CT_Epetra_MultiVector_ID_t BID, int  MaxIters, double Tolerance );
  !> <BR> <BR> <BR> ForTrilinos prototype:
  !!  iterate ( AztecOO this, Epetra_RowMatrix A, Epetra_MultiVector x, Epetra_MultiVector b, int MaxIters, double Tolerance, ForTrilinos_error err );

  subroutine iterate_RowMatrix(this,A,x,b,MaxIters,tolerance,err) 
    class(AztecOO)   ,intent(in) :: this
    class(Epetra_RowMatrix) ,intent(in) :: A
    class(Epetra_MultiVector) ,intent(in) :: x,b
    integer(c_int)   ,intent(in) :: MaxIters
    real(c_double)   ,intent(in) :: tolerance
    type(error) ,optional    ,intent(out) :: err
    integer(c_int)               :: error_out
    error_out = AztecOO_Iterate(this%AztecOO_id,A%get_EpetraRowMatrix_ID(),x%get_EpetraMultiVector_ID(),b%get_EpetraMultiVector_ID(),MaxIters,tolerance)
    if (present(err)) err=error(error_out)
  end subroutine

  subroutine invalidate_AztecOO_ID(this)
    class(AztecOO),intent(inout) :: this
    this%AztecOO_id%table = FT_Invalid_ID
    this%AztecOO_id%index = FT_Invalid_Index 
    this%AztecOO_id%is_const = FT_FALSE
  end subroutine

  subroutine ctrilinos_delete_AztecOO(this)
    class(AztecOO),intent(inout) :: this
    call AztecOO_Destroy( this%AztecOO_id ) 
  end subroutine

end module 
