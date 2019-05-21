/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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


#include "leconditionhandler.h"
#include "j9.h"
#include "j9protos.h"

#include "atoe.h"
#include "j9port.h"



#if 0
#define J9SIGNAL_DEBUG
#endif

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)

void j9vm_inline_le_condition_handler (_FEEDBACK *fc, _INT4 *token, _INT4 *leResult, _FEEDBACK *newfc);
_ENTRY globalLeConditionHandlerENTRY = { (_POINTER)&j9vm_inline_le_condition_handler, NULL };

/**
 * Instead of calling through the port library for signal protection (via
 * j9sig_protect), the VM installs an LE condition handler itself on call-in
 * when -XCEEHDLR is in effect.
 *
 * This is required as j9sig_protect takes the function to protect as a parameter,
 * so the protected function's signature must be known at compile time. But, in this case the protected function
 * is a Java native, who's signature we of course don't know at compile time.
 *
 * j9vm_inline_le_condition_handler uses j9port_control to obtain a function pointer to the handler, omrsig_le_condition_handler,
 * which omrsig_protect_ceehdlr registers (using CEEHDLR) on calls to the port library's sig_protect function.
 *
 * j9vm_inline_le_condition_handler then creates and provides the OMRZOSLEConditionHandlerRecord expected by the port library's
 * 	omrsig_le_condition_handler and calls omrsig_le_condition_handler directly
 *
 * @param[in]	_FEEDBACK *fc 		condition token representing the condition for which this handler was invoked
 * 										- forwarded to the omrsig_le_condition_handler.
 * @param[in]	_INT4 *token 		points to a J9VMThread*. Needed to obtain the portlibrary and by the the OMRZOSLEConditionHandlerRecord.
 * @param[out]	_INT4 *leResult		tells the OS what to do when the condition handler returns.
 * 										- forwarded to the omrsig_le_condition_handler
 * @param[out]	_FEEDBACK *newfc	forwarded to the omrsig_le_condition_handler
 *
 * @see J9VMPlatformDependentZOS390#enterSEH:vmThreadWrapper:
 * @see j9portcontrol.c#j9port_control()
 * @see omrsignal_ceehdlr.c#omrsig_le_condition_handler()
 * @see omrsignal_ceehdlr.c#omrsig_protect_ceehdlr()
 */
void
j9vm_inline_le_condition_handler (_FEEDBACK *fc, _INT4 *token, _INT4 *leResult, _FEEDBACK *newfc)
{

 	struct OMRZOSLEConditionHandlerRecord thisRecord;
	J9VMThread* vmThread = (J9VMThread *) *token;
	PORT_ACCESS_FROM_VMC(vmThread);
	void (*portLibraryInternalLEConditionHandler)(_FEEDBACK *fc, _INT4 *token, _INT4 *leResult, _FEEDBACK *newfc);
	_INT4 portLibLEHandler_token;

#if defined(J9SIGNAL_DEBUG)
	printf("j9vm_inline_le_condition_handler, fc->tok_msgno: %i, fc->tok_facid: %s, fc->tok_sev: %i\n", fc->tok_msgno, e2a_func(fc->tok_facid, 3), fc->tok_sev);fflush(NULL);
#endif

	memset(&thisRecord, 0, sizeof(OMRZOSLEConditionHandlerRecord));

	/* Fake up the OMRZOSLEConditionHandlerRecord as if omrsig_protect_ceehdlr() had created it */
	thisRecord.portLibrary = PORTLIB;
	thisRecord.handler = structuredSignalHandler;
	thisRecord.handler_arg = vmThread;
	thisRecord.flags = J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION;

	portLibLEHandler_token = (_INT4)&thisRecord;

	/* peek into the port library implementation */
	if (j9port_control("SIG_INTERNAL_HANDLER", (UDATA)&portLibraryInternalLEConditionHandler)) {
		/* This will only happen if the portlibrary's support for LE condition handling either wasn't enabled at runtime
		 * or was overridden after the fact. In either case, percolate */
#if defined(J9SIGNAL_DEBUG)
		printf("\t *** j9port_control failed *** \n");
#endif
		*leResult = 20;
		return;
	}

	portLibraryInternalLEConditionHandler(fc, &portLibLEHandler_token, leResult, newfc);

	return;
}

#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */
