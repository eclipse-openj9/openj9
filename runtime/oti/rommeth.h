/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#ifndef rommeth_h
#define rommeth_h

#include "j9.h"
#include "j9consts.h"
#include "j9protos.h"

#define J9_U32_ALIGNED_ADDRESS(address)	((void *)(((UDATA)(address) + 3) & ~(UDATA)3))  /*U_32 alignment*/
#define J9_METHOD_PARAMS_SIZE_FROM_NUMBER_OF_PARAMS(numberOfParams) (sizeof(U_8) + (numberOfParams * sizeof(J9MethodParameter)))

#define J9EXCEPTIONINFO_HANDLERS(info) ((J9ExceptionHandler *) (((U_8 *) (info)) + sizeof(J9ExceptionInfo)))
#define J9EXCEPTIONINFO_THROWNAMES(info) ((J9SRP *) (((U_8 *) J9EXCEPTIONINFO_HANDLERS(info)) + ((info)->catchCount * sizeof(J9ExceptionHandler))))

#define J9_BYTECODE_START_FROM_ROM_METHOD(romMethod) (((U_8 *) (romMethod)) + sizeof(J9ROMMethod))
#define J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod) \
	(((UDATA) (romMethod)->bytecodeSizeLow) + (((UDATA) (romMethod)->bytecodeSizeHigh) << 16))
#define J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod) ((J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod) + 3) & ~(UDATA)3)

#define J9_BYTECODE_END_FROM_ROM_METHOD(romMethod) (J9_BYTECODE_START_FROM_ROM_METHOD(romMethod) + J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod))
#define J9_MAX_STACK_FROM_ROM_METHOD(romMethod) ((romMethod)->maxStack)
#define J9_EXTENDED_MODIFIERS_ADDR_FROM_ROM_METHOD(romMethod) (((U_32 *) (J9_BYTECODE_START_FROM_ROM_METHOD(romMethod) + J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod))))
#define J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(romMethod) (((U_32 *) (J9_EXTENDED_MODIFIERS_ADDR_FROM_ROM_METHOD(romMethod) + (J9ROMMETHOD_HAS_EXTENDED_MODIFIERS(romMethod) ? 1: 0))))
#define J9_GENERIC_SIGNATURE_FROM_ROM_METHOD(romMethod) ((J9ROMMETHOD_HAS_GENERIC_SIGNATURE(romMethod)) ? NNSRP_PTR_GET(J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(romMethod), J9UTF8 *) : NULL)
#define J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod) ((J9ExceptionInfo *) (J9_GENERIC_SIG_ADDR_FROM_ROM_METHOD(romMethod) + (J9ROMMETHOD_HAS_GENERIC_SIGNATURE(romMethod) ? 1 : 0)))
#define J9_NEXT_ROM_METHOD(romMethod) nextROMMethod(romMethod)
#define J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) ((romMethod)->argCount)
#define J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod) ((romMethod)->tempCount)
#define J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod) ((J9ROMMethod *) (J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod) - sizeof(J9ROMMethod)))
#define J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod) (((J9Method *) ramMethod)->bytecodes)

#endif /* rommeth_h */
