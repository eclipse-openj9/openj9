/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#ifndef vmchkdbg_h
#define vmchkdbg_h

#if defined(J9VM_OUT_OF_PROCESS)
#define DONT_REDIRECT_SRP /* Don't let j9dbgext.h redefine NNSRP_GET */
#include "j9dbgext.h"
#include "dbggen.h"

#if !defined(UT_TRACE_OVERHEAD)
#define UT_TRACE_OVERHEAD -1 /* disable assertions and tracepoints out of process */
#endif

#define DBG_ARROW(base, item) dbgReadSlot((UDATA)&((base)->item), sizeof((base)->item))
#define DBG_STAR(ptr) dbgReadSlot((UDATA)(ptr), sizeof(*(ptr)))
#define DBG_INDEX(array, index) dbgReadSlot((UDATA)((array) + (index)), sizeof((array)[0]))

#define VMCHECK_NNSRP_GET(base, field, type) \
	((type) ((((U_8 *) &((base)->field))) + (J9SRP)DBG_ARROW(base, field)))
#define VMCHECK_SRP_GET(base, field, type) \
	((type) (DBG_ARROW(base, field) ? VMCHECK_NNSRP_GET(base, field, type) : NULL))
#define VMCHECK_NNWSRP_GET(base, field, type) \
	((type) ((((U_8 *) &((base)->field))) + (J9WSRP)DBG_ARROW(base, field)))
#define VMCHECK_WSRP_GET(base, field, type) \
	((type) (DBG_ARROW(base, field) ? VMCHECK_NNWSRP_GET(base, field, type) : NULL))

#define J9ROMCLASS_CLASSNAME(base) NNSRP_GET((base)->className, struct J9UTF8*)

/* NOTE: These macros should match generated J9ROMCLASS_*(). */
#define VMCHECK_J9ROMCLASS_CLASSNAME(base) \
	VMCHECK_NNSRP_GET(base, className, struct J9UTF8*)
#define VMCHECK_J9ROMCLASS_SUPERCLASSNAME(base) \
	VMCHECK_SRP_GET(base, superclassName, struct J9UTF8*)
#define VMCHECK_J9ROMCLASS_OUTERCLASSNAME(base) \
	VMCHECK_SRP_GET(base, outerClassName, struct J9UTF8*)
#define VMCHECK_J9ROMCLASS_INTERFACES(base) \
	VMCHECK_NNSRP_GET(base, interfaces, J9SRP*)
#define VMCHECK_J9ROMCLASS_ROMMETHODS(base) \
	VMCHECK_NNSRP_GET(base, romMethods, struct J9ROMMethod*)
#define VMCHECK_J9ROMCLASS_ROMFIELDS(base) \
	VMCHECK_NNSRP_GET(base, romFields, struct J9ROMFieldShape*)
#define VMCHECK_J9ROMCLASS_INNERCLASSES(base) \
	VMCHECK_NNSRP_GET(base, innerClasses, J9SRP*)
#define VMCHECK_J9ROMCLASS_CPSHAPEDESCRIPTION(base) \
	VMCHECK_NNSRP_GET(base, cpShapeDescription, U_32*)

#define VMCHECK_J9_CP_FROM_METHOD(method) \
	((J9ConstantPool *) ((UDATA)(DBG_ARROW(method, constantPool) & ~J9_STARTPC_STATUS)))

#define VMCHECK_J9_NEXT_ROM_METHOD(base) dbgNextROMMethod(base)

#define VMCHECK_PORT_ACCESS_FROM_JAVAVM(vm)

#define vmchkMonitorEnter(vm, monitor)
#define vmchkMonitorExit(vm, monitor)

#define vmchkAllClassesStartDo(vm, walkState) \
	dbgAllClassesStartDo(walkState, vm, NULL)
#define vmchkAllClassesNextDo(vm, walkState) \
	dbgAllClassesNextDo(walkState)
#define vmchkAllClassesEndDo(vm, walkState)

#define vmchkAllClassLoadersStartDo(vm, walkState) \
	dbgAllClassLoadersStartDo(walkState, vm, 0)
#define vmchkAllClassLoadersNextDo(vm, walkState) \
	dbgAllClassLoadersNextDo(walkState)
#define vmchkAllClassLoadersEndDo(vm, walkState)

#else /* J9VM_OUT_OF_PROCESS */

#define DBG_ARROW(base, item) ((UDATA)(base)->item)
#define DBG_STAR(ptr) ((UDATA)*(ptr))
#define DBG_INDEX(array, index) ((UDATA)((array)[(index)]))

#define VMCHECK_SRP_GET(base, field, type) SRP_GET((base)->field, type)

#define VMCHECK_J9ROMCLASS_CLASSNAME(base) J9ROMCLASS_CLASSNAME(base)
#define VMCHECK_J9ROMCLASS_SUPERCLASSNAME(base) J9ROMCLASS_SUPERCLASSNAME(base)
#define VMCHECK_J9ROMCLASS_OUTERCLASSNAME(base) J9ROMCLASS_OUTERCLASSNAME(base)
#define VMCHECK_J9ROMCLASS_INTERFACES(base) J9ROMCLASS_INTERFACES(base)
#define VMCHECK_J9ROMCLASS_ROMMETHODS(base) J9ROMCLASS_ROMMETHODS(base)
#define VMCHECK_J9ROMCLASS_ROMFIELDS(base) J9ROMCLASS_ROMFIELDS(base)
#define VMCHECK_J9ROMCLASS_INNERCLASSES(base) J9ROMCLASS_INNERCLASSES(base)
#define VMCHECK_J9ROMCLASS_CPSHAPEDESCRIPTION(base) J9ROMCLASS_CPSHAPEDESCRIPTION(base)

#define VMCHECK_J9_CP_FROM_METHOD(method) J9_CP_FROM_METHOD(method)

#define VMCHECK_J9_NEXT_ROM_METHOD(base) J9_NEXT_ROM_METHOD(base)

#define VMCHECK_PORT_ACCESS_FROM_JAVAVM(vm) PORT_ACCESS_FROM_JAVAVM(vm)

#if defined(J9VM_THR_PREEMPTIVE)
#define vmchkMonitorEnter(vm, monitor) omrthread_monitor_enter(monitor)
#define vmchkMonitorExit(vm, monitor) omrthread_monitor_exit(monitor)
#else /* defined(J9VM_THR_PREEMPTIVE) */
#define vmchkMonitorEnter(vm, monitor)
#define vmchkMonitorExit(vm, monitor)
#endif /* defined(J9VM_THR_PREEMPTIVE) */

#define vmchkAllClassesStartDo(vm, walkState) \
	(vm)->internalVMFunctions->allClassesStartDo(walkState, vm, NULL)
#define vmchkAllClassesNextDo(vm, walkState) \
	(vm)->internalVMFunctions->allClassesNextDo(walkState)
#define vmchkAllClassesEndDo(vm, walkState) \
	(vm)->internalVMFunctions->allClassesEndDo(walkState)

#define vmchkAllClassLoadersStartDo(vm, walkState) \
	(vm)->internalVMFunctions->allClassLoadersStartDo(walkState, vm, 0)
#define vmchkAllClassLoadersNextDo(vm, walkState) \
	(vm)->internalVMFunctions->allClassLoadersNextDo(walkState)
#define vmchkAllClassLoadersEndDo(vm, walkState) \
	(vm)->internalVMFunctions->allClassLoadersEndDo(walkState)

#endif /* J9VM_OUT_OF_PROCESS */

#define VMCHECK_CLASS_DEPTH(clazz) J9CLASS_DEPTH(clazz)
#define VMCHECK_IS_CLASS_OBSOLETE(clazz) (J9CLASS_FLAGS(clazz) & J9AccClassHotSwappedOut)
#define VMCHECK_J9_CURRENT_CLASS(clazz) (VMCHECK_IS_CLASS_OBSOLETE(clazz) ? (J9Class *)DBG_ARROW(clazz, arrayClass) : (clazz))

#endif /* gcdbgext_h */
