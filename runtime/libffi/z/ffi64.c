/* -----------------------------------------------------------------------
   ffi64.c - Copyright (c) 2000, 2007 Software AG
             Copyright (c) 2008 Red Hat, Inc
             Copyright IBM Corp. 2016

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

/* Flags to control the behaviour of CEL4RO31. Used by RO31_CB->flags */
#define FFI390_CEL4RO31_FLAG_LOAD_DLL              0x80000000
#define FFI390_CEL4RO31_FLAG_QUERY_TARGET_FUNC     0x40000000
#define FFI390_CEL4RO31_FLAG_EXECUTE_TARGET_FUNC   0x20000000

/* Return codes for CEL4RO31 */
#define FFI390_CEL4RO31_RETCODE_OK                                   0x0
#define FFI390_CEL4RO31_RETCODE_ERROR_MULTITHREAD_INVOCATION         0x1
#define FFI390_CEL4RO31_RETCODE_ERROR_STORAGE_ISSUE                  0x2
#define FFI390_CEL4RO31_RETCODE_ERROR_FAILED_AMODE31_ENV             0x3
#define FFI390_CEL4RO31_RETCODE_ERROR_FAILED_LOAD_TARGET_DLL         0x4
#define FFI390_CEL4RO31_RETCODE_ERROR_FAILED_QUERY_TARGET_FUNC       0x5

/* Return codes for CELQGIPB */
#define FFI390_CELQGIPB_RETCODE_INITIAL                0xFFFFFFFF
#define FFI390_CELQGIPB_RETCODE_OK                     0x0
#define FFI390_CELQGIPB_RETCODE_FAILED_ALLOC           0x1
#define FFI390_CELQGIPB_RETCODE_DEPTH_EXCEEDED         0x2
#define FFI390_CELQGIPB_RETCODE_OTHER_THREAD_SWITCHING 0x3

/* Fixed length Control Block structure CEL4RO31 */
typedef struct ffi_cel4ro31_control_block {
  uint32_t version;               /**< (Input) A integer which contains the version of RO31_INFO */
  uint32_t length;                /**< (Input) A integer which contains the total length of RO31_INFO */
  uint32_t flags;                 /**< (Input) Flags to control the behavior of CEL4RO31 */
  uint32_t module_offset;         /**< (Input) Offset to RO31_module section from start of RO31_CB. Req'd for dll load flag. */
  uint32_t function_offset;       /**< (Input) Offset to RO31_function section from start of RO31_CB. Req'd for dll query flag. */
  uint32_t arguments_offset;      /**< (Input) Offset to outgoing arguments section from start of RO31_CB. Req'd for function execution flag. */
  uint32_t dll_handle;            /**< DLL handle of target program (Input) DLL handle if dll query flag. (Output) DLL handle if dll load flag. */
  uint32_t func_desc;             /**< Function descriptor of target program (Input) if function execution flag. (Output) if dll query flag. */
  uint32_t gpr15_return_value;    /**< (Output) Return GPR buffer containing 32-bit GPR15 value when target program returned after execution. */
  uint32_t gpr0_return_value;     /**< (Output) Return GPR buffer containing 32-bit GPR0 value when target program returned after execution. */
  uint32_t gpr1_return_value;     /**< (Output) Return GPR buffer containing 32-bit GPR1 value when target program returned after execution. */
  uint32_t gpr2_return_value;     /**< (Output) Return GPR buffer containing 32-bit GPR2 value when target program returned after execution. */
  uint32_t gpr3_return_value;     /**< (Output) Return GPR buffer containing 32-bit GPR3 value when target program returned after execution. */
  int32_t retcode;                /**< (Output) Return code of CEL4RO31. */
  uint32_t arguments_length;      /**< (Input) Variable arguments length, in fullword integer (i.e. num of 4-byte slots). */
} ffi_cel4ro31_control_block;

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

/* Function descriptor of CEL4RO31 runtime call from GTCA control block */
typedef void cel4ro31_cwi_func(void*);
#define FFI390_CEL4RO31_FNPTR ((cel4ro31_cwi_func*)((char*)(*(int*)(((char*)__gtca())+1096))+8))

/* Function descriptor of CELQGIPB runtime call from GTCA control block */
typedef void celqgipb_cwi_func(uint32_t*, ffi_cel4ro31_control_block**, uint32_t*);
#define FFI390_CELQGIPB_FNPTR ((celqgipb_cwi_func*)((char*)(*(int*)(((char*)__gtca())+1096))+96))

/* CEEPCB_3164 indicator is the 6th bit of PCB's CEEPCBFLAG6 field (byte 344).
 * CEECAAPCB field is byte 912 of CAA.
 *
 * References for AMODE64:
 * https://www.ibm.com/docs/en/zos/2.4.0?topic=mappings-language-environment-process-control-block
 * https://www.ibm.com/docs/en/zos/2.4.0?topic=mappings-language-environment-common-anchor-area
 */
#define FFI390_CEL4RO31_CEEPCBFLAG6_VALUE *((char *)(*(long *)(((char *)__gtca())+912))+344)
#define FFI390_CEL4RO31_CEEPCB_3164_MASK 0x4

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
/* Name     - ffi_prep_args_cel4ro31.                                 */
/*                                                                    */
/* Function - Prepare parameters for call to function via CEL4RO31    */
/*                                                                    */
/* ffi_prep_args is called by ffi_call_CEL4RO31 after it has          */
/* allocated argument list storage as part of its control block.      */
/*                                                                    */
/*====================================================================*/

void
ffi_prep_args_cel4ro31 (unsigned char *arg_list_ptr, extended_cif *ecif)
{
  /* The argument list is laid out in sequential fashion for all
   * parameter types under Standard/MVS linkage.
   */
  int i;
  ffi_type **type_ptr;
  void **p_argv = ecif->avalue;
  unsigned char* arg_ptr = arg_list_ptr;

  /* A hidden first parameter pointing to a buffer needs to be
   * added for floating point and structure return types.
   */
  int has_hidden_param = FFI390_RET_STRUCT == ecif->cif->flags ||
                         FFI390_RET_FLOAT == ecif->cif->flags ||
                         FFI390_RET_DOUBLE == ecif->cif->flags;

#ifdef FFI_DEBUG
  printf("prep_args_cel4ro31: arg_list_ptr=%x, extended_cif=%x\n", arg_list_ptr, ecif);
#endif

  if (has_hidden_param)
  {
    arg_ptr += sizeof(int);
  }

   /* Now for the arguments.  CEL4RO31 is targetting 31-bit callees.
    * Slots are considered 4-bytes.
    */
  for (type_ptr = ecif->cif->arg_types, i = ecif->cif->nargs;
       i > 0;
       i--, type_ptr++, p_argv++)
    {
      void *arg = *p_argv;
      int type = (*type_ptr)->type;
      int size = (*type_ptr)->size;

      /* If structure type, cop how a structure type is passed. */
      if (type == FFI_TYPE_STRUCT)
      {
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
        *(void **) arg_ptr = * (void**) (* p_argv);
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

      /* Expand the values to full 32-bit */
      case FFI_TYPE_UINT16:
        *(unsigned int *) arg_ptr = * (unsigned short *) (* p_argv);
        arg_ptr += 2;
        break;

      case FFI_TYPE_SINT16:
        *(signed int *) arg_ptr = * (signed short *) (* p_argv);
        arg_ptr += 2;
        break;

      case FFI_TYPE_UINT8:
        *(unsigned int *) arg_ptr = * (unsigned char *) (* p_argv);
        arg_ptr += 3;
        break;

      case FFI_TYPE_SINT8:
        *(signed int *) arg_ptr = * (signed char*) (* p_argv);
        arg_ptr += 3;
        break;

      default:
        FFI_ASSERT (0);
        break;
      }
    arg_ptr += size;
  }

  if (has_hidden_param)
  {
    /* Update the hidden param with the address of the current arg_ptr.
      * arg_list is guaranteed to be below-the-bar, so we can safely truncate
      * to 32-bits.
      */
    *(unsigned int *) arg_list_ptr = (unsigned int) ((unsigned long long)arg_ptr & 0xFFFFFFFF);
  }

}

/*======================== End of Routine ============================*/
/*====================================================================*/
/*                                                                    */
/* Name     - ffi_prep_cif_machdep_cel4ro31.                          */
/*                                                                    */
/* Function - Perform machine dependent CIF processing for CEL4RO31.  */
/*                                                                    */
/*====================================================================*/
ffi_status
ffi_prep_cif_machdep_cel4ro31(ffi_cif *cif)
{
  size_t struct_size = 0;
  int n_ov = 0;

  ffi_type **ptr;
  int i;

  /* Check if LE APIs are available. */
  if (0 == (FFI390_CEL4RO31_CEEPCBFLAG6_VALUE & FFI390_CEL4RO31_CEEPCB_3164_MASK))
  {
    return FFI_BAD_ABI;
  }

  /* CEL4RO31 supports only 31-bit MVS/Standard Linkage only.
   *
   * https://www-01.ibm.com/servers/resourcelink/svc00100.nsf/pages/zOSV2R4sa380688/$file/ceev100_v2r4.pdf (page 116-118)
   * https://www.ibm.com/support/knowledgecenter/SSLTBW_2.4.0/com.ibm.zos.v2r4.ccrug00/mvslnkcnv.htm
   *
   * Linkage Summary:
   *  - Arguments are passed via Parameter List pointed to by GPR1, with each slot 4 bytes in length.
   *  - 32-bit integral types are returned in GPR15.  64-bit integral types are returned in GPR15:GPR0.
   *  - Floating point and structure return values are placed in storage whose address is passed as the first parameter
   *
   * CEL4RO31 specific details:
   *  - All arguments of the Parameter List are set up in the storage referenced by RO31_arguments of the RO31 control block.
   *  - CEL4RO31 will update GPR1 to point to this storage before invoking callee function.
   *  - Floating point arguments and return values in registers are not touched by CEL4RO31.
   *
   * Note: Each Slot is considered 4-bytes given the 31-bit target.
   */

  /* Determine return value handling.
   * Integral values <=4bytes are widened and put in GPR15
   * Integral values >4bytes and <=8bytes are widened and put in
   * GPR15 (left most 32-bits) and GPR0 (right most 32-bits).
   * Floating point values are returned in a buffer referenced by hidden first parameter.
   * Aggregates are returned in a buffer pointed to by hidden first parameter.
   */
  switch (cif->rtype->type)
  {
    /* Void is easy.  */
    case FFI_TYPE_VOID:
      cif->flags = FFI390_RET_VOID;
      break;

    /* Structures are returned in storage pointed to by first parameter.
      * Which will be allocated at the end of the arguments area, as we need
      * it to be guaranteed to be below the bar storage.
      * Capture size of return type + 1 slot for hidden param.
      */
    case FFI_TYPE_STRUCT:
      struct_size = cif->rtype->size;
      cif->flags = FFI390_RET_STRUCT;
      n_ov = 1;
      break;

    /* Floating point and complex values in a 4-byte or 8-byte slot referenced
      * by the first parameter.  Capture size of return type + 1 slot for hidden param.
      */
    case FFI_TYPE_FLOAT:
      cif->flags = FFI390_RET_FLOAT;
      n_ov = sizeof(float) / sizeof(int) + 1;
      break;

    case FFI_TYPE_DOUBLE:
    case FFI_TYPE_COMPLEX:
      cif->flags = FFI390_RET_DOUBLE;
      n_ov = sizeof(double) / sizeof(int) + 1;
      break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
    case FFI_TYPE_LONGDOUBLE:
      cif->flags = FFI390_RET_LDBLE;
      n_ov = sizeof(long double) / sizeof(int) + 1;
      break;
#endif

    /* 64-bit Integer values are returned in GPR15:GPR0. */
    case FFI_TYPE_UINT64:
    case FFI_TYPE_SINT64:
    case FFI_TYPE_POINTER:
      cif->flags = FFI390_RET_INT64;
      break;

    /* 32-bit Integer values are returned in GPR15. */
    case FFI_TYPE_INT:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_UINT8:
    case FFI_TYPE_SINT8:
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

    /* Now handle all primitive int/float data types. Everything
      * gets passed via the argument list. */
    switch (type)
    {
      case FFI_TYPE_STRUCT:
        /** Structures will passed via pointer, we must reserve
         * space to copy its data to below the bar storage, for
         * proper call-by-value semantics.
         */
        struct_size += ROUND_SIZE ((*ptr)->size);
        n_ov++;
        break;

      case FFI_TYPE_LONGDOUBLE:
        n_ov += sizeof(long double) / sizeof(int);
        break;

      case FFI_TYPE_FLOAT:
        n_ov += sizeof(float) / sizeof(int);
        break;

      case FFI_TYPE_DOUBLE:
      case FFI_TYPE_COMPLEX:
        n_ov += sizeof(double) / sizeof(int);
        break;

      /* 64-bit integer types requires two 32-bit slots. */
      case FFI_TYPE_UINT64:
      case FFI_TYPE_SINT64:
      case FFI_TYPE_POINTER:
        n_ov += 2;
        break;

      /* Everything else is as a single slot. */
      default:
        n_ov++;
        break;
    }
  }

  cif->bytes = n_ov * sizeof(int) + struct_size;

  return FFI_OK;
}

/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_call_CEL4RO31.                                      */
/*                                                                    */
/* Function - Invoke the target function via CEL4RO31 interface.      */
/*                                                                    */
/*====================================================================*/

void
ffi_call_CEL4RO31(void (*fn)(void), extended_cif *ecif)
{
  unsigned int IPB_length = sizeof(ffi_cel4ro31_control_block) + ecif->cif->bytes;;
  unsigned int IPB_retcode = FFI390_CELQGIPB_RETCODE_INITIAL;
  ffi_cel4ro31_control_block *control_block = NULL;
  int is_malloc31_buffer = 0;
  unsigned char* argument_list_ptr = NULL;

  if (NULL != FFI390_CELQGIPB_FNPTR)
  {
    FFI390_CELQGIPB_FNPTR(&IPB_length, &control_block, &IPB_retcode);
  }

  if ((FFI390_CELQGIPB_RETCODE_OK != IPB_retcode))
  {
    /* IPB unavailable or request failed.  Attempt to allocate below-the-bar storage ourselves. */
    control_block = (ffi_cel4ro31_control_block *) __malloc31(IPB_length);
    is_malloc31_buffer = 1;
    if (NULL == control_block)
    {
      FFI_ASSERT(0);
      return;
    }
  }

  /* Initialize the RO31 control block members and calculate the various offsets. */
  control_block->version = 1;
  control_block->flags = FFI390_CEL4RO31_FLAG_EXECUTE_TARGET_FUNC;
  control_block->module_offset = 0;
  control_block->function_offset = 0;
  control_block->retcode = 0;
  control_block->func_desc = (unsigned int)(((unsigned long)fn) & 0xFFFFFFFF);
  control_block->dll_handle = 0;

  /* Control block length. */
  control_block->length = IPB_length;

  /* If no arguments, arguments_offset needs to be 0.  Otherwise, it points to the memory
   * immediately past the fixed control_block to the 'arguments_length' member.  We don't
   * use cif->nargs here because there may be a hidden parameter for certain return types.
   * 'arguments_length' member contains the number of fullword argument slots.
   */
  control_block->arguments_offset = (0 == ecif->cif->bytes)? 0 : offsetof(ffi_cel4ro31_control_block, arguments_length);
  control_block->arguments_length = ecif->cif->bytes / sizeof(int);

  /* Prepare arguments into the arg buffer directly after the fixed control block area.
   */
  argument_list_ptr = ((unsigned char *)control_block) + sizeof(ffi_cel4ro31_control_block);
  ffi_prep_args_cel4ro31(argument_list_ptr, ecif);

  FFI390_CEL4RO31_FNPTR(control_block);

  /* Copy over return value from call into returnStorage, if retcode is valid. */
  if (FFI390_CEL4RO31_RETCODE_OK == (control_block->retcode))
  {
    unsigned int *argument_list_first_parm_ptr = (unsigned int *)argument_list_ptr;
    switch (ecif->cif->rtype->type)
    {
      /* Void is easy.  */
      case FFI_TYPE_VOID:
        break;

      /* Structures are returned in storage pointed to by first parameter.
        * which was allocated at the end of the arguments area.  Copy the member
        * values back out to return storage.
        */
      case FFI_TYPE_STRUCT:
        memcpy(ecif->rvalue, (unsigned char*)(argument_list_first_parm_ptr[0]), ecif->cif->rtype->size);
        break;

      /* Floating point and complex values in a 4-byte or 8-byte slot referenced
        * by the first parameter.
        */
      case FFI_TYPE_FLOAT:
        *((float *)ecif->rvalue) = *((float *)argument_list_first_parm_ptr[0]);
        break;

      case FFI_TYPE_DOUBLE:
      case FFI_TYPE_COMPLEX:
        *((double *)ecif->rvalue) = *((double *)argument_list_first_parm_ptr[0]);
        break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
      case FFI_TYPE_LONGDOUBLE:
        *((long double *)ecif->rvalue) = *((long double *)argument_list_first_parm_ptr[0]);
        break;
#endif

      /* 64-bit Integer values are returned in GPR15:GPR0. */
      case FFI_TYPE_UINT64:
      case FFI_TYPE_SINT64:
      case FFI_TYPE_POINTER:
        /* 64-bit values from 31-bit are returned via GPR15:GPR0 */
        *((unsigned long long*)ecif->rvalue) = (((unsigned long long)(control_block->gpr15_return_value) << 32) | control_block->gpr0_return_value);
        break;

      /* 32-bit Integer values are returned in GPR15. */
      case FFI_TYPE_INT:
      case FFI_TYPE_UINT32:
      case FFI_TYPE_SINT32:
      case FFI_TYPE_UINT16:
      case FFI_TYPE_SINT16:
      case FFI_TYPE_UINT8:
      case FFI_TYPE_SINT8:
        *((unsigned long long*)ecif->rvalue) = control_block->gpr15_return_value;
        break;

      default:
        FFI_ASSERT (0);
        break;
    }
  }

  if (is_malloc31_buffer)
  {
  free(control_block);
  }
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
      (ecif->cif->rtype->size > 12)){
     arg_ptr += (unsigned long) ecif->rvalue;

  }
   /*Now for the arguments.  */

  for (type_ptr = ecif->cif->arg_types, i = ecif->cif->nargs;
       i > 0;
       i--, type_ptr++, p_argv++)
    {
      void *arg = *p_argv;
      int type = (*type_ptr)->type;
      int size = (*type_ptr)->size;

     /*  Now handle all primitive int/pointer/float data types.  */
      switch (type)
	{
	  case FFI_TYPE_STRUCT:
	  case FFI_TYPE_STRUCT_FF:
	  case FFI_TYPE_STRUCT_DD:
	    memcpy(arg_ptr, *p_argv, size);
	    break;

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
	    *(void **) arg_ptr = * (void**) (* p_argv);
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
	    break;

	  case FFI_TYPE_SINT16:
	    *(signed short *) arg_ptr = * (signed short *) (* p_argv);
	    break;

	  case FFI_TYPE_UINT8:
	    *(unsigned char *) arg_ptr = * (unsigned char *) (* p_argv);
	    break;

	  case FFI_TYPE_SINT8:
	    *(signed char *) arg_ptr = * (signed char*) (* p_argv);
	    break;

	  default:
	    FFI_ASSERT (0);
	    break;
        }
      arg_ptr += size;

    }

}

/*======================== End of Routine ============================*/

/**
 * Helper functions to know if the given struct needs to be treated for complex
 * type for float or double.
 */
static unsigned short
get_ffi_element_type_in_struct(ffi_type *arg_type)
{
  while ((FFI_TYPE_STRUCT == arg_type->type)
        && (NULL != arg_type->elements[0])
        && (NULL == arg_type->elements[1])
  ) {
    arg_type = arg_type->elements[0];
  }
  return arg_type->type;
}

static unsigned short
ffi_check_struct_for_complex(ffi_type *arg_type)
{
  if ((FFI_TYPE_STRUCT == arg_type->type) && (NULL != arg_type->elements[0]))
  {
    unsigned short firstArgType = get_ffi_element_type_in_struct(arg_type->elements[0]);
    if (FFI_TYPE_FLOAT == firstArgType)
    {
      if ((NULL != arg_type->elements[1])
          && (NULL == arg_type->elements[2])
          && (FFI_TYPE_FLOAT == get_ffi_element_type_in_struct(arg_type->elements[1]))
      ) {
        return FFI_TYPE_STRUCT_FF;
      }
    }
    else if (FFI_TYPE_DOUBLE == firstArgType)
    {
      if ((NULL != arg_type->elements[1])
          && (NULL == arg_type->elements[2])
          && (FFI_TYPE_DOUBLE == get_ffi_element_type_in_struct(arg_type->elements[1]))
      ) {
        return FFI_TYPE_STRUCT_DD;
      }
    }
  }
  return arg_type->type;
}

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

  if (FFI_CEL4RO31 == cif->abi)
    {
      return ffi_prep_cif_machdep_cel4ro31(cif);
    }

  /* 64-bit XPLINK handling below */

  /* Determine return value handling.
     Integral values <=8bytes are widened and put in GPR3
     Integral values >8bytes and <=16bytes are widened and put in
     GPR2 (left most 64-bits) and GPR3 (right most 64-bits)
     Floating point values, including complex type, are returned in
     FPR0, FPR2, FPR4, FPR6 (as many registers as required)
     Aggregates size of <=8 are returned GPR1 (left adjusted)
     Aggregates size between 9bytes-16bytes are returned in GPR1 and
     GPR2 (left adjusted)
     Aggregates size between 17bytes-24bytes are returned in GPR1, GPR2,
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
        if (struct_size <= 24)
	  cif->flags = FFI390_RET_STRUCT;
	else
          n_gpr++;
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
	  case FFI_TYPE_POINTER:
      case FFI_TYPE_INT:
      case FFI_TYPE_UINT32:
      case FFI_TYPE_SINT32:
      case FFI_TYPE_UINT16:
      case FFI_TYPE_SINT16:
      case FFI_TYPE_UINT8:
      case FFI_TYPE_SINT8:

	cif->flags = FFI390_RET_INT64;
	break;




      default:
        FFI_ASSERT (0);
        break;
    }
  cif->rtype->type = ffi_check_struct_for_complex(cif->rtype);
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

		(*ptr)->type = ffi_check_struct_for_complex(*ptr);

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
        if (cif->arg_types != NULL)
          ffi_call_SYSV(fn, &ecif, cif->flags, ecif.rvalue, cif->bytes, cif->nargs, (*cif->arg_types)->size);
        else
          ffi_call_SYSV(fn, &ecif, cif->flags, ecif.rvalue, cif->bytes, cif->nargs, 0);
#ifdef FFI_DEBUG
	printf("called_ffi_call_sysv nargs=%d\n",cif->nargs);
#endif
        break;
      case FFI_CEL4RO31:
        ffi_call_CEL4RO31(fn, &ecif);
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
  if (cif->abi != FFI_SYSV || cif->abi != FFI_CEL4RO31)
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

