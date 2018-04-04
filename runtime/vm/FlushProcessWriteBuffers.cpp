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
VOID WINAPI FlushProcessWriteBuffers(void);
#elif defined(LINUX) /* WIN32 */
#include <sys/mman.h>
#endif /* LINUX */

#include "vm_internal.h"
#include "ut_j9vm.h"
#include "AtomicSupport.hpp"

extern "C" {

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)

void
flushProcessWriteBuffers(J9JavaVM *vm)
{
#if defined(WIN32)
	FlushProcessWriteBuffers();
#elif defined(LINUX) /* WIN32 */
	int mprc = mprotect(vm->exclusiveGuardPage.address, vm->exclusiveGuardPage.pageSize, PROT_READ | PROT_WRITE);
	Assert_VM_true(0 == mprc);
	VM_AtomicSupport::add((UDATA*)vm->exclusiveGuardPage.address, 1);
	mprc = mprotect(vm->exclusiveGuardPage.address, vm->exclusiveGuardPage.pageSize, PROT_NONE);
	Assert_VM_true(0 == mprc);
#else /* LINUX */
#error flushProcessWriteBuffers unimplemented
#endif /* LINUX */
}

UDATA
initializeExclusiveAccess(J9JavaVM *vm)
{
#if defined(LINUX)
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA rc = 1;
	UDATA pageSize = j9vmem_supported_page_sizes()[0];
	void *addr = j9vmem_reserve_memory(
		NULL,
		pageSize,
		&vm->exclusiveGuardPage,
		J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_COMMIT,
		pageSize, OMRMEM_CATEGORY_VM);
	if (NULL != addr) {
		if (0 == mlock(addr, pageSize)) {
			if (0 == mprotect(vm->exclusiveGuardPage.address, vm->exclusiveGuardPage.pageSize, PROT_NONE)) {
				rc = 0;
			} else {
				j9tty_printf(PORTLIB, "mrprotect bad\n");				
			}
		} else {
			j9tty_printf(PORTLIB, "mlock bad\n");
			/* free mem */
		}
	} else {
		j9tty_printf(PORTLIB, "vmem bad\n");		
	}
	return rc;
#else /* LINUX */
	return 0;
#endif /* LINUX */
}

void
shutDownExclusiveAccess(J9JavaVM *vm)
{
#if defined(LINUX)
	if (NULL != vm->exclusiveGuardPage.address) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		j9vmem_free_memory(vm->exclusiveGuardPage.address, vm->exclusiveGuardPage.pageSize, &vm->exclusiveGuardPage);
		vm->exclusiveGuardPage.address = NULL;
	}
#endif /* LINUX */
}

#endif /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */

} /* extern "C" */
