/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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

/**
 * @file
 * @ingroup Port
 * @brief Runtime instrumentation
 */
#include "j9port.h"
#include "portpriv.h"
#include "ut_j9prt.h"
#include "j9ri.h"

#include <errno.h>
#include <string.h>

#define __NR_s390_runtime_instr 342

#if defined(J9VM_PORT_OMRSIG_SUPPORT)
#include "omrsig.h"
#endif /* defined(J9VM_PORT_OMRSIG_SUPPORT) */

/**
 * RION instruction
 * Resulting Condition Code:
 * 	0 Normal completion
 * 	1 --
 * 	2 Problem-state prohibited
 * 	3 Invalid run-time-instrumentation controls
 */
static int 
j9ri_rion(void)
{
	int cc = 1;
	asm volatile(
		"	.insn	ri,0xaa010000,0,0\n"
				"		ipm		%0\n"
				"		srl		%0,28"
		: "=d" (cc) : : "cc");
	return cc;
}

/**
 * RIOFF instruction
 * Resulting Condition Code:
 * 	0 Normal completion
 * 	1 --
 * 	2 Problem-state prohibited
 * 	3 Invalid run-time-instrumentation controls
 */
static int 
j9ri_rioff(void)
{
	int cc = 1;
	asm volatile(
		"	.insn	ri,0xaa030000,0,0\n"
				"		ipm		%0\n"
				"		srl		%0,28"
		: "=d" (cc) : : "cc");
	return cc;
}

/**
 * This is the signalHandler for the real-time RI signal.
 * The real-time signal is sent to the thread if the run-time instrumentation buffer is full,(ENOBUFS == siginfo->si_int ),
 * or if the runtime-instrumentation-halted interrupt occurs.
 * This is a dummy signalHandler because the JIT polls for buffer full by itself.
 */
static void
j9ri_signalHandler(int signal, siginfo_t *siginfo, void *context_arg)
{
	Trc_PRT_ri_signalHandler_Entry();

	Trc_PRT_ri_signalHandler_Exit();
}

void
j9ri_params_init(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams, void *riControlBlock)
{
	Trc_PRT_ri_params_init_Entry();
	riParams->flags = 0;
	riParams->controlBlock = riControlBlock;
	Trc_PRT_ri_params_init_Exit();
}

/**
 * Register a signalHandler for RI real-time signals
 * This is called once from the JIT during TR_J9VM::initializeS390ProcessorFeatures.
 * If the registration fails, RI will not be used.
 */
int32_t
j9ri_enableRISupport(struct J9PortLibrary *portLibrary)
{
#if !defined(J9ZTPF)
	struct sigaction sig;
	Trc_PRT_ri_enableSupport_Entry();
	memset(&sig, 0, sizeof(sig));
	sig.sa_sigaction = j9ri_signalHandler;
	sig.sa_flags = SA_SIGINFO;

	if (-1 == OMRSIG_SIGACTION(SIG_RI_INTERRUPT, &sig, NULL)) {
		Trc_PRT_ri_enableSupport_Exception(errno);
		return -1;
	}

		/*
	 * errno values from syscall routine is as follows:
	 * EOPNOTSUPP -  RI facility is not available
	 * EINVAL - signum is wrong or command is not valid signum
	 */
	if (-1 == syscall(__NR_s390_runtime_instr, 1, SIG_RI_INTERRUPT)) {
		Trc_PRT_ri_enableSupport_Exception(errno);
		OMRSIG_SIGACTION(SIG_RI_INTERRUPT, NULL, NULL);
		return -2;
	}

	if (-1 == syscall(__NR_s390_runtime_instr, 2, SIG_RI_INTERRUPT)) {
		Trc_PRT_ri_enableSupport_Exception(errno);
		OMRSIG_SIGACTION(SIG_RI_INTERRUPT, NULL, NULL);
		return -3;
	}

	Trc_PRT_ri_enableSupport_Exit();
	return 0;
#else /* !defined(J9ZTPF) */
	Trc_PRT_ri_enableSupport_Entry();
	Trc_PRT_ri_enableSupport_Exit();
	return -1;
#endif /* !defined(J9ZTPF) */
}

/**
 * Unregister the signalhandler.
 */
int32_t
j9ri_disableRISupport(struct J9PortLibrary *portLibrary)
{
	Trc_PRT_ri_disableSupport_Entry();

	if (-1 == OMRSIG_SIGACTION(SIG_RI_INTERRUPT, NULL, NULL)) {
		Trc_PRT_ri_disableSupport_Exception(errno);
		return -1;
	}

	Trc_PRT_ri_disableSupport_Exit();
	return 0;
}

void
j9ri_initialize(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams)
{
	int32_t ret_code = -1;
	Trc_PRT_ri_initialize_Entry();

#if !defined(J9ZTPF)
	/*
	 * syscall is s390_runtime_instr
	 * errno values from syscall routine is as follows:
	 * EOPNOTSUPP -  RI facility is not available
	 * EINVAL - signum is wrong or command is not valid signum
	 * ENOMEM - Failed to allocate mem for RI control block
	 */

	ret_code = syscall(__NR_s390_runtime_instr, 1, SIG_RI_INTERRUPT);
	if (0 == ret_code) {
		riParams->flags |= J9PORT_RI_INITIALIZED;
	} else {
		Trc_PRT_ri_initialize_Exception(errno);
	}
#endif /* !defined(J9ZTPF) */
	Trc_PRT_ri_initialize_Exit();
}

void
j9ri_deinitialize(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams)
{
	int32_t ret_code = -1;
	Trc_PRT_ri_deinitialize_Entry();

#if !defined(J9ZTPF)
	/*
	 * errno values from syscall routine is as follows:
	 * EOPNOTSUPP -  RI facility is not available
	 * EINVAL - signum is wrong or command is not valid signum
	 */
	ret_code = syscall(__NR_s390_runtime_instr,2,SIG_RI_INTERRUPT);
	if (0 == ret_code) {
		riParams->flags &= ~(J9PORT_RI_INITIALIZED);  /* Clear J9PORT_RI_INITIALIZE bit */
	} else {
		Trc_PRT_ri_deinitialize_Exception(errno);
	}
#endif /* !defined(J9ZTPF) */
	Trc_PRT_ri_deinitialize_Exit();
}


void
j9ri_enable(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams)
{

	Trc_PRT_ri_enable_Entry();
	if (J9_ARE_ALL_BITS_SET(riParams->flags, J9PORT_RI_INITIALIZED)) {
		/*
		 * ret_code values from RION routine is as follows:
		 * 0x0  - authorization routine successful.
		 * 0x1  - RION failure - Not initialized or bad RI control block
		 */
		int32_t ret_code = j9ri_rion();
		if (0 == ret_code) {
			riParams->flags |= J9PORT_RI_ENABLED;  /* Set J9PORT_RI_ENABLED bit */
		} else {
			Trc_PRT_ri_enable_Exception(ret_code);
		}
	} else {
		/*
		 * Use -1 for return code when RI is not initialized.
		 */
		Trc_PRT_ri_enable_Exception(-1);
	}

	Trc_PRT_ri_enable_Exit();
}

void
j9ri_disable(struct J9PortLibrary *portLibrary, struct J9RIParameters *riParams)
{
	Trc_PRT_ri_disable_Entry();
	if (J9_ARE_ALL_BITS_SET(riParams->flags, J9PORT_RI_INITIALIZED)) {
		/*
		 * ret_code values from RIOFF routine is as follows:
		 * 0x0  - authorization routine successful.
		 * 0x1  - RIOFF failure - Not initialized or bad RI control block
		 */
		int32_t ret_code = j9ri_rioff();
		if (0 == ret_code) {
			riParams->flags &= ~(J9PORT_RI_ENABLED);  /* Clear J9PORT_RI_ENABLED bit */
		} else {
			Trc_PRT_ri_disable_Exception(ret_code);
		}
	} else {
		/*
		 * Use -1 for return code when RI is not initialized.
		 */
		Trc_PRT_ri_disable_Exception(-1);
	}

	Trc_PRT_ri_disable_Exit();
}
