/*******************************************************************************
 * Copyright (c) 2021, 2022 IBM Corp. and others
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

#include "j9.h"
#include "j9protos.h"
#include "j9vmnls.h"
#include "objhelp.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "AtomicSupport.hpp"
#include "ObjectAllocationAPI.hpp"
#include "VMAccess.hpp"

extern "C" {

#if JAVA_SPEC_VERSION >= 16

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and ignore the return value in the case of void.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return void
 */
void JNICALL
native2InterpJavaUpcall0(J9UpcallMetaData *data, void *argsListPointer)
{
}

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return an I_32 value in the case of byte/char/short/int.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return an I_32 value
 */
I_32 JNICALL
native2InterpJavaUpcall1(J9UpcallMetaData *data, void *argsListPointer)
{
	return (I_32)0;
}

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return an I_64 value in the case of long/pointer.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return an I_64 value
 */
I_64 JNICALL
native2InterpJavaUpcallJ(J9UpcallMetaData *data, void *argsListPointer)
{
	return (I_64)0;
}

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return a float value as specified in the return type.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return a float
 */
float JNICALL
native2InterpJavaUpcallF(J9UpcallMetaData *data, void *argsListPointer)
{
	return (float)0;
}

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return a double value as specified in the return type.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return a double
 */
double JNICALL
native2InterpJavaUpcallD(J9UpcallMetaData *data, void *argsListPointer)
{
	return (double)0;
}

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return a U_8 pointer to the requested struct.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return a U_8 pointer
 */
U_8 * JNICALL
native2InterpJavaUpcallStruct(J9UpcallMetaData *data, void *argsListPointer)
{
	return NULL;
}

#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
