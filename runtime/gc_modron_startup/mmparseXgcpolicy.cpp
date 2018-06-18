
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
 
/**
 * @file
 * @ingroup GC_Modron_Startup
 */

#include "j9.h"
#include "j9cfg.h"
#include "j2sever.h"
#include "jvminit.h"
#include "mmparse.h"

#include "Configuration.hpp"
#include "GCExtensions.hpp"

#include <string.h>

bool MMINLINE
isOptthruputGCPolicySupported(MM_GCExtensions *extensions)
{
#if defined(J9VM_GC_MODRON_STANDARD)
	return true;
#endif /* J9VM_GC_MODRON_STANDARD */
	return false;
}

bool MMINLINE
isNoGcGCPolicySupported(MM_GCExtensions *extensions)
{
	return true;
}

bool MMINLINE
isOptavgpauseGCPolicySupported(MM_GCExtensions *extensions)
{
#if defined(J9VM_GC_MODRON_STANDARD)
	return true;
#endif /* J9VM_GC_MODRON_STANDARD */
	return false;
}

bool MMINLINE
isGenconGCPolicySupported(MM_GCExtensions *extensions)
{
#if defined(J9VM_GC_MODRON_STANDARD) && defined(J9VM_GC_GENERATIONAL)
	return true;
#endif /* J9VM_GC_MODRON_STANDARD && J9VM_GC_GENERATIONAL */
	return false;
}

bool MMINLINE
isSubpoolAliasGCPolicySupported(MM_GCExtensions *extensions)
{
#if defined(J9VM_GC_MODRON_STANDARD) && defined(J9VM_GC_SUBPOOLS_ALIAS)
	return true;
#endif /* J9VM_GC_MODRON_STANDARD && J9VM_GC_SUBPOOLS_ALIAS */
	return false;
}

bool MMINLINE 
isMetronomeGCPolicySupported(MM_GCExtensions *extensions)
{
#if defined(J9VM_GC_REALTIME)
#if defined(AIXPPC)
	return true;
#elif defined(LINUX) && (defined(J9HAMMER) || defined(J9X86))
	return true;
#endif
#endif /* J9VM_GC_REALTIME */
	return false;
}

bool MMINLINE 
isBalancedGCPolicySupported(MM_GCExtensions *extensions)
{
#if defined (J9VM_GC_VLHGC) && defined (J9VM_ENV_DATA64)
	return true;
#endif /* J9VM_GC_VLHGC && J9VM_ENV_DATA64 */
	return false;
}

/**
 * Consume -Xgcpolicy: arguments.
 * support -XX:+UseNoGC option for compatibility
 * 
 * For compatibility with previous versions multiple gc policy specifications allowed:
 * last one wins
 */
void
gcParseXgcpolicy(MM_GCExtensions *extensions)
{
	J9JavaVM *vm = (J9JavaVM *)extensions->getOmrVM()->_language_vm;
	J9VMInitArgs *vmArgs = vm->vmArgsArray;
	bool enableUnsupported = false;
	
	IDATA xgcpolicyIndex = FIND_ARG_IN_VMARGS_FORWARD( STARTSWITH_MATCH, "-Xgcpolicy:", NULL );
	IDATA lastXgcpolicyIndex = 0;
	while (xgcpolicyIndex >= 0) {
		char *policy = NULL;
		GET_OPTION_VALUE( xgcpolicyIndex, ':', &policy);

		if (NULL != policy) {
			if (0 == strcmp("enableUnsupported", policy)) {
				/* -Xgcpolicy:enableUnsupported permits selection of GC policies which aren't officially supported. */
				CONSUME_ARG(vmArgs, xgcpolicyIndex);
				enableUnsupported = true;
			} else if (0 == strcmp("disableUnsupported", policy)) {
				CONSUME_ARG(vmArgs, xgcpolicyIndex);
				enableUnsupported = false;
			} else {
				lastXgcpolicyIndex = xgcpolicyIndex;
				if (0 == strcmp("optthruput", policy)) {
					if (isOptthruputGCPolicySupported(extensions) || enableUnsupported) {
						CONSUME_ARG(vmArgs, xgcpolicyIndex);
						extensions->configurationOptions._gcPolicy = gc_policy_optthruput;
					}
				} else if (0 == strcmp("subpool", policy)) {
					if (isSubpoolAliasGCPolicySupported(extensions) || enableUnsupported) {
						CONSUME_ARG(vmArgs, xgcpolicyIndex);
						/* subpool is not supported anymore, use optthruput instead */
						extensions->configurationOptions._gcPolicy = gc_policy_optthruput;
					}
				} else if (0 == strcmp("optavgpause", policy)) {
					if (isOptavgpauseGCPolicySupported(extensions) || enableUnsupported) {
						CONSUME_ARG(vmArgs, xgcpolicyIndex);
						extensions->configurationOptions._gcPolicy = gc_policy_optavgpause;
					}
				} else if (0 == strcmp("gencon", policy)) {
					if (isGenconGCPolicySupported(extensions) || enableUnsupported) {
						CONSUME_ARG(vmArgs, xgcpolicyIndex);
						extensions->configurationOptions._gcPolicy = gc_policy_gencon;
					}
				} else if (0 == strcmp("metronome", policy)) {
					if (isMetronomeGCPolicySupported(extensions) || enableUnsupported) {
						CONSUME_ARG(vmArgs, xgcpolicyIndex);
						extensions->configurationOptions._gcPolicy = gc_policy_metronome;
					}
				} else if (0 == strcmp("balanced", policy)) {
					if (isBalancedGCPolicySupported(extensions) || enableUnsupported) {
						CONSUME_ARG(vmArgs, xgcpolicyIndex);
						extensions->configurationOptions._gcPolicy = gc_policy_balanced;
					}
				} else if (0 == strcmp("nogc", policy)) {
					if (isNoGcGCPolicySupported(extensions) || enableUnsupported) {
						CONSUME_ARG(vmArgs, xgcpolicyIndex);
						extensions->configurationOptions._gcPolicy = gc_policy_nogc;
					}
				}
			}
		}
		
		xgcpolicyIndex = FIND_NEXT_ARG_IN_VMARGS_FORWARD( STARTSWITH_MATCH, "-Xgcpolicy:", NULL, xgcpolicyIndex);
	}

	IDATA xxUseNoGCIndex = FIND_AND_CONSUME_ARG(STARTSWITH_MATCH, "-XX:+UseNoGC", NULL);
	if (xxUseNoGCIndex > lastXgcpolicyIndex) {
		if (isNoGcGCPolicySupported(extensions) || enableUnsupported) {
			extensions->configurationOptions._gcPolicy = gc_policy_nogc;
		}
	}
}
