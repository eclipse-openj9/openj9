/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#if defined(WIN32)
#include <windows.h>
#elif defined(LINUX) || defined(AIXPPC) /* WIN32 */
#include <sys/mman.h>
#endif /* LINUX || AIXPPC */

#include "vm_internal.h"
#include "ut_j9vm.h"
#include "AtomicSupport.hpp"

extern "C" {

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)

void
flushProcessWriteBuffers(J9JavaVM *vm)
{
	/* If this is called before the underlying data structures have been initialized,
	 * assume that the VM is single-threaded and proceed without error.
	 */
#if defined(WIN32)
	if (NULL != vm->flushFunction) {
		((VOID (WINAPI*)(void))vm->flushFunction)();
	}
#elif defined(LINUX) || defined(AIXPPC) /* WIN32 */
	if (NULL != vm->flushMutex) {
		omrthread_monitor_enter(vm->flushMutex);
		void *addr = vm->exclusiveGuardPage.address;
		UDATA pageSize = vm->exclusiveGuardPage.pageSize;
		int mprotectrc = mprotect(addr, pageSize, PROT_READ | PROT_WRITE);
		Assert_VM_true(0 == mprotectrc);
		VM_AtomicSupport::add((UDATA*)addr, 1);
		mprotectrc = mprotect(addr, pageSize, PROT_NONE);
		Assert_VM_true(0 == mprotectrc);
		omrthread_monitor_exit(vm->flushMutex);
	}
#else /* LINUX || AIXPPC */
#error flushProcessWriteBuffers unimplemented
#endif /* LINUX || AIXPPC */
}

UDATA
initializeExclusiveAccess(J9JavaVM *vm)
{
	UDATA rc = 0;
#if defined(LINUX) || defined(AIXPPC)
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA pageSize = j9vmem_supported_page_sizes()[0];
	void *addr = j9vmem_reserve_memory(
		NULL,
		pageSize,
		&vm->exclusiveGuardPage,
		J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT | J9PORT_VMEM_NO_AFFINITY,
		pageSize, OMRMEM_CATEGORY_VM);
	if (NULL == addr) {
		Trc_VM_failedtoAllocateGuardPage(pageSize);
		rc = 1;
	} else {
		/* AIX mlock is only allowed as root */
#if defined(LINUX)
		int mlockrc = mlock(addr, pageSize);
		Assert_VM_true(0 == mlockrc);
#endif /* LINUX */
		int mprotectrc = mprotect(addr, pageSize, PROT_NONE);
		Assert_VM_true(0 == mprotectrc);
	}
	if (0 != omrthread_monitor_init_with_name(&vm->flushMutex, 0, "flushProcessWriteBuffers")) {
		shutDownExclusiveAccess(vm);
		rc = 1;
	}
#elif defined(WIN32) /* LINUX || AIXPPC */
	HMODULE h_kernel32 = GetModuleHandle("kernel32");
	Assert_VM_notNull(h_kernel32);
	void *flushFunction = (void*)GetProcAddress(h_kernel32, "FlushProcessWriteBuffers");
	Assert_VM_notNull(flushFunction);
	vm->flushFunction = flushFunction;
#endif /* WIN32 */
	return rc;
}

void
shutDownExclusiveAccess(J9JavaVM *vm)
{
#if defined(LINUX) || defined(AIXPPC)
	J9PortVmemIdentifier *guardPage = &vm->exclusiveGuardPage;
	void *addr = guardPage->address;
	if (NULL != addr) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		j9vmem_free_memory(addr, guardPage->pageSize, guardPage);
	}
	if (NULL != vm->flushMutex) {
		omrthread_monitor_destroy(vm->flushMutex);
		vm->flushMutex = NULL;
	}
#endif /* LINUX || AIXPPC */
}

#endif /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */

} /* extern "C" */
