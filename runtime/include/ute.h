/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#ifndef _IBM_UTE_H
#define _IBM_UTE_H

/* @ddr_namespace: default */

#include <limits.h>

#include "jni.h"
#include "j9.h"
#include "ute_module.h"
#include "ute_core.h"
#include "j9trace.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define J9_RAS_IS_METHOD_TRACED(flags)  (( flags & (J9_RAS_METHOD_TRACING|J9_RAS_METHOD_TRIGGERING) ))
#define UTSINTERFACEFROMVM(vm) ((J9UtServerInterface *)((UtInterface *)((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf)->server)

#define UTSI_TRACEMETHODENTER_FROMVM(vm, thr, method, receiverAddress, methodType) \
	do { \
		RasGlobalStorage *tempRasGbl; \
		tempRasGbl = (RasGlobalStorage *)vm->j9rasGlobalStorage; \
		if (tempRasGbl != NULL) { \
			((J9UtServerInterface *)((UtInterface *)tempRasGbl->utIntf)->server)->TraceMethodEnter(thr, method, receiverAddress, methodType); \
		} \
	} while(0)
#define UTSI_TRACEMETHODEXIT_FROMVM(vm, thr, method, exceptionPtr, returnValuePtr, methodType) \
	do { \
		RasGlobalStorage *tempRasGbl; \
		tempRasGbl = (RasGlobalStorage *)vm->j9rasGlobalStorage; \
		if (tempRasGbl != NULL) { \
			((J9UtServerInterface *)((UtInterface *)tempRasGbl->utIntf)->server)->TraceMethodExit(thr, method, exceptionPtr, returnValuePtr, methodType); \
		} \
	} while(0)

/*
 * =============================================================================
 *   The server interface for calls into UT
 * =============================================================================
 */
typedef struct J9UtServerInterface {
	/* OMR layer API */
	UtServerInterface omrf;

	/* Java layer API */
	void (*TraceMethodEnter)(J9VMThread* thr, J9Method* method, void *receiverAddress, UDATA methodType);
	void (*TraceMethodExit)(J9VMThread* thr, J9Method* method, void* exceptionPtr, void *returnValuePtr, UDATA methodType);
	void (*StartupComplete)(J9VMThread* thr);
} J9UtServerInterface;

#ifdef  __cplusplus
}
#endif
#endif /* !_IBM_UTE_H */
