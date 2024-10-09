/*******************************************************************************
 * Copyright IBM Corp. and others 1991
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef util_api_h
#define util_api_h

/* @ddr_namespace: map_to_type=UtilApiConstants */

/**
* @file util_api.h
* @brief Public API for the UTIL module.
*
* This file contains public function prototypes and
* type definitions for the UTIL module.
*
*/

#include "j9.h"
#include "jni.h"
#include "cfr.h"
#include "dbg.h"
#include "jvmti.h" /* for hshelp.c functions */
#include "omrutil.h"
#include "omrutilbase.h"
#include "shchelp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	ONLY_SPEC_MODIFIERS,
	INCLUDE_INTERNAL_MODIFIERS
} modifierScope;

typedef enum
{
	MODIFIERSOURCE_FIELD,
	MODIFIERSOURCE_METHOD,
	MODIFIERSOURCE_CLASS,
	MODIFIERSOURCE_METHODPARAMETER
} modifierSource;

/* ---------------- alignedmemcpy.asm ---------------- */
void
alignedMemcpy(J9VMThread *vmStruct, void *dest, void *source, UDATA bytes, UDATA alignment);

void
alignedBackwardsMemcpy(J9VMThread *vmStruct, void *dest, void *source, UDATA bytes, UDATA alignment);

/* ---------------- annhelp.c ---------------- */

/**
 * Check if a fieldref contains the specified Runtime Visible annotation. Fieldref
 * must be resolved.
 *
 * @param currentThread Thread token
 * @param clazz The class the field belongs to.
 * @param cpIndex The constant pool index of the fieldref.
 * @param annotationName The name of the annotation to check for.
 * @return TRUE if the annotation is found, FALSE otherwise.
 */
BOOLEAN
fieldContainsRuntimeAnnotation(J9VMThread *currentThread, J9Class *clazz, UDATA cpIndex, J9UTF8 *annotationName);

/**
 * Check if a method contains the specified Runtime Visible annotation.
 *
 * @param currentThread Thread token
 * @param method The method to be queried
 * @param annotationName The annotation name
 * @return TRUE if the annotation is found, FALSE otherwise.
 */
BOOLEAN
methodContainsRuntimeAnnotation(J9VMThread *currentThread, J9Method *method, J9UTF8 *annotationName);

/* ---------------- argbits.c ---------------- */

/**
* @brief
* @param *signature
* @param *resultArrayBase
* @param resultArraySize
* @param isStatic
* @return void
*/
void
argBitsFromSignature(U_8 * signature, U_32 * resultArrayBase, UDATA resultArraySize, UDATA isStatic);


/* ---------------- bcdump.c ---------------- */

/**
* @brief
* @param portLib
* @param romClass
* @param romMethod
* @param flags
* @return IDATA
*/
IDATA dumpBytecodes(J9PortLibrary * portLib, J9ROMClass * romClass, J9ROMMethod * romMethod, U_32 flags);


/**
* @brief
* @param portLib
* @param romClass
* @param bytecodes
* @param walkStartPC
* @param walkEndPC
* @param flags
* @param *printFunction
* @param *userData
* @param *indent
* @return IDATA
*/
IDATA j9bcutil_dumpBytecodes(J9PortLibrary * portLib, J9ROMClass * romClass,
							 U_8 * bytecodes, UDATA walkStartPC, UDATA walkEndPC,
							 UDATA flags, void *printFunction, void *userData, char *indent);


/* ---------------- binarysup.c ---------------- */

/**
* @brief
* @param data1
* @param length1
* @param data2
* @param length2
* @return IDATA
*/
IDATA
compareUTF8Length(U_8* data1, UDATA length1, void* data2, UDATA length2);


/* ---------------- checkcast.cpp ---------------- */

/**
* @brief	Returns TRUE if instanceClass can be cast to castClass, FALSE otherwise.
* 			Updates the castClassCache.  DO NOT call this without VM access.
*
* @param *instanceClass
* @param *castClass
* @return IDATA
*/
IDATA
instanceOfOrCheckCast( J9Class *instanceClass, J9Class *castClass );

/**
* @brief	Returns TRUE if instanceClass can be cast to castClass, FALSE otherwise.
* 			Does not update the castClassCache.
*
* @param *instanceClass
* @param *castClass
* @return IDATA
*/
IDATA
instanceOfOrCheckCastNoCacheUpdate( J9Class *instanceClass, J9Class *castClass );

/* ---------------- defarg.c ---------------- */

/**
 * Look for the defined argument starting at the bottom of
 * the vmInitArgs->options array.
 *
 * @param vmInitArgs a JavaVMInitArgs structure
 * @param defArg the defined argument to be found
 *
 * @return the argument string value found or NULL
 */
const char*
getDefinedArgumentFromJavaVMInitArgs(JavaVMInitArgs *vmInitArgs, const char *defArg);

/* ---------------- divhelp.c ---------------- */

/**
* @brief Helper function called by VM interpreter, using pointers to
*        values. Calculates quotient of 2 longs.
* @param[in] *a Pointer to long dividend.
* @param[in] *b Pointer to long divisor.
* @param[out] *c Pointer to long quotient.
* @return I_32 0.
*
*/
void helperLongDivideLong(I_64 *a, I_64 *b, I_64 *c);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Calculates the product of
*        2 longs.
* @param[in] *a Pointer to long multiplicand.
* @param[in] *b Pointer to long multiplier.
* @param[out] *c Pointer to long product.
* @return I_32 0.
*/
void helperLongMultiplyLong(I_64 *a, I_64 *b, I_64 *c);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Calculates the remainder when
*        dividing 2 longs.
* @param[in] *a Pointer to long dividend.
* @param[in] *b Pointer to long divisor.
* @param[out] *c Pointer to long remainder.
* @return I_32 0.
*/
void helperLongRemainderLong(I_64 *a, I_64 *b, I_64 *c);


/* ---------------- divhelp.c C-helpers---------------- */

/**
* @brief Helper function used by JIT. Given 2 long
*        values, returns quotient.
* @param[in] a Long dividend.
* @param[in] b Long divisor.
* @return Resulting long quotient.
*
*/
I_64 helperCLongDivideLong(I_64 a, I_64 b);


/**
* @brief Helper function used by JIT. Given 2 long
*        values, returns product.
* @param[in] a Long multiplicand.
* @param[in] b Long multiplier.
* @return Resulting long product.
*
*/
I_64 helperCLongMultiplyLong(I_64 a, I_64 b);


/**
* @brief Helper function used by JIT. Given 2 long
*        values, returns remainder.
* @param[in] a Long dividend.
* @param[in] b Long divisor.
* @return Resulting long remainder.
*
*/
I_64 helperCLongRemainderLong(I_64 a, I_64 b);

/* ---------------- dllloadinfo.c ---------------- */

/**
* @brief Retrieves the load info for the appropriate
*        GC DLL based on reference mode.
* @param[in] vm The Java VM.
* @return J9VMDllLoadInfo for the GC DLL selected.
*/
J9VMDllLoadInfo *getGCDllLoadInfo(J9JavaVM *vm);

/**
* @brief Retrieves the load info for the appropriate
*        VERBOSE DLL based on reference mode.
* @param[in] vm The Java VM.
* @return J9VMDllLoadInfo for the VERBOSE DLL selected.
*/
J9VMDllLoadInfo *getVerboseDllLoadInfo(J9JavaVM *vm);

/* ---------------- eventframe.c ---------------- */

/**
* @brief
* @param currentThread
* @param hadVMAccess
* @return void
*/
void
popEventFrame(J9VMThread * currentThread, UDATA hadVMAccess);


/**
* @brief
* @param currentThread
* @param wantVMAccess
* @param jniRefSlots
* @return UDATA
*/
UDATA
pushEventFrame(J9VMThread * currentThread, UDATA wantVMAccess, UDATA jniRefSlots);


/* ---------------- extendedmethodblockaccess.c ---------------- */

/**
* @brief
* @param method
* @return U_8 *
*/
U_8 *
fetchMethodExtendedFlagsPointer(J9Method* method);


/**
* @brief
* @param vm
* @param mtFlag
* @param flags
* @return void
*/
void
setExtendedMethodFlags(J9JavaVM * vm, U_8 * mtFlag, U_8 flags);


/**
* @brief
* @param vm
* @param mtFlag
* @param flags
* @return void
*/
void
clearExtendedMethodFlags(J9JavaVM * vm, U_8 * mtFlag, U_8 flags);


/* ---------------- fieldutil.c ---------------- */

extern const U_8 fieldModifiersLookupTable[];

/**
* @brief
* @param field
* @return J9UTF8 *
*/
J9UTF8 * romFieldGenericSignature(J9ROMFieldShape * field);

/**
* Get the start of the field annotation data from the ROMclass field (including the 4-byte length)
* @param field
* @return pointer to data, or NULL if the field does not contain annotations
*/
U_32 * getFieldAnnotationsDataFromROMField(J9ROMFieldShape * field);

/**
* Get the start of the field type annotation data from the ROMclass field (including the 4-byte length)
* @param field
* @return pointer to data, or NULL if the field does not contain type annotations
*/
U_32 * getFieldTypeAnnotationsDataFromROMField(J9ROMFieldShape * field);

/**
* @brief
* @param field
* @return U_32 *
*/
U_32 * getPackedLengthAnnotationValueFromROMField(J9ROMFieldShape * field);

/**
* @brief
* @param field
* @return U_32 *
*/
U_32 * romFieldInitialValueAddress(J9ROMFieldShape * field);


/**
* @brief
* @param modifiers
* @return UDATA
*/
UDATA romFieldSize(J9ROMFieldShape *romField);


/**
* @brief
* @param state
* @return J9ROMFieldShape *
*/
J9ROMFieldShape * romFieldsNextDo(J9ROMFieldWalkState* state);


/**
* @brief
* @param romClass
* @param state
* @return J9ROMFieldShape *
*/
J9ROMFieldShape * romFieldsStartDo(J9ROMClass * romClass, J9ROMFieldWalkState* state);

typedef enum J9WalkFieldAction {
	J9WalkFieldActionContinue,
	J9WalkFieldActionStop
} J9WalkFieldAction;

typedef struct J9WalkFieldHierarchyState {
	J9WalkFieldAction (*fieldCallback)(J9ROMFieldShape *romField, J9Class *declaringClass, void *userData); /**< callback invoked on each field */
	void *userData; /**< data passed to fieldCallback */
} J9WalkFieldHierarchyState;

/**
 * Walk the ROM fields of a class, including inherited fields.
 * Invoke a callback on each field.
 *
 * @param[in] clazz The class.
 * @param[in,out] state Struct containing the callback and other walk info.
 */
void walkFieldHierarchyDo(J9Class *clazz, J9WalkFieldHierarchyState *state);


/* ---------------- final.c ---------------- */

/**
* @brief
* @param nameLength
* @param name
* @param sigLength
* @param sig
* @return UDATA
*/
UDATA methodIsFinalInObject(UDATA nameLength, U_8* name, UDATA sigLength, U_8* sig);


/* ---------------- fltconv.c ---------------- */

#if (defined(J9VM_INTERP_FLOAT_SUPPORT))  /* File Level Build Flags */

/**
* @brief Helper function called by VM interpreter, using pointers
*        to values. Converts a double precision number to a
*        floating point value.
* @param[in] *src Pointer to double value to be converted to floating point.
* @param[out] *dst Pointer to the resulting floating point value.
* @return Void.
*
*/
void helperConvertDoubleToFloat(jdouble *src, jfloat *dst);


/**
* @brief Helper function called by VM interpreter, using pointers
*        to values. Converts a double precision number to an integer value.
* @param[in] *src Pointer to double value to be converted to integer.
* @param[out] *dst Pointer to the resulting integer value.
* @return Void.
*
*/
void helperConvertDoubleToInteger(jdouble *src, I_32 *dst);


/**
* @brief Helper function called by VM interpreter, using pointers
*        to values. Converts a double precision number to long value.
* @param[in] *src Pointer to double value to be converted to long.
* @param[out] *dst Pointer to the resulting long value.
* @return Void.
*
*/
void helperConvertDoubleToLong(jdouble *src, I_64 *dst);


/**
* @brief Helper function called by VM interpreter, using pointers
*        to values. Converts floating point number to a double
*        precision value.
* @param[in] *src Pointer to floating point value to be converted to double.
* @param[out] *dst Pointer to the resulting double value.
* @return Void.
*
*/
void helperConvertFloatToDouble(jfloat *src, jdouble *dst);


/**
* @brief Helper function called by VM interpreter, using pointers
*        to values. Converts floating point number to an integer value.
* @param[in] *src Pointer to floating point value to be converted to integer.
* @param[out] *dst Pointer to the resulting integer value.
* @return Void.
*
*/
void helperConvertFloatToInteger(jfloat *src, I_32 *dst);


/**
* @brief Helper function called by VM interpreter, using pointers
*        to values. Converts floating point number to long value.
* @param[in] *src Pointer to floating point value to be converted to long.
* @param[out] *dst Pointer to the resulting long value.
* @return Void.
*
*/
void helperConvertFloatToLong(jfloat *src, I_64 *dst);


/**
* @brief Helper function called by VM interpreter, using pointers
*        to values. Converts an integer number to double.
* @param[in] *src Pointer to integer value to be converted to double.
* @param[out] *dst Pointer to the resulting double value.
* @return Void.
*
*/
void helperConvertIntegerToDouble(I_32 *src, jdouble *dst);


/**
* @brief Helper function called by VM interpreter, using pointers
*        to values. Converts an integer number to floating point.
* @param[in] *src Pointer to integer value to be converted to floating point.
* @param[out] *dst Pointer to the resulting floating point value.
* @return Void.
*
*/
void helperConvertIntegerToFloat(I_32 *src, jfloat *dst);


/**
* @brief Helper function called by VM interpreter, using pointers
*        to values. Converts a long number to double precision.
* @param[in] *src Pointer to long value to be converted to double.
* @param[out] *dst Pointer to the resulting double value.
* @return Void.
*
*/
void helperConvertLongToDouble(I_64 *src, jdouble *dst);


/**
* @brief Helper function called by VM interpreter, using pointers
*        to values. Converts a long number to floating point.
* @param[in] *src Pointer to long value to be converted to floating point.
* @param[out] *dst Pointer to the resulting floating point value.
* @return Void.
*
*/
void helperConvertLongToFloat(I_64 *src, jfloat *dst);


/* ---------------- fltconv.c C-helpers---------------- */

/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given a double number,
*        converts it to floating point and returns the
*        floating point value.
* @param[in] src Double number to be converted to floating point.
* @return Resulting floating point value.
*
*/
jfloat helperCConvertDoubleToFloat(jdouble src);


/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given a double number,
*        converts it to an integer and returns the
*        integer value.
* @param[in] src Double number to be converted to an integer.
* @return Resulting integer value.
*
*/
I_32 helperCConvertDoubleToInteger(jdouble src);


/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given a double number,
*        converts it to a long and returns the
*        long value.
* @param[in] src Double number to be converted to a long.
* @return Resulting long value.
*
*/
I_64 helperCConvertDoubleToLong(jdouble src);


/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given a floating point number,
*        converts it to a double and returns the
*        double value.
* @param[in] src Floating point number to be converted to a double.
* @return Resulting double value.
*
*/
jdouble helperCConvertFloatToDouble(jfloat src);


/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given a floating point number,
*        converts it to an integer and returns the
*        integer value.
* @param[in] src Floating point number to be converted to an integer.
* @return Resulting integer value.
*
*/
I_32 helperCConvertFloatToInteger(jfloat src);

/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given floating point number,
*        converts it to long and returns the long value.
* @param[in] a Floating point number to be converted to long.
* @return Resulting long value.
*
*/
I_64 helperCConvertFloatToLong(jfloat src);



/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given a floating point number,
*        converts it to a double and returns the
*        double value.
* @param[in] src Floating point number to be converted to a double.
* @return Resulting double value.
*
*/
jdouble helperCConvertIntegerToDouble(I_32 src);


/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given an integer number,
*        converts it to floating point and returns the
*        floating point value.
* @param[in] src Integer number to be converted to floating point.
* @return Resulting floating point value.
*
*/
jfloat helperCConvertIntegerToFloat(I_32 src);


/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given a floating point number,
*        converts it to a double and returns the
*        double value.
* @param[in] src Floating point number to be converted to a double.
* @return Resulting double value.
*
*/
jdouble helperCConvertLongToDouble(I_64 src);


/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given a long number,
*        converts it to floating point and returns the
*        floating point value.
* @param[in] src The long number to be converted to floating point.
* @return Resulting floating point value.
*
*/
jfloat helperCConvertLongToFloat(I_64 src);

#endif /* J9VM_INTERP_FLOAT_SUPPORT */ /* End File Level Build Flags */


/* ---------------- fltdmath.c ---------------- */

/**
* @brief
* @param u
* @param v
* @param *result
* @return void
*/
void addDD( double u, double v, double *result);


/**
* @brief
* @param f1
* @param f2
* @param *rp
* @return void
*/
void addDF(float f1, float f2, float *rp);


/**
* @brief
* @param d1
* @param d2
* @return int
*/
int
compareDD(double d1, double d2);


/**
* @brief
* @param f1
* @param f2
* @return int
*/
int compareDF(float f1, float f2);


/**
* @brief
* @param d
* @param *fp
* @return int
*/
int convertDoubleToFloat(double d, float *fp);


/**
* @brief
* @param f
* @param *dp
* @return void
*/
void convertFloatToDouble(float f, double *dp);


/**
* @brief
* @param d1
* @param d2
* @param *result
* @return void
*/
void divideDD( double d1, double d2, double *result );


/**
* @brief
* @param f1
* @param f2
* @param *rp
* @return void
*/
void divideDF(float f1, float f2, float *rp);


/**
* @brief
* @param m1
* @param m2
* @param *result
* @return void
*/
void multiplyDD( double m1, double m2, double *result );


/**
* @brief
* @param f1
* @param f2
* @param *rp
* @return void
*/
void multiplyDF(float f1, float f2, float *rp);


/**
* @brief
* @param d1
* @param d2
* @param *rp
* @return void
*/
void remDD(double d1, double d2, double *rp);


/**
* @brief
* @param f1
* @param f2
* @param *rp
* @return void
*/
void remDF(float f1, float f2, float *rp);


/**
* @brief
* @param u
* @param v
* @param *result
* @return void
*/
void subDD(double u, double v, double *result);


/**
* @brief
* @param f1
* @param f2
* @param *rp
* @return void
*/
void subDF(float f1, float f2, float *rp);


/* ---------------- fltmath.c ---------------- */

#if (defined(J9VM_INTERP_FLOAT_SUPPORT))  /* File Level Build Flags */

/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Given 2 pointers to double values,
*        returns  flag indicating how the values compare.
* @param[in] *a Pointer to double value.
* @param[in] *b Pointer to double value.
* @return Integer flag: 0 if a=b
*                       1 if a>b
*                      -1 if a<b
* 					   -2 if a or b NaN.
*
*/
int helperDoubleCompareDouble(jdouble *a, jdouble *b);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Calculates the quotient of
*        2 double precision numbers.
* @param[in] *a Pointer to double dividend.
* @param[in] *b Pointer to double divisor.
* @param[out] *c Pointer to double quotient.
* @return I_32 0.
*/
I_32 helperDoubleDivideDouble(jdouble *a, jdouble *b, jdouble *c);


/**
* @brief Helper function called by VM interpreter, using pointers to
*        values. Calculates difference of 2 double numbers.
* @param[in] *a Pointer to double minuend.
* @param[in] *b Pointer to double subtrahend.
* @param[out] *c Pointer to double difference.
* @return I_32 0.
*
*/
I_32 helperDoubleMinusDouble(jdouble *a, jdouble *b, jdouble *c);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Calculates the product of
*        2 double precision numbers.
* @param[in] *a Pointer to double multiplicand.
* @param[in] *b Pointer to double multiplier.
* @param[out] *c Pointer to double product.
* @return I_32 0.
*/
I_32 helperDoubleMultiplyDouble(jdouble *a, jdouble *b, jdouble *c);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Calculates the sum of
*        2 double precision numbers.
* @param[in] *a Pointer to double addend.
* @param[in] *b Pointer to double addend.
* @param[out] *c Pointer to double sum.
* @return I_32 0.
*
*/
I_32 helperDoublePlusDouble(jdouble *a, jdouble *b, jdouble *c);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Given 2 pointers to floating
*        point values, returns  flag indicating how the
*        values compare.
* @param[in] *a Pointer to floating point value.
* @param[in] *b Pointer to floating point value.
* @return Integer flag: 0 if a=b
*                       1 if a>b
*                      -1 if a<b
* 					   -2 if a or b NaN.
*
*/
I_32 helperFloatCompareFloat(jfloat *a, jfloat *b);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Calculates the quotient of
*        2 floating point numbers.
* @param[in] *a Pointer to floating point dividend.
* @param[in] *b Pointer to floating point divisor.
* @param[out] *c Pointer to floating point quotient.
* @return I_32 0.
*/
I_32 helperFloatDivideFloat(jfloat *a, jfloat *b, jfloat *c);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Calculates the difference of
*        2 floating point numbers.
* @param[in] *a Pointer to floating point minuend.
* @param[in] *b Pointer to floating point subtrahend.
* @param[out] *c Pointer to floating point difference.
* @return I_32 0.
*
*/
I_32 helperFloatMinusFloat(jfloat *a, jfloat *b, jfloat *c);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Calculates the product of
*        2 floating point numbers.
* @param[in] *a Pointer to floating point multiplicand.
* @param[in] *b Pointer to floating point multiplier.
* @param[out] *c Pointer to floating point product.
* @return I_32 0.
*/
I_32 helperFloatMultiplyFloat(jfloat *a, jfloat *b, jfloat *c);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Calculates the sum of
*        2 floating point numbers.
* @param[in] *a Pointer to floating point addend.
* @param[in] *b Pointer to floating point addend.
* @param[out] *c Pointer to floating point sum.
* @return I_32 0.
*
*/
I_32 helperFloatPlusFloat(jfloat *a, jfloat *b, jfloat *c);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Negates a double precision number.
* @param[in] *a Pointer to double value.
* @param[in] *b Pointer to double value.
* @return I_32 0.
*
*/
I_32 helperNegateDouble(jdouble *a, jdouble *b);


/**
* @brief Helper function called by VM interpreter, using
*        pointers to values. Negates a floating point number.
* @param[in] *a Pointer to floating point value.
* @param[in] *b Pointer to floating point negative value.
* @return I_32 0.
*
*/
I_32 helperNegateFloat(jfloat *a, jfloat *b);


/* ---------------- fltmath.c C-helpers---------------- */


/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given 2 double values,
*        returns a flag indicating how the values compare.
* @param[in] a Double value.
* @param[in] b Double value.
* @return Integer flag: 0 if a=b
*                       1 if a>b
*                      -1 if a<b
* 					   -2 if a or b NaN.
*
*/
I_32 helperCDoubleCompareDouble(jdouble a, jdouble b);


/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given 2 double values,
*        returns quotient.
* @param[in] a Double dividend.
* @param[in] b Double divisor.
* @return Resulting double quotient.
*
*/
jdouble helperCDoubleDivideDouble(jdouble a, jdouble b);
/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given 2 double values,
*        returns difference.
* @param[in] a Double minuend.
* @param[in] b Double subtrahend.
* @return Resulting double difference.
*
*/
jdouble helperCDoubleMinusDouble(jdouble a, jdouble b);


/**
* @brief Helper function used by JIT. Given 2 floating point
*        values, returns product.
* @param[in] a Double multiplicand.
* @param[in] b Double multiplier.
* @return Resulting double product.
*
*/
jdouble helperCDoubleMultiplyDouble(jdouble a, jdouble b);


/**
* @brief Helper function used by JIT. Given 2 double
*        values, returns sum.
* @param[in] a Double addend.
* @param[in] b Double addend.
* @return Resulting double sum.
*
*/
jdouble helperCDoublePlusDouble(jdouble a, jdouble b);


/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given 2 double values,
*        returns  flag indicating how the values compare.
* @param[in] a Double value.
* @param[in] b Double value.
* @return Integer flag: 0 if a=b
*                       1 if a>b
*                      -1 if a<b
* 					   -2 if a or b NaN.
*
*/
I_32 helperCFloatCompareFloat(jfloat a, jfloat b);

/**
* @brief Helper function called by JIT, using standard
*        C-calling convention. Given 2 floating point
*        values, returns quotient.
* @param[in] a Floating point dividend.
* @param[in] b Floating point divisor.
* @return Resulting floating point quotient.
*
*/
jfloat helperCFloatDivideFloat(jfloat a, jfloat b);


/**
* @brief Helper function used by JIT. Given 2 floating point
*        values, returns difference.
* @param[in] a Floating point minuend.
* @param[in] b Floating point subtrahend.
* @return Resulting floating point difference.
*
*/
jfloat helperCFloatMinusFloat(jfloat a, jfloat b);


/**
* @brief Helper function used by JIT. Given 2 floating point
*        values, returns product.
* @param[in] a Floating point multiplicand.
* @param[in] b Floating point multiplier.
* @return Resulting floating point product.
*
*/
jfloat helperCFloatMultiplyFloat(jfloat a, jfloat b);


/**
* @brief Helper function used by JIT. Given 2 floating point
*        values, returns sum.
* @param[in] a Floating point addend.
* @param[in] b Floating point addend.
* @return Resulting floating point addition.
*
*/
jfloat helperCFloatPlusFloat(jfloat a, jfloat b);


#endif /* J9VM_INTERP_FLOAT_SUPPORT */ /* End File Level Build Flags */


/* ---------------- fltodd.c ---------------- */

/**
* @brief
* @param d
* @return int
*/
int isDoubleOdd(double d);


/* ---------------- fltrem.c ---------------- */

#if (defined(J9VM_INTERP_FLOAT_SUPPORT))  /* File Level Build Flags */

/**
* @brief
* @param a
* @param b
* @param c
* @return I_32
*/
I_32 helperDoubleRemainderDouble(jdouble * a, jdouble * b, jdouble * c);


/**
* @brief
* @param a
* @param b
* @param c
* @return I_32
*/
I_32 helperFloatRemainderFloat(jfloat * a, jfloat * b, jfloat * c);


/* ---------------- fltrem.c C-helpers---------------- */

/**
* @brief Helper function used by JIT. Given 2 double
*        values, returns remainder.
* @param[in] a Double dividend.
* @param[in] b Double divisor.
* @return Resulting double remainder.
*
*/
jdouble helperCDoubleRemainderDouble(jdouble a, jdouble b);


/**
* @brief Helper function used by JIT. Given 2 floating
*        point values, returns remainder.
* @param[in] a Floating point dividend.
* @param[in] b Floating point  divisor.
* @return Resulting floating point  remainder.
*
*/
jfloat helperCFloatRemainderFloat(jfloat a, jfloat b);


#endif /* J9VM_INTERP_FLOAT_SUPPORT */ /* End File Level Build Flags */

/* ---------------- j9crc32.c ---------------- */

/**
* @brief
* @param crc
* @param *bytes
* @param len
* @return U_32
*/
U_32 j9crc32(U_32 crc, U_8 *bytes, U_32 len);

/**
* @brief
* @param crc
* @param *bytes
* @param len
* @param step
* @return U_32
*/
U_32 j9crcSparse32(U_32 crc, U_8 *bytes, U_32 len, U_32 step);


/* ---------------- j9fptr.c ---------------- */

/**
* @brief
* @param *fp
* @return void *
*/
void *helperCompatibleFunctionPointer(void *fp);


/* ---------------- jitlook.c ---------------- */

#if (defined(J9VM_INTERP_NATIVE_SUPPORT))  /* File Level Build Flags */

/**
* @brief
* @param *table
* @param searchValue
* @return J9JITExceptionTable*
*/
J9JITExceptionTable* hash_jit_artifact_search(J9JITHashTable *table, UDATA searchValue);


/**
* @brief
* @param *tree
* @param searchValue
* @return J9JITExceptionTable*
*/
J9JITExceptionTable* jit_artifact_search(J9AVLTree *tree, UDATA searchValue);


#endif /* J9VM_INTERP_NATIVE_SUPPORT */ /* End File Level Build Flags */


/* ---------------- jitresolveframe.c ---------------- */

#if (defined(J9VM_INTERP_NATIVE_SUPPORT))
/**
* @brief
* @param vmThread
* @param sp
* @param pc
* @return J9SFJITResolveFrame*
*/
J9SFJITResolveFrame*
jitPushResolveFrame(J9VMThread* vmThread, UDATA* sp, U_8* pc);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


/* ---------------- jlm.c ---------------- */

/**
* @brief
* @param vmThread
* @return jint
*/
jint
JlmStart(J9VMThread* vmThread);


/**
* @brief
* @param void
* @return jint
*/
jint
JlmStartTimeStamps(void);


/**
* @brief
* @param void
* @return jint
*/
jint
JlmStop(void);


/**
* @brief
* @param void
* @return jint
*/
jint
JlmStopTimeStamps(void);


/**
* @brief
* @param *env
* @param *jlmd
* @param  dump_format
* @return jint
*/
jint
request_MonitorJlmDump(jvmtiEnv* env, J9VMJlmDump *jlmd, jint dump_format);


/**
* @brief
* @param *jvm
* @param *dump_size
* @param  dump_format
* @return jint
*/
jint request_MonitorJlmDumpSize(J9JavaVM *jvm, UDATA *dump_size, jint dump_format);


/* ---------------- moninfo.c ---------------- */

/**
* @brief
* @param vm 	the Java VM
* @param object
* @param pcount
* @return J9VMThread*
*/
J9VMThread*
getObjectMonitorOwner(J9JavaVM* vm, j9object_t object, UDATA* pcount);


/**
* @brief
* @param J9VMThread
* @param object
* @return UdATA
*/
UDATA
isObjectStackAllocated(J9VMThread *targetThread, j9object_t aObj);

#if defined (J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
/* ---------------- offload.c ---------------- */

/**
* @brief
* @param *javaVM the J9JavaVM.
* @param *libraryName a null terminated string containing the library name.
* @param *libraryHandle the handle of a JNI library providing the libraryName.
* @param isStatic indicates if the library is a static JNI library.
* @return UDATA the J9_NATIVE_LIBRARY_SWITCH bits.
*/
UDATA
validateLibrary(struct J9JavaVM *javaVM, const char *libraryName, UDATA libraryHandle, jboolean isStatic);
#endif

/* ---------------- optinfo.c ---------------- */

/**
 * Retrieves number of permitted subclasses in this sealed class. Assumes that
 * ROM class parameter references a sealed class.
 *
 * @param J9ROMClass sealed class
 * @return U_32 number of permitted subclasses in optionalinfo
 */
U_32*
getNumberOfPermittedSubclassesPtr(J9ROMClass *romClass);

/**
 * Find the permitted subclass name constant pool entry at index in the optional data of the ROM class parameter.
 * This method assumes there is at least one permitted subclass in the ROM class.
 *
 * @param U_32* permittedSubclassesCountPtr
 * @param U_32 class index
 * @return the permitted subclass name at index from ROM class
 */
J9UTF8*
permittedSubclassesNameAtIndex(U_32* permittedSubclassesCountPtr, U_32 index);

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
/**
 * Retrieves number of interfaces injected into a class.
 * @param J9ROMClass class with injected interfaces
 * @return U_32 number of injected interfaces in optionalinfo
 */
U_32
getNumberOfInjectedInterfaces(J9ROMClass *romClass);

/**
 * Retrieves number of loadable descriptors in this class. This method
 * assumes that the ROM class parameter references a class with a
 * LoadableDescriptors attribute.
 *
 * @param J9ROMClass class
 * @return U_32 * the first U_32 is the number of classes, followed by that
 * number of constant pool indices
 */
U_32 *
getLoadableDescriptorsInfoPtr(J9ROMClass *romClass);

/**
 * Find the loadable descriptor constant pool entry at index in the optional
 * data of the ROM class parameter. This method assumes there is at least one
 * loadable descriptor in the ROM class.
 *
 * @param U_32 * the pointer returned by getLoadableDescriptorsInfoPtr
 * @param U_32 class index
 * @return the loadable descriptor at index from ROM class
 */
J9UTF8*
loadableDescriptorAtIndex(U_32 *permittedSubclassesCountPtr, U_32 index);
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
/**
 * Retrieves flags stored in ImplicitCreation attribute. This method assumes
 * there is an ImplicitCreation attribute in the ROM class (J9_ROMCLASS_OPTINFO_IMPLICITCREATION_ATTRIBUTE
 * is set in romClass->optionalFlags).
 *
 * @param J9ROMClass class
 * @return ImplicitCreation flags
 */
U_16
getImplicitCreationFlags(J9ROMClass *romClass);
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

/**
 * Retrieves number of record components in this record. Assumes that
 * ROM class parameter references a record class.
 *
 * @param J9ROMClass record class
 * @return U_32 number of record components in optionalinfo
 */
U_32
getNumberOfRecordComponents(J9ROMClass *romClass);

/**
 * Reason if record component has an optional signature attribute.
 *
 * @param J9ROMRecordComponentShape* record component
 * @return true if record component has an optional signature attribute
 */
BOOLEAN
recordComponentHasSignature(J9ROMRecordComponentShape* recordComponent);

/**
 * Reason if record component has an optional annotations attribute.
 *
 * @param J9ROMRecordComponentShape* record component
 * @return true if record component has an optional annotations attribute
 */
BOOLEAN
recordComponentHasAnnotations(J9ROMRecordComponentShape* recordComponent);

/**
 * Reason if record component has an optional type annotations attribute.
 *
 * @param J9ROMRecordComponentShape* record component
 * @return true if record component has an optional type annotations attribute
 */
BOOLEAN
recordComponentHasTypeAnnotations(J9ROMRecordComponentShape* recordComponent);

/**
 * Return the generic signature attribute from record component parameter.
 *
 * @param J9ROMRecordComponentShape* record component
 * @return J9UTF8* generic signature attribute, or null if one does
 * not exist for this record component.
 *
 */
J9UTF8*
getRecordComponentGenericSignature(J9ROMRecordComponentShape* recordComponent);

/**
 * Return the annotation attribute data from the record component parameter.
 *
 * @param J9ROMRecordComponentShape* record component
 * @return U_32* annotation attribute data, or null is it does not exist
 * for this record component.
 */
U_32*
getRecordComponentAnnotationData(J9ROMRecordComponentShape* recordComponent);

/**
 * Return the type annotation attribute data from the record component parameter.
 *
 * @param J9ROMRecordComponentShape* record component
 * @return U_32* type annotation attribute data, or null is it does not exist
 * for this record component.
 */
U_32*
getRecordComponentTypeAnnotationData(J9ROMRecordComponentShape* recordComponent);

/**
 * Find first record component in the optional data of the ROM class parameter.
 * This method assumes there is at least one record component in the ROM class.
 *
 * @param J9ROMClass* record class
 * @return first record component from ROM class
 */
J9ROMRecordComponentShape*
recordComponentStartDo(J9ROMClass *romClass);

/**
 * Find the record component. This method assumes there is
 * at least one more record component.
 *
 * @param J9ROMRecordComponentShape* last record component
 * @return J9ROMRecordComponentShape* next record component
 */
J9ROMRecordComponentShape*
recordComponentNextDo(J9ROMRecordComponentShape* recordComponent);

/**
* @brief
* @param *vm
* @param *romMethod
* @param *romClass
* @param *classLoader
* @param relativePC
* @param *romClass
* @return UDATA
*/
UDATA
getLineNumberForROMClassFromROMMethod(J9JavaVM *vm, J9ROMMethod *romMethod, J9ROMClass *romClass, J9ClassLoader *classLoader, UDATA relativePC);

/**
* @brief
* @param *romMethod
* @return U_32 *
*/
U_32 *
getStackMapInfoForROMMethod(J9ROMMethod *romMethod);



/**
* @brief
* @param *romClass
* @return U_32 *
*/
U_32 *
getClassAnnotationsDataForROMClass(J9ROMClass *romClass);

/**
* Get the type annotations attribute for  class
* @param *romClass
* @return pointer to the 4 length bytes preceding the actual type annotation attribute
*/
U_32 *
getClassTypeAnnotationsDataForROMClass(J9ROMClass *romClass);

/**
* @brief
* @param *vm
* @param *classLoader
* @param *romClass
* @return J9EnclosingObject *
*/
J9EnclosingObject *
getEnclosingMethodForROMClass(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMClass *romClass);


/**
* @brief
* @param *vm
* @param *classLoader
* @param *romClass
* @return J9UTF8 *
*/
J9UTF8 *
getGenericSignatureForROMClass(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMClass *romClass);


/**
* @brief
* @param *vm
* @param *method
* @param relativePC
* @return UDATA
*/
UDATA
getLineNumberForROMClass(J9JavaVM *vm, J9Method *method, UDATA relativePC);


/**
 * @brief encode one array of line numbers, will write in the buffer starting at position *buffer for
 * a length of 1 to 5 bytes per line number depending on the size of pcOffset and lineNumberOffset. The buffer
 * pointer will be moved to be after the last byte written.
 * Fail if the startPC of the J9CfrLineNumberTableEntry are not ordered in the lineNumberTableEntryArray.
 * @param J9CfrLineNumberTableEntry * lineNumberTableEntryArray
 * 		array of J9CfrLineNumberTableEntry to be compressed
 * @param U_16 lineNumberTableEntryCount
 * 		size of lineNumberTableEntry
 * @param J9CfrLineNumberTableEntry * lastLineNumberTableEntry
 * 		previous lineNumberTableEntry when doing incremental compression.
 * 		Can be NULL when there is no previous lineNumberTableEntry
 * @return BOOLEAN TRUE for success and FALSE for failure
*/
BOOLEAN
compressLineNumbers(J9CfrLineNumberTableEntry * lineNumberTableEntryArray, U_16 lineNumberTableEntryCount, J9CfrLineNumberTableEntry * lastLineNumberTableEntry, U_8 ** buffer);

/**
* @brief decode one line number
* Fail if the format of one byte can not be recognized, it would happen
* when trying to decode a byte starting by 1111 for example.
* @param **currentLineNumber pointer to a line number got from
* 	getMethodDebugInfoFromROMMethod
* @param [in|out] *lineNumber Pointer to an object of J9LineNumber, the elements
* 	lineNumber and location must be initialized to 0, because the function
* 	will increment the offset given by lineNumber.
* @return BOOLEAN TRUE for success and FALSE for failure
*/
BOOLEAN
getNextLineNumberFromTable(U_8 **currentLineNumber, J9LineNumber *lineNumber);

/**
* @brief compress one local variable table entry
* @param I_32 deltaIndex
* @param I_32 deltaStartPC
* @param I_32 deltaLength
* @param U_8 *buffer Buffer where to write the compressed data, must be of a size of at least 13 U_8
* @return UDATA the size of the compressed data between 1 and 13
 */
UDATA
compressLocalVariableTableEntry(I_32 deltaIndex, I_32 deltaStartPC, I_32 deltaLength, U_8 * buffer);

/**
* @brief
* @param *methodInfo
* @return J9LineNumber *
*/
U_8 *
getLineNumberTable(J9MethodDebugInfo *methodInfo);

/**
* @brief
* @param *vm
* @param *method
* @return J9MethodDebugInfo *
*/
J9MethodDebugInfo *
getMethodDebugInfoForROMClass(J9JavaVM *vm, J9Method *method);


/**
* @brief
* @param *vm
* @param *classLoader
* @param *romClass
* @return J9UTF8 *
*/
J9UTF8 *
getSimpleNameForROMClass(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMClass *romClass);


/**
* @brief
* @param *vm
* @param *classLoader
* @param *romClass
* @return J9SourceDebugExtension *
*/
J9SourceDebugExtension *
getSourceDebugExtensionForROMClass(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMClass *romClass);


/**
* @brief
* @param *vm
* @param *classLoader
* @param *romClass
* @return J9UTF8 *
*/
J9UTF8 *
getSourceFileNameForROMClass(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMClass *romClass);

/**
* @brief return the line number count for a specific method
* @param J9MethodDebugInfo *methodInfo
* @return U_32
*/
U_32
getLineNumberCount(J9MethodDebugInfo *methodInfo);

/**
* @brief return the line number info compressed size for a specific method
* @param J9MethodDebugInfo *methodInfo
* @return U_32
*/
U_32
getLineNumberCompressedSize(J9MethodDebugInfo *methodInfo);

/**
* @brief
* @param *methodInfo
* @return U_8 *
*/
U_8 *
getVariableTableForMethodDebugInfo(J9MethodDebugInfo *methodInfo);

/**
* @brief
* @param *methodInfo
* @return U_32
*/
U_32
getMethodDebugInfoStructureSize(J9MethodDebugInfo *methodInfo);

/**
* @brief
* @param *vm
* @param *romClass
* @return void
*/
void
releaseOptInfoBuffer(J9JavaVM *vm, J9ROMClass *romClass);

/**
* @brief
* @param *state
* @return J9VariableInfoValues *
*/
J9VariableInfoValues *
variableInfoNextDo(J9VariableInfoWalkState *state);

/**
* @brief
* @param methodInfo
* @param state
* @return J9VariableInfoValues *
*/
J9VariableInfoValues *
variableInfoStartDo(J9MethodDebugInfo * methodInfo, J9VariableInfoWalkState* state);



/* ---------------- rcdump.c ---------------- */

/**
* @brief
* @param *romClass
* @param *portLib
* @param *translationBuffers
* @param flags
* @return IDATA
*/
IDATA j9bcutil_dumpRomClass( J9ROMClass *romClass, J9PortLibrary *portLib, J9TranslationBufferSet *translationBuffers, U_32 flags);


/**
* @brief
* @param *romMethod
* @param *romClass
* @param *portLib
* @param flags
* @param methodIndex
* @return I_32
*/
I_32 j9bcutil_dumpRomMethod( J9ROMMethod *romMethod, J9ROMClass *romClass, J9PortLibrary *portLib, U_32 flags, U_32 methodIndex);


/* ---------------- romclasswalk.c ---------------- */

/**
* @brief
* @param romClass
* @param *slotCallback called on every slot (optional)
* @param *sectionCallback called on every section (optional)
* @param *validateRangeCallback called to validate range of traversable memory
* @param userData
* @return void
*/
void allSlotsInROMClassDo(J9ROMClass* romClass,
		void(*slotCallback)(J9ROMClass*, U_32, void*, const char*, void*),
		void(*sectionCallback)(J9ROMClass*, void*, UDATA, const char*, void*),
		BOOLEAN(*validateRangeCallback)(J9ROMClass*, void*, UDATA, void*),
		void* userData);


/* ---------------- romhelp.c ---------------- */

/**
 * Returns the original ROM method.
 * If the original ROM method is NULL, fire an assertion.
 *
 * @param method The method
 * @return J9ROMMethod *
 */
J9ROMMethod *
getOriginalROMMethod(J9Method * method);


/**
 * Returns the original ROM method.
 * If no method can be determined, return NULL.
 *
 * @param method The method
 * @return J9ROMMethod *
 */
J9ROMMethod *
getOriginalROMMethodUnchecked(J9Method * method);

#if JAVA_SPEC_VERSION >= 20
/**
 * Return class file version (minorVersion << 16 + majorVersion) in an int.
 *
 * @param[in] currentThread the current thread.
 * @param[in] cls the class
 *
 * @return	the class file version
 */
U_32
getClassFileVersion(J9VMThread *currentThread, J9Class *cls);
#endif /* JAVA_SPEC_VERSION >= 20 */
/* ---------------- subclass.c ---------------- */

/**
* @brief
* @param subclassState
* @return J9Class *
*/
J9Class *
allSubclassesNextDo(J9SubclassWalkState * subclassState);


/**
* @brief
* @param rootClass
* @param subclassState
* @param includeRootClass
* @return J9Class *
*/
J9Class *
allSubclassesStartDo(J9Class * rootClass, J9SubclassWalkState * subclassState, UDATA includeRootClass);


/* ---------------- superclass.c ---------------- */

/**
* @brief
* @param superClass
* @param baseClass
* @return UDATA
*/
UDATA
isSameOrSuperClassOf(J9Class * superClass, J9Class * baseClass);

/**
 * @brief Determine if baseInterface extends superInterface
 * @param superInterface The interface to check whether baseInterface extends it
 * @param baseInterface The interface to start the check from
 * @return BOOLEAN true if baseInterface is compatible with superInterface,  false otherwise.
 */
BOOLEAN
isSameOrSuperInterfaceOf(J9Class *superInterface, J9Class *baseInterface);

/* ---------------- thrhelp.c ---------------- */

/**
 * @brief If possible, get the J9VMThread from the OMR_VMThread stored in the omrthread_t tls.
 * @param vm
 * @param omrthread
 * @return J9VMThread *
 */
J9VMThread *
getVMThreadFromOMRThread(J9JavaVM *vm, omrthread_t omrthread);

/**
 * @brief Initialize the currentOSStackFree field in the J9VMThread. Must be called by
 *	running thread, not thread creating the new vmThread.
 * @param currentThread vmThread token
 * @param osThread omrThread token
 * @param osStackSize requested stack size, determined by -Xmso or J9_OS_STACK_SIZE
 */
void
initializeCurrentOSStackFree(J9VMThread *currentThread, omrthread_t osThread, UDATA osStackSize);

/* ---------------- thrinfo.c ---------------- */

/**
* @brief
* @param targetThread
* @param pLockObject
* @param pLockOwner
* @param pCount
* @return UDATA
*/
UDATA
getVMThreadObjectState(J9VMThread *targetThread, j9object_t *pLockObject, J9VMThread **pLockOwner, UDATA *pCount);

/**
* @brief
* @param targetThread
* @param pLockObject
* @param pLockOwner
* @param pCount
* @return UDATA
*/
UDATA
getVMThreadObjectStatesAll(J9VMThread *targetThread, j9object_t *pLockObject, J9VMThread **pLockOwner, UDATA *pCount);


/**
* @brief
* @param targetThread
* @param pLockObject
* @param pRawLock
* @param pLockOwner
* @param pCount
* @return UDATA
*/
UDATA
getVMThreadRawState(J9VMThread *targetThread, j9object_t *pLockObject, omrthread_monitor_t *pRawLock, J9VMThread **pLockOwner, UDATA *pCount);

/**
* @brief
* @param targetThread
* @param pLockObject
* @param pRawLock
* @param pLockOwner
* @param pCount
* @return UDATA
*/
UDATA
getVMThreadRawStatesAll(J9VMThread *targetThread, j9object_t *pLockObject, omrthread_monitor_t *pRawLock, J9VMThread **pLockOwner, UDATA *pCount);

/**
* @brief
* @param vm
* @param object
* @param lockWord
* @return J9ThreadAbstractMonitor *
*/
J9ThreadAbstractMonitor *
getInflatedObjectMonitor(J9JavaVM *vm, j9object_t object, j9objectmonitor_t lockWord);

/* ---------------- thrname.c ---------------- */
/**
* @brief
* @param currentThread
* @param vmThread
* @param name
* @param nameIsStatic
* @return void
*/
void
setVMThreadNameWithFlag(J9VMThread *currentThread, J9VMThread *vmThread, char *name, U_8 nameIsStatic);

/**
* @brief
* @param currentThread
* @param vmThread
* @param nameObject
* @return IDATA
*/
IDATA
setVMThreadNameFromString(J9VMThread *currentThread, J9VMThread *vmThread, j9object_t nameObject);

/**
* @brief
* @param javaVM
* @param nameObject
* @return char*
*/
char*
getVMThreadNameFromString(J9VMThread *vmThread, j9object_t nameObject);


/* ---------------- utf8hash.c ---------------- */

/**
 * @brief
 * @param data points to raw UTF8 bytes
 * @param length is the number of bytes
 * @return UDATA
 */
UDATA
computeHashForUTF8(const U_8 * data, UDATA length);


/* ---------------- wildcard.c ---------------- */

/**
* @brief
* @param pattern
* @param patternLength
* @param needle
* @param needleLength
* @param matchFlag
* @return IDATA
*/
IDATA
parseWildcard(const char * pattern, UDATA patternLength, const char** needle, UDATA* needleLength, U_32 * matchFlag);


/**
* @brief
* @param matchFlag
* @param needle
* @param needleLength
* @param haystack
* @param haystackLength
* @return IDATA
*/
IDATA
wildcardMatch(U_32 matchFlag, const char* needle, UDATA needleLength, const char* haystack, UDATA haystackLength);

/* ---------------- pkgname.c ---------------- */

/**
 * Fetch the actual package name from a package ID.
 * A pointer to the first byte of the UTF8 encoded name is returned,
 * and the length (also in bytes) is returned in the nameLength param.
 * The length does not include the final slash.
 * For instance, the packageNameLength of "java/lang/Object" is 9.
 * The packageNameLength of "Foo" is 0.
 * @param[in] key - the package ID
 * @param[out] packageNameLength - the length of the name in bytes
 * @return const U_8* - the package name.
 */
const U_8*
getPackageName(J9PackageIDTableEntry* key, UDATA* packageNameLength);

/**
 * Return the length of the package name part of the class name in bytes.
 * The final slash is not counted.
 * For instance, the packageNameLength of "java/lang/Object" is 9.
 * The packageNameLength of "Foo" is 0.
 * @param[in] romClass - the class to calculate the packageNameLength for
 * @return UDATA the length in bytes
 */
UDATA
packageNameLength(J9ROMClass* romClass);


/* ---------------- sendslot.c ---------------- */

/**
 * Calculate the number of slots occupied by the arguments to a method
 * with the given signature (not counting the receiver).
 *
 * For instance,
 *  - "()V" has 0 send slots,
 *  - "(I)V" has 1 send slots,
 *  - "(D)V" has 2 send slots,
 *  - "(Ljava/lang/String;IZ[[DD)Ljava/lang/StringBuffer;" has 6 send slots
 *
 * @param[in] signature - the method signature to process
 * @return UDATA - the number of slots used by the arguments
 */
UDATA
getSendSlotsFromSignature(const U_8* signature);

/* ---------------- returntype.c ---------------- */

/**
 * Determine the return type given a method signature.
 * Returns the character that is the return type for a method and
 * stores the address that the return type starts on into *outData.
 * If the signature is malformed (i.e. doesn't contain a closing ')'
 * character), return 0.
 *
 * @param[in] inData - the method signature to process
 * @param[in] inLength - the length of the signature, in bytes
 * @param[out] outData - a returned pointer to beginning of the return type
 * @return U_16 - the first character of the return type, or 0 on error
 */
U_16
getReturnTypeFromSignature(U_8 * inData, UDATA inLength, U_8 **outData);

/* ---------------- mthutil.c ---------------- */

/**
 * @brief Retrieve the J9Method which maps to the index within the iTable for interfaceClass.
 * @param interfaceClass The interface class to query
 * @param index The iTable index
 * @return J9Method* The J9Method which maps to index within interfaceClass
  */
J9Method *
iTableMethodAtIndex(J9Class *interfaceClass, UDATA index);

/**
 * @brief Retrieve the iTable index of an interface method within the iTable for
 *        its declaring class.
 * @param method The interface method
 * @return UDATA The iTable index (not including the fixed J9ITable header)
 */
UDATA
getITableIndexWithinDeclaringClass(J9Method *method);

/**
 * @brief Retrieve the index of an interface method within the iTable for an interface
 *        (not necessarily the same interface, as iTables contain methods from all
 *        extended interfaces as well as the local one).
 * @param method The interface method
 * @param targetInterface The interface in whose table to search
 *                        (NULL to use the declaring class of method)
 * @return UDATA The iTable index (not including the fixed J9ITable header)
 */
UDATA
getITableIndexForMethod(J9Method * method, J9Class *targetInterface);

/**
 * Returns the first ROM method following the argument.
 * If this is called on the last ROM method in a ROM class
 * it will return an undefined value.
 * The defining class of method must be an interface; do not use this
 * for methods inherited from java.lang.Object.
 *
 * @param[in] romMethod - the current ROM method
 * @return - the ROM method following the current one
 */
J9ROMMethod*
nextROMMethod(J9ROMMethod * romMethod);

/**
 * Returns the index of the specified method.
 * Takes replaced classes into account.
 * If no index can be determined, fire an assertion.
 *
 * @param method The method
 * @return the index of the method (0-based)
 */
UDATA
getMethodIndex(J9Method *method);


/**
 * Returns the index of the specified method.
 * Takes replaced classes into account.
 * If no index can be determined, return UDATA_MAX.
 *
 * @param method The method
 * @return the index of the method (0-based) or UDATA_MAX
 */
UDATA
getMethodIndexUnchecked(J9Method *method);


/* Returns 0 if a and b are have identical fieldName, fieldNameLength, signature, signatureLength.
 * Returns a positive number if a is "greater than" b, and a negative number if a is "less than" b.
 * Greater/Less than is determined first by the name length, then signature length, then memcmp on name
 * and lastly memcmp on signature.
 *
 * @param name of a
 * @param length of name a
 * @param sig of a
 * @param length of sig a
 * @param name of b
 * @param length of name b
 * @param sig of b
 * @param length of sig b
 */
IDATA
compareMethodNameAndSignature(
		U_8 *aNameData, U_16 aNameLength, U_8 *aSigData, U_16 aSigLength,
		U_8 *bNameData, U_16 bNameLength, U_8 *bSigData, U_16 bSigLength);


/* Returns 0 if a and b are have identical fieldName, fieldNameLength, and the signatures match up to the end of the shorter signature
 * Returns a positive number if a is "greater than" b, and a negative number if a is "less than" b.
 * Greater/Less than is determined first by the name length, then signature length, then memcmp on name
 * and lastly memcmp on signature.
 *
 * @param name of a
 * @param length of name a
 * @param sig of a
 * @param length of sig a
 * @param name of b
 * @param length of name b
 * @param sig of b
 * @param length of sig b
 */
IDATA
compareMethodNameAndPartialSignature(
		U_8 *aNameData, U_16 aNameLength, U_8 *aSigData, U_16 aSigLength,
		U_8 *bNameData, U_16 bNameLength, U_8 *bSigData, U_16 bSigLength);


/* ---------------- extendedHCR.c ---------------- */

/**
 * \brief   Check whether jvmti class redefine extensions are allowed
 * \ingroup jvmtiClass
 *
 *
 * @param[in] vm  J9JavaVM structure
 * @return  boolean
 *
 *	Return a boolean specifying whether j9 specific jvmti class redefine extensions
 *	are allowed in the current run configuration.
 */
UDATA
areExtensionsEnabled(J9JavaVM * vm);

#if defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)

/* ---------------- hshelp.c ---------------- */

/**
 * @brief
 * struct used during hot code replacement to track the mapping
 * of old classes to new classes
 */
typedef struct J9JVMTIClassPair {
	J9Class * originalRAMClass;
	UDATA flags;
    J9ROMMethod ** methodRemap;              /*<! Keeps track of which original method remaps to which replacement method. */
    U_32 * methodRemapIndices;
	union {
		J9ROMClass * romClass;
		J9Class * ramClass;
	} replacementClass;
} J9JVMTIClassPair;

/**
 * @brief
 * Struct used in the class loader hotswap hashtable to keep track of
 * replaced classes. We need this data to correctly fix static refs upon
 * redefinition
 */
typedef struct J9HotswappedClassPair {
	J9Class * originalClass;
	J9Class * replacementClass;
} J9HotswappedClassPair;

#define J9JVMTI_CLASS_PAIR_FLAG_PRUNED 1
#define J9JVMTI_CLASS_PAIR_FLAG_REDEFINED 2
#define J9JVMTI_CLASS_PAIR_FLAG_USES_EXTENSIONS 4

/**
 * @brief
 * struct used to map a replaced Classes method address to the method
 * address in the replacing Class.
 */
typedef struct J9JVMTIMethodPair {
	J9Method * oldMethod;
	J9Method * newMethod;
} J9JVMTIMethodPair;

/**
 *  Helper structure used while creating the jit class redefinition event data
 */
typedef struct J9JVMTIHCRJitEventData {
	UDATA * dataCursor;      /*!< cursor into the data buffer */
	UDATA * data;            /*!< data buffer containing the jit class redefinition event data */
	UDATA classCount;        /*!< number of classes in the data buffer */
	UDATA initialized;       /*!< indicates that the structure has been initialized and is ready for use and dealloc */
} J9JVMTIHCRJitEventData;

void
jitEventFree(J9JavaVM * vm, J9JVMTIHCRJitEventData * eventData);

void
fixStaticRefs(J9VMThread * currentThread, J9HashTable * classPairs, UDATA extensionsUsed);

void
hshelpUTRegister(J9JavaVM *vm);

UDATA
areMethodsEquivalent (J9ROMMethod * method1, J9ROMClass * romClass1, J9ROMMethod * method2, J9ROMClass * romClass2);

void
fixSubclassHierarchy (J9VMThread * currentThread, J9HashTable* classHashTable);

void
fixITables(J9VMThread * currentThread, J9HashTable* classHashTable);

void
fixArrayClasses(J9VMThread * currentThread, J9HashTable* classHashTable);

void
fixJNIRefs (J9VMThread * currentThread, J9HashTable* classHashTable, BOOLEAN fastHCR, UDATA extensionsUsed);

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
/**
 * @brief Identify MemberName objects that are (potentially) affected by class
 * redefinition and put them into a state suitable for fix-up.
 *
 * This must be done before java/lang/Class objects are updated to point to
 * replacement RAM classes.
 *
 * vmtarget is temporarily repurposed as the next pointer for an intrusive
 * linked list of all MemberName objects to fix up. This list allows the same
 * MemberNames to be processed again by fixMemberNames() without needing to
 * identify the same MemberNames after java/lang/Class instances and JNI
 * method/field IDs have been updated.
 *
 * For MemberNames representing methods (MN_IS_METHOD, MN_IS_CONSTRUCTOR),
 * vmindex is temporarily set to point to the corresponding J9JNIMethodID,
 * which will be used in fixMemberNames() to complete the fix-up.
 *
 * Note that classHashTable is expected to contain an entry for every affected
 * class, in particular including classes whose vTable layouts or iTables
 * change due to redefinition of a supertype.
 *
 * Once preparation completes, it's necessary to fixMemberNames() even if class
 * redefinition later fails, since vmtarget and vmindex need to be restored to
 * their usual meanings.
 *
 * @param[in] currentThread the J9VMThread of the current thread
 * @param[in] classHashTable the hash table of J9JVMTIClassPairs for redefinition
 * @return the first MemberName in the list, or NULL if there are none
 */
j9object_t
prepareToFixMemberNames(J9VMThread *currentThread, J9HashTable *classPairs);

/**
 * @brief Update MemberNames based on their JNI field/method IDs.
 *
 * memberNamesToFix will be set to NULL so that multiple calls with the same
 * list are idempotent.
 *
 * @param[in] currentThread the J9VMThread of the current thread
 * @param[in,out] memberNamesToFix the list of MemberNames from prepareToFixMemberNames()
 */
void
fixMemberNames(J9VMThread *currentThread, j9object_t *memberNamesToFix);

/**
 * @brief Determine the value of MemberName.vmindex for a method.
 *
 * This is the vTable offset for virtual dispatch (MH_REF_INVOKEVIRTUAL), the
 * iTable index for interface dispatch (MH_REF_INVOKEINTERFACE), and -1 for
 * direct dispatch (MH_REF_INVOKESTATIC, MH_REF_INVOKESPECIAL).
 *
 * clazz can differ from the defining class of the method when doing virtual
 * dispatch of a method inherited from an interface, in which case the defining
 * class is the interface but clazz is the inheriting (non-interface) class.
 *
 * @param[in] clazz the class (that will be) represented by MemberName.clazz
 * @return the value that MemberName.vmindex should take on
 */
jlong
vmindexValueForMethodMemberName(J9JNIMethodID *methodID, J9Class *clazz, jint flags);
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */

void
fixConstantPoolsForFastHCR(J9VMThread *currentThread, J9HashTable *classPairs, J9HashTable *methodPair);

void
unresolveAllClasses (J9VMThread * currentThread, J9HashTable * classPairs, J9HashTable * methodPairs, UDATA extensionsUsed);

void
fixHeapRefs (J9JavaVM * vm, J9HashTable * classPairs);

#if defined(J9VM_OPT_METHOD_HANDLE)
void
fixDirectHandles(J9VMThread * currentThread, J9HashTable * classHashTable, J9HashTable * methodHashTable);
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */

void
copyPreservedValues (J9VMThread * currentThread, J9HashTable* classHashTable, UDATA extensionsUsed);

enum jvmtiError
recreateRAMClasses (J9VMThread * currentThread, J9HashTable* classHashTable, J9HashTable * methodHashTable, UDATA extensionsUsed, BOOLEAN fastHCR);

enum jvmtiError
determineClassesToRecreate (J9VMThread * currentThread, jint class_count, J9JVMTIClassPair * specifiedClasses, J9HashTable ** classPairsPtr,
	J9HashTable ** methodPairs, J9JVMTIHCRJitEventData * jitEventData, BOOLEAN fastHCR);

enum jvmtiError
verifyClassesAreCompatible (J9VMThread * currentThread, jint class_count, J9JVMTIClassPair * classPairs, UDATA extensionsEnabled, UDATA * extensionsUsed);

enum jvmtiError
verifyClassesCanBeReplaced (J9VMThread * currentThread, jint class_count, const struct jvmtiClassDefinition * class_definitions);

enum jvmtiError
reloadROMClasses (J9VMThread * currentThread, jint class_count, const struct jvmtiClassDefinition * class_definitions, J9JVMTIClassPair * classPairs, UDATA options);

enum jvmtiError
verifyNewClasses (J9VMThread * currentThread, jint class_count, J9JVMTIClassPair * classPairs);

jvmtiError
fixMethodEquivalencesAndCallSites(J9VMThread * currentThread,
	J9HashTable * classPairs,
	J9JVMTIHCRJitEventData * eventData,
	BOOLEAN fastHCR, J9HashTable ** methodEquivalences,
	UDATA extensionsUsed);

jboolean
classIsModifiable(J9JavaVM * vm, J9Class * clazz);

void
classesRedefinedEvent(J9VMThread * currentThread, jint classCount, J9HashTable * classHashTable, UDATA * jitEventData,
					  J9Method * (*getMethodEquivalence)(J9VMThread * currentThread, J9Method * method));

void
fixVTables_forExtendedRedefine(J9VMThread * currentThread, J9HashTable * classPairs, J9HashTable * methodPairs);

void
fixVTables_forNormalRedefine(J9VMThread * currentThread, J9HashTable * classPairs, J9HashTable * methodPairs,
							 BOOLEAN fastHCR, J9HashTable ** methodEquivalences);

void
flushClassLoaderReflectCache(J9VMThread * currentThread, J9HashTable * classPairs);

#ifdef J9VM_INTERP_NATIVE_SUPPORT
void
jitClassRedefineEvent(J9VMThread * currentThread, J9JVMTIHCRJitEventData * jitEventData, UDATA extensionsEnabled, UDATA extensionsUsed);
#endif

void
notifyGCOfClassReplacement(J9VMThread * currentThread, J9HashTable * classPairs, UDATA isFastHCR);

#if JAVA_SPEC_VERSION >= 11
void
fixNestMembers(J9VMThread * currentThread, J9HashTable * classPairs);
#endif /* JAVA_SPEC_VERSION >= 11 */

#endif /* J9VM_INTERP_HOT_CODE_REPLACEMENT */

/* ---------------- filecache.c ---------------- */

/**
* @brief write data to a file that was opened using the cached_file_open function.
* @param *portLibrary
* @param fd
* @param *buf
* @param nbytes
* @return IDATA
*/
IDATA
j9cached_file_write(struct J9PortLibrary *portLibrary, IDATA fd, const void *buf, const IDATA nbytes);

/**
* @brief open a file for writing through a caching mechanism.
* @param *portLibrary
* @param *path
* @param flags
* @param mode
* @return IDATA
*/
IDATA
j9cached_file_open(struct J9PortLibrary *portLibrary, const char *path, I_32 flags, I_32 mode);

/**
* @brief close a file that was opened by the cached_file_open function.
* @param *portLibrary
* @param fd
* @return I_32
*/
I_32
j9cached_file_close(struct J9PortLibrary *portLibrary, IDATA fd);

/**
* @brief perform a file seek on a file that was opened using the cached_file_open function.
* @param *portLibrary
* @param fd
* @param offset
* @param whence
* @return I_64
*/
I_64
j9cached_file_seek(struct J9PortLibrary *portLibrary, IDATA fd, I_64 offset, I_32 whence);

/**
* @brief perform a file sync on a file that was opened using the cached_file_open function.
* @param *portLibrary
* @param fd
* @return I_32
*/
I_32
j9cached_file_sync(struct J9PortLibrary *portLibrary, IDATA fd);


/* ---------------- shchelp_j9.c ---------------- */

/**
 * Populate a J9PortShcVersion struct with the version data of the running JVM
 *
 * @param [in] vm  pointer to J9JavaVM structure
 * @param [in] j2seVersion  The j2se version the JVM is running
 * @param [out] result  The struct to populate
 */
void
setCurrentCacheVersion(J9JavaVM *vm, UDATA j2seVersion, J9PortShcVersion* result);

/**
 * Get the running JVM feature
 *
 * @param [in] vm  pointer to J9JavaVM structure.*
 * @return U_32
 */
U_32
getJVMFeature(J9JavaVM *vm);

/**
 * Get the OpenJ9 SHA
 *
 * @return uint64_t The OpenJ9 SHA
 */
uint64_t
getOpenJ9Sha();

#if JAVA_SPEC_VERSION < 21
/**
 * If the class is a lambda class get the pointer to the last '$' sign of the class name which is in the format of HostClassName$$Lambda$<IndexNumber>/0x0000000000000000.
 * NULL otherwise.
 *
 * @param[in] className  pointer to the class name
 * @param[in] classNameLength  length of the class name
 * @return Pointer to the last '$' sign of the class name if it is a lambda class.
 * 		   NULL otherwise.
 */
char*
getLastDollarSignOfLambdaClassName(const char *className, UDATA classNameLength);
#endif /*JAVA_SPEC_VERSION < 21 */

/**
 * Checks if the given class name corresponds to a Lambda class.
 *
 * @param[in] className The class name to check
 * @param[in] classNameLength The length of the class name
 * @param[in,out] deterministicPrefixLength The length of the deterministic class name prefix (optional)
 * @return TRUE if the class name corresponds to a lambda class, otherwise FALSE
 */
BOOLEAN
isLambdaClassName(const char *className, UDATA classNameLength, UDATA *deterministicPrefixLength);

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
/**
 * Checks if the given class name corresponds to a LambdaForm class.
 *
 * @param[in] className The class name to check
 * @param[in] classNameLength The length of the class name
 * @param[in,out] deterministicPrefixLength The length of the deterministic class name prefix (optional)
 * @return TRUE if the class name corresponds to a lambdaForm class, otherwise FALSE
 */
BOOLEAN
isLambdaFormClassName(const char *className, UDATA classNameLength, UDATA *deterministicPrefixLength);
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */

/* ---------------- cphelp.c ---------------- */

/**
* @brief copy the classPathEntry at a specified index from a classLoader
* @param *currentThread
* @param *classLoader
* @param cpIndex
* @param *cpEntry
* @return UDATA
*/
UDATA
getClassPathEntry(J9VMThread * currentThread, J9ClassLoader * classLoader, IDATA cpIndex, J9ClassPathEntry * cpEntry);


/**
 * Returns location from where the class has been loaded.
 * If the Class is loaded from a module in the jimage file, location refers to jrt URL.
 * If the Class is loaded from a patch path or classpath, location refers to the path where the class was found.
 *
 * @param [in] currentThread Current J9VMThread
 * @param [in] clazz pointer to J9Class
 * @param [out] length length of the string returned
 * @return string representing the location from which class has been loaded
 */
U_8 *
getClassLocation(J9VMThread * currentThread, J9Class * clazz, UDATA *length);

/**
 * Composes jrt URL for the give module and adds it to the hashtable with module as the key.
 *
 * @param [in] currentThread Current J9VMThread
 * @param [in] module module for which jrt URL is needed
 *
 * @return pointer to J9UTF8 representing jrt URL for the module. Returns NULL if any error occurs
 */
J9UTF8 *
getModuleJRTURL(J9VMThread *currentThread, J9ClassLoader *classLoader, J9Module *module);

/**
 * Append a single path segment to the bootstrap class loader
 *
 * @param [in] vm Java VM
 * @param [in] filename a jar file to be added
 *
 * @return the number of bootstrap classloader classPathEntries afterwards
 * 			Returning value 0 indicates an error has occurred.
 */
UDATA
addJarToSystemClassLoaderClassPathEntries(J9JavaVM *vm, const char *filename);

/* ---------------- genericSignalHandler.c ---------------- */

/**
* @brief generic signal handler that dumps the registers contents from the time of crash and aborts.
*
* @param *portLibrary	the port library
* @param *gpType		the port library defined signal
* @param *gpInfo		opaque cookie needed by the port library signal handling mechanism
* @param *userData		not used.
* @return UDATA
*/
UDATA genericSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData);


typedef struct J9PropsFile {
	J9PortLibrary* portLibrary;
	J9HashTable* properties;
} J9PropsFile, *j9props_file_t;

/**
 * Iterator for properties files.
 * @param file The file being walked.
 * @param key The current key.
 * @param key The current value.
 * @param userData Opaque data.
 * @return TRUE to keep walking, FALSE to terminate the walk.
 */
typedef BOOLEAN (j9props_file_iterator)(j9props_file_t file, const char* key, const char* value, void* userData);

/**
 * Opens and parses the specified props file.
 * @param portLibrary
 * @param file The name of the properties file to open.
 * @param errorBuf The buffer into which error messages will be written, may be NULL.
 * @param bufLength The length of the buffer in bytes.
 * @return The parsed properties file, or NULL on error in which case an
 * appropriate error string will be written into the errorBuf.
 */
j9props_file_t props_file_open(J9PortLibrary* portLibrary, const char* file, char* errorBuf, UDATA errorBufLength);

/**
 * Closes a properties file.
 * @param file The file to close.
 */
void props_file_close(j9props_file_t file);

/**
 * Looks up a key in the properties.
 * @param file The file to inspect.
 * @param key The key to look up.
 * @return The associated value, or NULL if the key does not exist.
 */
const char* props_file_get(j9props_file_t file, const char* key);

/**
 * Looks up a key in the properties.
 * @param file The file to inspect.
 * @param key The key to look up.
 * @return The associated value, or NULL if the key does not exist.
 */
void props_file_do(j9props_file_t file, j9props_file_iterator iterator, void* userData);

/* ----------------- ObjectHash.cpp ---------------- */
/**
 * Hash an UDATA via murmur3 algorithm
 *
 * @param vm 		a java VM
 * @param value 	an UDATA Value
 * @return hash 	an I_32 that may be negative or zero
 */
I_32 convertValueToHash(J9JavaVM *vm, UDATA value);

/**
 * Compute hashcode of an objectPointer via murmur3 algorithm
 *
 * @pre objectPointer must be a valid object reference
 *
 * @param vm 				a java VM
 * @param objectPointer 	a valid object reference.
 * @return value			an I_32 that may be negative or zero
 */
I_32 computeObjectAddressToHash(J9JavaVM *vm, j9object_t objectPointer);


/**
 * Fetch objectPointer's hashcode
 *
 * @pre objectPointer must be a valid object reference
 *
 * @param vm				a java VM
 * @param objectPointer 	a valid object reference.
 * @return hash value		a I_32 that may be negative or zero
 */
I_32 objectHashCode(J9JavaVM *vm, j9object_t objectPointer);

#if defined(WIN32)
/* ---------------- openlibrary.c ---------------- */

/**
 * Opens the given library. Lookup in System directory to load the library.
 * If not found, look up in Windows directory.
 *
 * @param[in] name The name of the library to open
 * @param[in] descriptor The dll handle
 * @param[in] flags To determine whether name needs suffix appended
 *
 * @return 0 if success, Windows error code on failure
 */
UDATA
j9util_open_system_library(char *name, UDATA *descriptor, UDATA flags);

#endif /*if defined(WIN32)*/


#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
/* ---------------- freessp.c ---------------- */

/**
 * Register the J9VMThread->systemStackPointer field with the operating system.
 * This function needs to be called by the running thread on itself as the OS
 * registers a link between this J9VMThread's stack pointer register and the
 * native thread.  This allows the JIT to use the system stack pointer as a
 * general purpose register while also allowing the OS to still have the
 * necessary access to the C stack.
 *
 * @param[in] currentThread The current J9VMThread to enable freeSSP on.
 */
void registerSystemStackPointerThreadOffset(J9VMThread *currentThread);

#endif /*if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)*/



#if defined(AIXPPC)

/* ---------------- sethwprefetch.c ---------------- */

/**
 * HWPrefetch value on AIX, corresponding to
 * DPFD_DEFAULT
 */
#define XXSETHWPREFETCH_OS_DEFAULT_VALUE 0

/**
 * HWPrefetch value on AIX, corresponding to
 * DPFD_NONE
 */
#define XXSETHWPREFETCH_NONE_VALUE 1

/**
 * Set hardware prefetch value on AIX.
 *
 * @param[in] value Valid values are:
 * 	DPFD_DEFAULT 0
 * 	DPFD_NONE 1
 * 	DPFD_SHALLOWEST 2
 * 	DPFD_SHALLOW 3
 * 	DPFD_MEDIUM 4
 * 	DPFD_DEEP 5
 * 	DPFD_DEEPER 6
 * 	DPFD_DEEPEST 7
 * 	DSCR_SSE 8
 *
 * @return 0 on success, -1 on error
 *
 */

IDATA setHWPrefetch(UDATA value);

#endif /*if defined(AIXPPC)*/


#if defined(LINUX)
/* ---------------- osinfo.c ---------------- */

/**
 * Opens /proc/sys/kernel/sched_compat_yield if it exists
 * and returns the value.
 *
 * @param[in] javaVM A pointer to the J9JavaVM structure.
 *
 * @return A char representing the value of /proc/sys/kernel/sched_compat_yield or
 * 		   a whitespace char (' ') if the file does not exist or an error occurred.
 */
char
j9util_sched_compat_yield_value(J9JavaVM *javaVM);

#endif /*if defined(LINUX)*/

/* ----------------  jnierrhelp.c ---------------- */

/**
 * Map from OMR error code to JNI error code
 *
 * @param[in] OMR error code
 *
 * @return JNI error code
 */
jint omrErrorCodeToJniErrorCode(omr_error_t omrError);

/* ---------------- jniprotect.c ---------------- */

/**
 * Signal protects the call to function with the VM signal handler.
 * The gpProtected field of the thread is set and unset appropriately.
 *
 * @param[in] function The function to signal protect.
 * @param[in] env The JNIEnv* of the current thread.
 * @param[in] args Argument to pass to the protected function.
 *
 * @return The result of the protected function.
 */
UDATA
gpProtectAndRun(protected_fn function, JNIEnv * env, void *args);


/* ---------------- resolvehelp.c ---------------- */

/**
 * Get the "right" method from the superclass if this is meant to be a super send (invokespecial).
 *
 * @param vmStruct the current vmThread
 * @param currentClass the class that contains the invokespecial
 * @param resolvedClass the class resolved from the MethodRef in the constantpool
 * @param method the J9Method returned by javaLookupMethod() for the MethodRef.
 * @param lookupOptions options for javaLookupMethod in the case of re-resolution for super send
 *
 * @return the J9Method for the super send or the passed in the J9Method if this is not a super send.
 */
J9Method *
getMethodForSpecialSend(J9VMThread *vmStruct, J9Class *currentClass, J9Class *resolvedClass, J9Method *method, UDATA lookupOptions);

/**
 * JVMS 4.9.2: If resolvedClass is an interface, ensure that it is a DIRECT superinterface of currentClass,
 * or resolvedClass == currentClass.
 *
 * @param vmStruct the current vmThread
 * @param resolvedClass the class resolved from the MethodRef in the constantpool
 * @param currentClass the class that contains the invokespecial
 *
 * @return TRUE if resolvedClass is a direct superinterface of (or equal to) currentClass, and FALSE otherwise.
 */
BOOLEAN
isDirectSuperInterface(J9VMThread *vmStruct, J9Class *resolvedClass, J9Class *currentClass);

/**
 * Error message helper for throwing IncompatibleClassChangeError for invalid supersends (target not direct superinterface)
 *
 * @param vmStruct the current vmThread
 * @param currentClass the class that contains the invokespecial
 * @param resolvedClass the class resolved from the MethodRef in the constantpool
 */
void
setIncompatibleClassChangeErrorInvalidDefenderSupersend(J9VMThread *vmStruct, J9Class *resolvedClass, J9Class *currentClass);

void
printModifiers(J9PortLibrary *portLib, U_32 modifiers, modifierScope modScope, modifierSource modifierSrc, BOOLEAN valueTypeClass);

void
j9_printClassExtraModifiers(J9PortLibrary *portLib, U_32 modifiers);

/* ---------------- vmstate.c ---------------- */

/**
 * Set the vmState of the current thread.
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] newState the new vmState value
 *
 * @return the previous vmState value
 */
UDATA
setVMState(J9VMThread *currentThread, UDATA newState);

/* ---------------- modularityHelper.c ---------------- */

#if JAVA_SPEC_VERSION >= 11

#define ERRCODE_SUCCESS                             0
#define ERRCODE_GENERAL_FAILURE                     1
#define ERRCODE_PACKAGE_ALREADY_DEFINED             2
#define ERRCODE_MODULE_ALREADY_DEFINED              3
#define ERRCODE_HASHTABLE_OPERATION_FAILED          4
#define ERRCODE_DUPLICATE_PACKAGE_IN_LIST           5
#define ERRCODE_MODULE_WASNT_FOUND                  6
#define ERRCODE_PACKAGE_WASNT_FOUND                 7

/** Helper function to identify unnamed module. A module is unnamed if the
 *  name field is NULL
 *  @note It assumes the caller guarantees moduleObject is not NULL
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] moduleObject the module object to be checked
 *
 * @return true if it is an unnamed module, false otherwise.
 *
 */
BOOLEAN
isModuleUnnamed(J9VMThread * currentThread, j9object_t moduleObject);

/**
 * Determine if fromModule has read access to toModule
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] fromModule the module attempting to read
 * @param[in] toModule the module to be read
 * @param[in] errCode the status code returned
 *
 * @return a boolean value to indicate if the access is granted
 */
BOOLEAN
isAllowedReadAccessToModule(J9VMThread * currentThread, J9Module * fromModule, J9Module * toModule, UDATA * errCode);

/**
 * Determine if fromModule has been defined
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] fromModule the module to be checked
 *
 * @return a boolean value to indicate if the module has been defined
 */
BOOLEAN
isModuleDefined(J9VMThread * currentThread, J9Module * fromModule);

/**
 * Determine if a package within fromModule is exported to toModule
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] fromModule the module containing the package
 * @param[in] packageName the package to be checked if it is exported to toModule
 * @param[in] len length of the package name to be checked
 * @param[in] toModule the module to be checked if the package is exported to it
 * @param[in] toUnnamed the flag indicating if toModule is an unnamed module
 * @param[in] errCode the status code returned
 *
 * @return true if the package is exported to toModule, false if otherwise
 */
BOOLEAN
isPackageExportedToModuleWithName(J9VMThread *currentThread, J9Module *fromModule, U_8 *packageName, U_16 len, J9Module *toModule, BOOLEAN toUnnamed, UDATA *errCode);

/**
 * Get a package definition
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] fromModule the module containing the package
 * @param[in] packageName the package name for package hashtable query
 * @param[in] errCode the status code returned
 *
 * @return the package definition
 */
J9Package*
getPackageDefinition(J9VMThread * currentThread, J9Module * fromModule, const char *packageName, UDATA * errCode);
/**
 * Get a pointer to J9Package structure associated with incoming classloader and package name
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] classLoader the classloader with package hashtable
 * @param[in] packageName the package name for query
 *
 * @return a pointer to J9Package structure associated with incoming classloader and package name
 */
J9Package*
hashPackageTableAt(J9VMThread * currentThread, J9ClassLoader * classLoader, const char *packageName);
/**
 * Add UTF package name to construct a J9Package for hashtable query
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] j9package the J9Package structure for hashtable query
 * @param[in] packageName the package name for query
 * @param[in] buf the buffer to store UTF package name
 * @param[in] bufLen the buffer length
 *
 * @return true if finished successfully, false if NativeOutOfMemoryError occurred
 */
BOOLEAN
addUTFNameToPackage(J9VMThread *currentThread, J9Package *j9package, const char *packageName, U_8 *buf, UDATA bufLen);

/**
 * Find the J9Package with given package name. Caller needs to hold the
 * classLoaderModuleAndLocationMutex before calling this function
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] fromModule module owning the package
 * @param[in] packageName name of package to search for
 * @param[in] len length of the package to search for
 * @param[out] errCode return err code
 *
 * @return J9Package, NULL otherwise with errCode set
 */
J9Package*
getPackageDefinitionWithName(J9VMThread *currentThread, J9Module *fromModule, U_8 *packageName, U_16 len, UDATA *errCode);

#endif /* JAVA_SPEC_VERSION >= 11 */

/* ---------------- strhelp.c ---------------- */
/* This function searches for the last occurrence of the character c in a string given its length
 *
 * @param[in] str The string
 * @param[in] c The character to be located
 * @param[in] len The length of the string
 *
 * @return The last occurrence of the character c, or null if c is not found
 */
char*
strnrchrHelper(const char *str, char c, UDATA len);

/* Check if a string has a given suffix.
 *
 * @param[in] str The string
 * @param[in] strLen The length of the string without null-terminator
 * @param[in] suffix The suffix string
 * @param[in] suffixLen The length of the suffix string without null-terminator
 *
 * @return TRUE if str ends with suffix, FALSE otherwise
 */
BOOLEAN
isStrSuffixHelper(const char* str, UDATA strLen, const char* suffix, UDATA suffixLen);
#ifdef __cplusplus
}
#endif

#endif /* util_api_h */
