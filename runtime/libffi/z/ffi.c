/* -----------------------------------------------------------------------
   ffi.c - Copyright (c) 2000, 2007 Software AG
           Copyright (c) 2008 Red Hat, Inc
           Copyright (c) 2016, 2018 IBM Corp.
 
   S390 Foreign Function Interface
 
   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:
 
   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.
 
   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */
/*====================================================================*/
/*                          Includes                                  */
/*                          --------                                  */
/*====================================================================*/
 
#include <ffi.h>
#include <ffi_common.h>
 
#include <stdlib.h>
#include <stdio.h>
 
/*====================== End of Includes =============================*/
 
/*====================================================================*/
/*                           Defines                                  */
/*                           -------                                  */
/*====================================================================*/

/* Maximum number of GPRs available for argument passing.  */ 
#define MAX_GPRARGS 3

/* Maximum number of FPRs available for argument passing.  */ 
#define MAX_FPRARGS 4

/* Round to multiple of 16.  */
#define ROUND_SIZE(size) (((size) + 15) & ~15)

/* If these values change, sysv.S must be adapted!  */
#define FFI390_RET_VOID		0
#define FFI390_RET_STRUCT	1
#define FFI390_RET_FLOAT	2
#define FFI390_RET_DOUBLE	3
#define FFI390_RET_LDBLE        4
#define FFI390_RET_INT32	5
#define FFI390_RET_INT64        6
/*===================== End of Defines ===============================*/
 
/*====================================================================*/
/*                          Prototypes                                */
/*                          ----------                                */
/*====================================================================*/
 
/*Making it extern to call this from sysvz.S*/
#pragma map(ffi_prep_args, "PREPARGS")
void ffi_prep_args (unsigned char *, extended_cif *);
void ffi_closure_helper_SYSV (ffi_closure *, unsigned long *, 
			 unsigned long long *, unsigned long *);

/*====================== End of Prototypes ===========================*/
 
/*====================================================================*/
/*                          Externals                                 */
/*                          ---------                                 */
/*====================================================================*/
 

#pragma map(ffi_call_SYSV, "FFISYS")
extern void ffi_call_SYSV(void (*fn)(void), extended_cif *, 
			  unsigned, unsigned *, unsigned, unsigned, unsigned);

extern void ffi_closure_SYSV(void);
 
/*====================== End of Externals ============================*/
 
/*====================================================================*/
/*                                                                    */
/* Name     - ffi_check_struct_type.                                  */
/*                                                                    */
/* Function - Determine if a structure can be passed within a         */
/*            general purpose or floating point register.             */
/*                                                                    */
/*====================================================================*/
 
static int
ffi_check_struct_type (ffi_type *arg)
{
  size_t size = arg->size;

  /* If the struct has just one element, look at that element
     to find out whether to consider the struct as floating point.  */
  while (arg->type == FFI_TYPE_STRUCT 
         && arg->elements[0] && !arg->elements[1])
    arg = arg->elements[0];

  /* Structs of size 1, 2, 4, and 8 are passed in registers,
     just like the corresponding int/float types.  */
  switch (size)
  {
    case 1:
    case 2:
      return FFI_TYPE_UINT32; 
    case 4:
      if (arg->type == FFI_TYPE_FLOAT)
       	return FFI_TYPE_FLOAT;
      else
       	return FFI_TYPE_UINT32; 
    case 8:
      if (arg->type == FFI_TYPE_DOUBLE)
       	return FFI_TYPE_DOUBLE;
      else
       	return FFI_TYPE_UINT64; 
    default:
      break;
  }

  /* Other structs are passed via a pointer to the data.  */
  return FFI_TYPE_POINTER;
}
 
/*======================== End of Routine ============================*/
 
/*====================================================================*/
/*                                                                    */
/* Name     - ffi_prep_args.                                          */
/*                                                                    */
/* Function - Prepare parameters for call to function.                */
/*                                                                    */
/* ffi_prep_args is called by the assembly routine once stack space   */
/* has been allocated for the function's arguments.                   */
/*                                                                    */
/*====================================================================*/
 
void
ffi_prep_args (unsigned char *stack, extended_cif *ecif)
{
  /* The stack space will be filled with these areas:
     Note: XPLINK Stack is downward growing

     ------------------------------------ <- Low Addresses
       Guard Page (4KB)
     ------------------------------------ 
       Stack Frame for Called functions
     ------------------------------------ <- Stack Ptr (r4)
       Backchain                            |+2048
     ------------------------------------   |
       Environment                          |
     ------------------------------------   |
       Entry Point                          |  Savearea 
     ------------------------------------   |  48 bytes
       Return Address                       |
     ------------------------------------   |
       R8 - R15                             |
     ------------------------------------ <-
       Reserved (8 bytes)                   +2096
     ------------------------------------ 
       Debug Area (4 bytes)                 +2104
     ------------------------------------ 
       Arg area prefix (4 bytes)            +2108
     ------------------------------------ 
       Argument area: Parm1 ... ParmN       +2112
     ------------------------------------ 
       Local (automatic storage)
      Saved FPRs   Saved  ARs   Saved VRs 
     ------------------------------------ <- High Addresses
  */

  int i;
  ffi_type **type_ptr;
  void **p_argv = ecif->avalue;
  unsigned char* arg_ptr = stack;
  
#ifdef FFI_DEBUG
  printf("prep_args: stack=%x, extended_cif=%x\n",stack,ecif);
#endif
  /* If we returning a structure larger than 12bytes,
     we set the first parameter register
     to the address of where we are returning this structure.  
   */  

  if (ecif->cif->flags == FFI_TYPE_STRUCT &&
      (ecif->cif->rtype->size > 12))
     arg_ptr += (unsigned long) ecif->rvalue;

   /*Now for the arguments.  */
 
  for (type_ptr = ecif->cif->arg_types, i = ecif->cif->nargs;
       i > 0;
       i--, type_ptr++, p_argv++)
    {
      void *arg = *p_argv;
      int type = (*type_ptr)->type;
      int size = (*type_ptr)->size;

     /*  Check how a structure type is passed.   */
      if (type == FFI_TYPE_STRUCT) {
	memcpy(arg_ptr, (char*)p_argv, (*type_ptr)->size);
	arg_ptr += (*type_ptr)->size;
	continue;
      }

     /*  Now handle all primitive int/pointer/float data types.  */
      switch (type) 
	{
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE 
	  case FFI_TYPE_LONGDOUBLE: 
	    *(long double *) arg_ptr = * (long double *) (*p_argv);
	    break;
#endif

	  case FFI_TYPE_DOUBLE:
	  case FFI_TYPE_COMPLEX:
	    *(double *) arg_ptr = * (double *) (*p_argv);
	    break;
	
	  case FFI_TYPE_FLOAT:
	    *(float *) arg_ptr = * (float *) (*p_argv);
	    break;

	  case FFI_TYPE_POINTER:
	    *(unsigned long *) arg_ptr = * (unsigned long *) (* p_argv); 
	    break;
 
	  case FFI_TYPE_SINT64:
	    *(signed long long *) arg_ptr = * (signed long long *) (* p_argv);
	    break;

	  case FFI_TYPE_UINT64:
	    *(unsigned long long *) arg_ptr = * (unsigned long long *) (* p_argv);
	    break;
 
	  case FFI_TYPE_UINT32:
	    *(unsigned int *) arg_ptr = * (unsigned int *) (*p_argv);
	    break;
 
	  case FFI_TYPE_SINT32:
	  case FFI_TYPE_INT:
	    *(signed int *) arg_ptr = * (signed int *) (*p_argv);
	    break;
 
	  case FFI_TYPE_UINT16:
	    *(unsigned short *) arg_ptr = * (unsigned short *) (* p_argv);
			arg_ptr += 2;
	    break;
 
	  case FFI_TYPE_SINT16:
	    *(signed short *) arg_ptr = * (signed short *) (* p_argv);
			arg_ptr += 2;
	    break;

	  case FFI_TYPE_UINT8:
	    *(unsigned char *) arg_ptr = * (unsigned char *) (* p_argv);
					arg_ptr += 3;
	    break;
 
	  case FFI_TYPE_SINT8:
	    *(signed char *) arg_ptr = * (signed char*) (* p_argv);
			arg_ptr += 3;
	    break;
 
	  default:
	    FFI_ASSERT (0);
	    break;
        }
      arg_ptr += size;
    }
  
}

/*======================== End of Routine ============================*/
 
/*====================================================================*/
/*                                                                    */
/* Name     - ffi_prep_cif_machdep.                                   */
/*                                                                    */
/* Function - Perform machine dependent CIF processing.               */
/*                                                                    */
/*====================================================================*/
 
ffi_status
ffi_prep_cif_machdep(ffi_cif *cif)
{
  size_t struct_size = 0;
  int n_gpr = 0;
  int n_fpr = 0;
  int n_ov = 0;

  ffi_type **ptr;
  int i;

  /* Determine return value handling.  
     Integral values <=4bytes are widened and put in GPR3
     Integral values >4bytes and <=8bytes are widened and put in
     GPR2 (left most 32-bits) and GPR3 (right most 32-bits)
     Floating point values, including complex type, are returned in 
     FPR0, FPR2, FPR4, FPR6 (as many registers as required)
     Aggregates size of <=4 are returned GPR1 (left adjusted)
     Aggregates size between 5bytes-8bytes are returned in GPR1 and 
     GPR2 (left adjusted)
     Aggregates size between 9bytes-12bytes are returned in GPR1, GPR2,
     and GPR3 (left adjusted)
     Anything greater in size and anyother type is returned in a buffer,
     the buffer is passed in as hidden first argument.
     */

  switch (cif->rtype->type)
    {
      /* Void is easy.  */
      case FFI_TYPE_VOID:
	cif->flags = FFI390_RET_VOID;
	break;

      /* Structures are returned in GPR or buffer depending on size.  */
      case FFI_TYPE_STRUCT:
	struct_size = cif->rtype->size;
	if (struct_size <= 4)
	  cif->flags = FFI390_RET_INT32;
	else if (struct_size <= 8)
	  cif->flags = FFI390_RET_INT64;
	else if (struct_size <= 12)
	  cif->flags = FFI390_RET_STRUCT;
	else
	  n_ov = struct_size;
	break; 

      /* Floating point and complex values are returned in fpr0, 2, 4, 6 */
      case FFI_TYPE_FLOAT:
      case FFI_TYPE_COMPLEX:
	cif->flags = FFI390_RET_FLOAT;
	break;

      case FFI_TYPE_DOUBLE:
	cif->flags = FFI390_RET_DOUBLE;
	break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
      case FFI_TYPE_LONGDOUBLE:
	cif->flags = FFI_TYPE_STRUCT;
	break;
#endif
      /* Integer values are returned in gpr 3 (and gpr 2
	 for 64-bit values on 31-bit machines).  */
      case FFI_TYPE_UINT64:
      case FFI_TYPE_SINT64:
	cif->flags = FFI390_RET_INT64;
	break;

      case FFI_TYPE_POINTER:
      case FFI_TYPE_INT:
      case FFI_TYPE_UINT32:
      case FFI_TYPE_SINT32:
      case FFI_TYPE_UINT16:
      case FFI_TYPE_SINT16:
      case FFI_TYPE_UINT8:
      case FFI_TYPE_SINT8:
	/* These are to be extended to word size.  */
	cif->flags = FFI390_RET_INT32;
	break;
 
      default:
        FFI_ASSERT (0);
        break;
    }

  /* Now for the arguments.  */
 
  for (ptr = cif->arg_types, i = cif->nargs;
       i > 0;
       i--, ptr++)
    {
      int type = (*ptr)->type;

      /* Check how a structure type is passed.  */
      if (type == FFI_TYPE_STRUCT) 
	{
		type = ffi_check_struct_type (*ptr);

	  /* If we pass the struct via pointer, we must reserve space
	     to copy its data for proper call-by-value semantics.  */
	  if (type == FFI_TYPE_POINTER)
	    struct_size += ROUND_SIZE ((*ptr)->size);
	}

      /* Now handle all primitive int/float data types.  */
      switch (type) 
      {
       	/* The first MAX_FPRARGS floating point arguments
	     go in FPRs, the rest overflow to the stack.  */ 
	
	case FFI_TYPE_LONGDOUBLE:
	  if (n_fpr < MAX_FPRARGS)
	    n_fpr+=2;
	  else
	    n_ov += sizeof(long double) / sizeof(long);
	  break; 
	
	case FFI_TYPE_COMPLEX:
       	case FFI_TYPE_DOUBLE:
       	case FFI_TYPE_FLOAT:
	  if (n_fpr < MAX_FPRARGS)
	    n_fpr++;
	  else
	    n_ov += sizeof (double) / sizeof (long);
	  break; 
	  
	  /* On 31-bit machines, 64-bit integers are passed in GPR pairs,
	     if one is still available, or else on the stack.  If only one
	     register is free, skip the register (it won't be used for any 
	     subsequent argument either).  */ 
	
	case FFI_TYPE_UINT64:
       	case FFI_TYPE_SINT64:
	  if (n_gpr == MAX_GPRARGS-1)
	    n_gpr = MAX_GPRARGS;
	  if (n_gpr < MAX_GPRARGS)
	    n_gpr += 2;
	  else
	    n_ov += 2;
	  break; 
	  
	  /* Everything else is passed in GPRs (until MAX_GPRARGS
	     have been used) or overflows to the stack.  */ 
	
	default: 
	  if (n_gpr < MAX_GPRARGS)
	    n_gpr++;
	  else
	    n_ov++;
	  break;
      }
    }

  /* Total stack space as required for:
     -empty slots for arguments passed in registers
     - overflow arguments,
     - and temporary structure copies.  */

  cif->bytes = ROUND_SIZE ((n_ov * sizeof (long)) + (n_fpr * sizeof (long long)) + (n_gpr * sizeof (long)) ) + struct_size;
/*  printf("prep_cif_machdep_cif_bytes: %d n_gpr=%d n_ov=%d n_fpr=%d\n",cif->bytes,n_gpr,n_ov,n_fpr); */
 
  return FFI_OK;
}
 
/*======================== End of Routine ============================*/
 
/*====================================================================*/
/*                                                                    */
/* Name     - ffi_call.                                               */
/*                                                                    */
/* Function - Call the FFI routine.                                   */
/*                                                                    */
/*====================================================================*/
 
void
ffi_call(ffi_cif *cif,
	 void (*fn)(void),
	 void *rvalue,
	 void **avalue)
{
  int ret_type = cif->flags;
  extended_cif ecif;
 
  ecif.cif    = cif;
  ecif.avalue = avalue;
  ecif.rvalue = rvalue;

  /* If we don't have a return value, we need to fake one.  */
  if (rvalue == NULL)
    {
  /*    if (ret_type == FFI_TYPE_STRUCT)
	ecif.rvalue = alloca (cif->rtype->size);
      else
   */   
	ret_type = FFI_TYPE_VOID;
    } 

  switch (cif->abi)
    {
      case FFI_SYSV:
	
        ffi_call_SYSV(fn, &ecif, cif->flags, ecif.rvalue, cif->bytes, cif->nargs, (*cif->arg_types)->size);
#ifdef FFI_DEBUG
	printf("called_ffi_call_sysv nargs=%d\n",cif->nargs);
#endif
        break;
 
      default:
        FFI_ASSERT (0);
        break;
    }
}
 
/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_closure_helper_SYSV.                                */
/*                                                                    */
/* Function - Call a FFI closure target function.                     */
/*                                                                    */
/*====================================================================*/
 
void
ffi_closure_helper_SYSV (ffi_closure *closure,
			 unsigned long *p_gpr,
			 unsigned long long *p_fpr,
			 unsigned long *p_ov)
{
  unsigned long long ret_buffer;

  void *rvalue = &ret_buffer;
  void **avalue;
  void **p_arg;

  int n_gpr = 0;
  int n_fpr = 0;
  int n_ov = 0;

  ffi_type **ptr;
  int i;

  /* Allocate buffer for argument list pointers.  */

  p_arg = avalue = alloca (closure->cif->nargs * sizeof (void *));

  /* If we returning a structure, pass the structure address 
     directly to the target function.  Otherwise, have the target 
     function store the return value to the GPR save area.  */

  if (closure->cif->flags == FFI_TYPE_STRUCT)
    rvalue = (void *) p_gpr[n_gpr++];

  /* Now for the arguments.  */

  for (ptr = closure->cif->arg_types, i = closure->cif->nargs;
       i > 0;
       i--, p_arg++, ptr++)
    {
      int deref_struct_pointer = 0;
      int type = (*ptr)->type;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
      /* 16-byte long double is passed like a struct.  */
      if (type == FFI_TYPE_LONGDOUBLE)
	type = FFI_TYPE_STRUCT;
#endif

      /* Check how a structure type is passed.  */
      if (type == FFI_TYPE_STRUCT || type == FFI_TYPE_COMPLEX)
	{
	  if (type == FFI_TYPE_COMPLEX)
	    type = FFI_TYPE_POINTER;
	  else
	    type = ffi_check_struct_type (*ptr);

	  /* If we pass the struct via pointer, remember to 
	     retrieve the pointer later.  */
	  if (type == FFI_TYPE_POINTER)
	    deref_struct_pointer = 1;
	}

      /* Pointers are passed like UINTs of the same size.  */
      if (type == FFI_TYPE_POINTER)
#ifdef __s390x__
	type = FFI_TYPE_UINT64;
#else
	type = FFI_TYPE_UINT32;
#endif

      /* Now handle all primitive int/float data types.  */
      switch (type) 
	{
	  case FFI_TYPE_DOUBLE:
	    if (n_fpr < MAX_FPRARGS)
	      *p_arg = &p_fpr[n_fpr++];
	    else
	      *p_arg = &p_ov[n_ov], 
	      n_ov += sizeof (double) / sizeof (long);
	    break;
	
	  case FFI_TYPE_FLOAT:
	    if (n_fpr < MAX_FPRARGS)
	      *p_arg = &p_fpr[n_fpr++];
	    else
	      *p_arg = (char *)&p_ov[n_ov++] + sizeof (long) - 4;
	    break;
 
	  case FFI_TYPE_UINT64:
	  case FFI_TYPE_SINT64:
#ifdef __s390x__
	    if (n_gpr < MAX_GPRARGS)
	      *p_arg = &p_gpr[n_gpr++];
	    else
	      *p_arg = &p_ov[n_ov++];
#else
	    if (n_gpr == MAX_GPRARGS-1)
	      n_gpr = MAX_GPRARGS;
	    if (n_gpr < MAX_GPRARGS)
	      *p_arg = &p_gpr[n_gpr], n_gpr += 2;
	    else
	      *p_arg = &p_ov[n_ov], n_ov += 2;
#endif
	    break;
 
	  case FFI_TYPE_INT:
	  case FFI_TYPE_UINT32:
	  case FFI_TYPE_SINT32:
	    if (n_gpr < MAX_GPRARGS)
	      *p_arg = (char *)&p_gpr[n_gpr++] + sizeof (long) - 4;
	    else
	      *p_arg = (char *)&p_ov[n_ov++] + sizeof (long) - 4;
	    break;
 
	  case FFI_TYPE_UINT16:
	  case FFI_TYPE_SINT16:
	    if (n_gpr < MAX_GPRARGS)
	      *p_arg = (char *)&p_gpr[n_gpr++] + sizeof (long) - 2;
	    else
	      *p_arg = (char *)&p_ov[n_ov++] + sizeof (long) - 2;
	    break;

	  case FFI_TYPE_UINT8:
	  case FFI_TYPE_SINT8:
	    if (n_gpr < MAX_GPRARGS)
	      *p_arg = (char *)&p_gpr[n_gpr++] + sizeof (long) - 1;
	    else
	      *p_arg = (char *)&p_ov[n_ov++] + sizeof (long) - 1;
	    break;
 
	  default:
	    FFI_ASSERT (0);
	    break;
        }

      /* If this is a struct passed via pointer, we need to
	 actually retrieve that pointer.  */
      if (deref_struct_pointer)
	*p_arg = *(void **)*p_arg;
    }


  /* Call the target function.  */
  (closure->fun) (closure->cif, rvalue, avalue, closure->user_data);

  /* Convert the return value.  */
  switch (closure->cif->rtype->type)
    {
      /* Void is easy, and so is struct.  */
      case FFI_TYPE_VOID:
      case FFI_TYPE_STRUCT:
      case FFI_TYPE_COMPLEX:
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
      case FFI_TYPE_LONGDOUBLE:
#endif
	break;

      /* Floating point values are returned in fpr 0.  */
      case FFI_TYPE_FLOAT:
	p_fpr[0] = (long long) *(unsigned int *) rvalue << 32;
	break;

      case FFI_TYPE_DOUBLE:
	p_fpr[0] = *(unsigned long long *) rvalue;
	break;

      /* Integer values are returned in gpr 2 (and gpr 3
	 for 64-bit values on 31-bit machines).  */
      case FFI_TYPE_UINT64:
      case FFI_TYPE_SINT64:
#ifdef __s390x__
	p_gpr[0] = *(unsigned long *) rvalue;
#else
	p_gpr[0] = ((unsigned long *) rvalue)[0],
	p_gpr[1] = ((unsigned long *) rvalue)[1];
#endif
	break;

      case FFI_TYPE_POINTER:
      case FFI_TYPE_UINT32:
      case FFI_TYPE_UINT16:
      case FFI_TYPE_UINT8:
	p_gpr[0] = *(unsigned long *) rvalue;
	break;

      case FFI_TYPE_INT:
      case FFI_TYPE_SINT32:
      case FFI_TYPE_SINT16:
      case FFI_TYPE_SINT8:
	p_gpr[0] = *(signed long *) rvalue;
	break;

      default:
        FFI_ASSERT (0);
        break;
    }
}
 
/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_prep_closure_loc.                                   */
/*                                                                    */
/* Function - Prepare a FFI closure.                                  */
/*                                                                    */
/*====================================================================*/
 
ffi_status
ffi_prep_closure_loc (ffi_closure *closure,
		      ffi_cif *cif,
		      void (*fun) (ffi_cif *, void *, void **, void *),
		      void *user_data,
		      void *codeloc)
{
  if (cif->abi != FFI_SYSV)
    return FFI_BAD_ABI;

#ifndef __s390x__
  *(short *)&closure->tramp [0] = 0x0d10;   /* basr %r1,0 */
  *(short *)&closure->tramp [2] = 0x9801;   /* lm %r0,%r1,6(%r1) */
  *(short *)&closure->tramp [4] = 0x1006;
  *(short *)&closure->tramp [6] = 0x07f1;   /* br %r1 */
  *(long  *)&closure->tramp [8] = (long)codeloc;
  *(long  *)&closure->tramp[12] = (long)&ffi_closure_SYSV;
#else
  *(short *)&closure->tramp [0] = 0x0d10;   /* basr %r1,0 */
  *(short *)&closure->tramp [2] = 0xeb01;   /* lmg %r0,%r1,14(%r1) */
  *(short *)&closure->tramp [4] = 0x100e;
  *(short *)&closure->tramp [6] = 0x0004;
  *(short *)&closure->tramp [8] = 0x07f1;   /* br %r1 */
  *(long  *)&closure->tramp[16] = (long)codeloc;
  *(long  *)&closure->tramp[24] = (long)&ffi_closure_SYSV;
#endif 
 
  closure->cif = cif;
  closure->user_data = user_data;
  closure->fun = fun;
 
  return FFI_OK;
}

/*======================== End of Routine ============================*/
 
