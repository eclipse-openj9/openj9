/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef bcverify_api_h
#define bcverify_api_h

/**
* @file bcverify_api.h
* @brief Public API for the BCVERIFY module.
*
* This file contains public function prototypes and
* type definitions for the BCVERIFY module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "jni.h"
#include "cfr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- bcverify.c ---------------- */

/**
* @brief
* @param portLib
* @param *verifyData
* @return IDATA
*/
IDATA 
allocateVerifyBuffers (J9PortLibrary * portLib, J9BytecodeVerificationData *verifyData);


/**
* @brief
* @param verifyData
* @param byteCount
* @return void*
*/
void* 
bcvalloc (J9BytecodeVerificationData * verifyData, UDATA byteCount);


/**
* @brief
* @param verifyData
* @param address
* @return void
*/
void 
bcvfree (J9BytecodeVerificationData * verifyData, void* address);


/**
* @brief
* @param portLib
* @param *verifyData
* @return void
*/
void 
freeVerifyBuffers (J9PortLibrary * portLib, J9BytecodeVerificationData *verifyData);


/**
* @brief
* @param portLib
* @param verifyData
* @return void
*/
void 
j9bcv_freeVerificationData (J9PortLibrary * portLib, J9BytecodeVerificationData * verifyData);


/**
* @brief
* @param portLib
* @return J9BytecodeVerificationData *
*/
J9BytecodeVerificationData * 
j9bcv_initializeVerificationData (J9JavaVM* javaVM);


/**
* @brief
* @param portLib
* @param ramClass
* @param romClass
* @param verifyData
* @return IDATA
*/
IDATA 
j9bcv_verifyBytecodes (J9PortLibrary * portLib, J9Class * ramClass, J9ROMClass * romClass,
											   J9BytecodeVerificationData * verifyData);


/**
* @brief
* @param vm
* @param stage
* @param reserved
* @return IDATA
*/
IDATA 
j9bcv_J9VMDllMain (J9JavaVM* vm, IDATA stage, void* reserved);

/* ---------------- chverify.c ---------------- */

/**
* @brief
* @param info
* @return IDATA
*/
I_32 
bcvCheckClassName (J9CfrConstantPoolInfo * info);


/**
* @brief
* @param info
* @return IDATA
*/
I_32 
bcvCheckName (J9CfrConstantPoolInfo * info);

/**
* @brief
* @param info
* @return IDATA
*/
I_32 
bcvCheckMethodName (J9CfrConstantPoolInfo * info);

/**
* @brief
* @param info
* @return IDATA
*/
I_32
bcvIsInitOrClinit (J9CfrConstantPoolInfo * info);

/* ---------------- clconstraints.c ---------------- */

/**
* @brief
* @param vmThread
* @param loader1
* @param loader2
* @param sig1
* @param sig2
* @return UDATA
*/
UDATA 
j9bcv_checkClassLoadingConstraintsForSignature (J9VMThread* vmThread, J9ClassLoader* loader1, J9ClassLoader* loader2, J9UTF8* sig1, J9UTF8* sig2);

/**
* @brief
* @param vmThread
* @param loader1
* @param loader2
* @param name1
* @param name2
* @param length
* @return UDATA
 */
UDATA
j9bcv_checkClassLoadingConstraintForName (J9VMThread* vmThread, J9ClassLoader* loader1, J9ClassLoader* loader2, U_8* name1, U_8* name2, UDATA length, UDATA copyUTFs);

/**
* @brief
* @param vmThread
* @param loader
* @param ramClass
* @return J9Class *
*/
J9Class * 
j9bcv_satisfyClassLoadingConstraint (J9VMThread* vmThread, J9ClassLoader* loader, J9Class* ramClass);


/**
* @brief
* @param jvm
* @return void
*/
void 
unlinkClassLoadingConstraints (J9JavaVM* jvm);


/* ---------------- rtverify.c ---------------- */

/**
* @brief
* @param *verifyData
* @param classLoader
* @param *className
* @param nameLength
* @param reasonCode
* 	output parameter denoting error conditions
* @return J9Class *
*/
J9Class *
j9rtv_verifierGetRAMClass( J9BytecodeVerificationData *verifyData, J9ClassLoader* classLoader, U_8 *className, UDATA nameLength, IDATA *reasonCode);


/**
 * Check whether a mismatched type/error occurs in the method signature for runtime verification
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param utf8string - pointer to method signature string
 * @param pStackTop - pointer to the top of the liveStack->stackElements
 * @return BCV_SUCCESS on success
 *         BCV_FAIL on errors
 *         BCV_ERR_INSUFFICIENT_MEMORY on OOM
 */
IDATA
j9rtv_verifyArguments (J9BytecodeVerificationData *verifyData, J9UTF8 * utf8string, UDATA ** pStackTop);


/**
* @brief
* @param *verifyData
* @return IDATA
*/
IDATA
j9rtv_verifyBytecodes (J9BytecodeVerificationData *verifyData);


/* ---------------- staticverify.c ---------------- */

/**
* @brief
* @param portLib
* @param classfile
* @param segment
* @param segmentLength
* @param freePointer
* @param flags
* @param hasRET
* @return IDATA
*/
IDATA 
j9bcv_verifyClassStructure (J9PortLibrary * portLib, J9CfrClassFile * classfile, U_8 * segment,
										U_8 * segmentLength, U_8 * freePointer, U_32 vmVersionShifted, U_32 flags, I_32 *hasRET);

/**
 * 	Check the validity of a method signature.
 * 	@param info signature
 * 	@param getSlots return number of slots if true, otherwise return pass/fail
 *	@return negative on failure, non-negative on success
 *
 */

IDATA j9bcv_checkMethodSignature (J9CfrConstantPoolInfo * info, BOOLEAN getSlots);

/**
 * 	Check the validity of a field signature.
 * 	@param info signature
 * 	@param currentIndex starting point of the signature string
 *	@return negative on failure, non-negative on success
 *
 */

IDATA j9bcv_checkFieldSignature (J9CfrConstantPoolInfo * info, UDATA currentIndex);

/* ---------------- vrfyhelp.c ---------------- */

/**
* @brief
* @param *verifyData
* @param **stackTopPtr
* @param *argCount
* @return BOOLEAN
*/
BOOLEAN
buildStackFromMethodSignature( J9BytecodeVerificationData *verifyData, UDATA **stackTopPtr, UDATA *argCount);


/**
* @brief
* @param portLib
* @param error
* @return U_8 *
*/
U_8 *
j9bcv_createVerifyErrorString(J9PortLibrary * portLib, J9BytecodeVerificationData * error);

/**
* @brief
* @param verifyData
* @param name
* @param length
* @return UDATA
*/
UDATA 
findClassName(J9BytecodeVerificationData * verifyData, U_8 * name, UDATA length);

/**
* @brief
* @param verifyData
* @param targetClass
* @param type
* @param arity
* @return UDATA
*/
UDATA
convertClassNameToStackMapType(J9BytecodeVerificationData * verifyData, U_8 *name, U_16 length, UDATA type, UDATA arity);

/**
* @brief
* @param *verifyData
* @param type
* @param bytecodes
* @return UDATA
*/
UDATA 
getSpecialType(J9BytecodeVerificationData *verifyData, UDATA type, U_8* bytecodes);


/**
* @brief
* @param *verifyData
* @return void
*/
void 
initializeClassNameList(J9BytecodeVerificationData *verifyData);


/**
* @brief
* @param *verifyData
* @param sourceClass
* @param targetClass
* @param reasonCode
* 	output parameter denoting error conditions
* @return IDATA
*/
IDATA 
isClassCompatible(J9BytecodeVerificationData *verifyData, UDATA sourceClass, UDATA targetClass, IDATA *reasonCode );


/**
* @brief
* @param *verifyData
* @param sourceClass
* @param targetClassName
* @param targetClassNameLength
* @param reasonCode
* 	output parameter denoting error conditions
* @return IDATA
*/
IDATA 
isClassCompatibleByName(J9BytecodeVerificationData *verifyData, UDATA sourceClass, U_8* targetClassName, UDATA targetClassNameLength, IDATA *reasonCode);


/**
* @brief
* @param verifyData
* @param fieldRef
* @param bytecode
* @param receiver
* @param reasonCode
* 	output parameter denoting error conditions
* @return IDATA
*/
IDATA 
isFieldAccessCompatible(J9BytecodeVerificationData * verifyData, J9ROMFieldRef * fieldRef, UDATA bytecode, UDATA receiver, IDATA *reasonCode);


/**
* @brief
* @param *verifyData
* @param *className
* @param reasonCode
* 	output parameter denoting error conditions
* @return U_32
*/
IDATA
isInterfaceClass(J9BytecodeVerificationData * verifyData, U_8* className, UDATA classLength, IDATA *reasonCode);


/**
* @brief
* @param *verifyData
* @param declaringClassName
* @param targetClass
* @param member
* @param isField
* @param reasonCode
* 	output parameter denoting error conditions
* @return UDATA
*/
UDATA 
isProtectedAccessPermitted(J9BytecodeVerificationData *verifyData, J9UTF8* declaringClassName, UDATA targetClass, void* member, UDATA isField, IDATA *reasonCode);

/**
* @brief
* @param *verifyData
* @param *signature
* @return UDATA
*/
UDATA 
parseObjectOrArrayName(J9BytecodeVerificationData *verifyData, U_8 *signature);


/**
* @brief
* @param verifyData
* @param utf8string
* @param stackTop
* @return UDATA *
*/
UDATA *
pushClassType(J9BytecodeVerificationData * verifyData, J9UTF8 * utf8string, UDATA * stackTop);


/**
* @brief
* @param *verifyData
* @param utf8string
* @param *stackTop
* @return UDATA*
*/
UDATA* 
pushFieldType(J9BytecodeVerificationData *verifyData, J9UTF8 * utf8string, UDATA *stackTop);


/**
* @brief
* @param verifyData
* @param romClass
* @param index
* @param stackTop
* @return UDATA *
*/
UDATA * 
pushLdcType(J9BytecodeVerificationData *verifyData, J9ROMClass * romClass, UDATA index, UDATA * stackTop);


/**
* @brief
* @param *verifyData
* @param utf8string
* @param stackTop
* @return UDATA*
*/
UDATA* 
pushReturnType(J9BytecodeVerificationData *verifyData, J9UTF8 * utf8string, UDATA * stackTop);

/**
 * Classification of Unicode characters for use in Java identifiers.
 */
#define VALID_START 2	/**< Character can appear anywhere. */
#define VALID_PART 	1	/**< Character can appear in any but the first position. */
#define INVALID 	0	/**< Character is invalid anywhere in the identifier. */

/**
 * Examines a Unicode character and determines if it is a valid start/part character
 * for use in Java identifiers.
 * @param testChar The character to test.
 * 
 * @return One of the following values:
 * 		VALID_START if testChar can appear in the first position of an identifier.
 * 		VALID_PART if testChar can appear in any but the first positions of an identifier.
 * 		INVALID otherwise.
 */
IDATA
checkCharacter(U_32 testChar);

I_32
isJavaIdentifierStart(U_32 c);

I_32
isJavaIdentifierPart(U_32 c);

#ifdef __cplusplus
}
#endif

#endif /* bcverify_api_h */

