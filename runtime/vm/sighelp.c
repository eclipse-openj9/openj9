/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "j9.h"
#include "vm_api.h"

typedef struct J9SignalMapping {
	const char *signalName;
	jint signalValue;
} J9SignalMapping;

#if defined(WIN32)
#define J9_SIGNAL_MAP_ENTRY(name, value) { name, value }
#else /* defined(WIN32) */
#define J9_SIGNAL_MAP_ENTRY(name, value) { J9_SIG_PREFIX name, value }
#endif /* defined(WIN32) */

static const J9SignalMapping signalMap[] = {
#if defined(SIGABRT)
	J9_SIGNAL_MAP_ENTRY("ABRT", SIGABRT),
#endif /* defined(SIGABRT) */
#if defined(SIGALRM)
	J9_SIGNAL_MAP_ENTRY("ALRM", SIGALRM),
#endif /* defined(SIGALRM) */
#if defined(SIGBREAK)
	J9_SIGNAL_MAP_ENTRY("BREAK", SIGBREAK),
#endif /* defined(SIGBREAK) */
#if defined(SIGBUS)
	J9_SIGNAL_MAP_ENTRY("BUS", SIGBUS),
#endif /* defined(SIGBUS) */
#if defined(SIGCHLD)
	J9_SIGNAL_MAP_ENTRY("CHLD", SIGCHLD),
#endif /* defined(SIGCHLD) */
#if defined(SIGCONT)
	J9_SIGNAL_MAP_ENTRY("CONT", SIGCONT),
#endif /* defined(SIGCONT) */
#if defined(SIGFPE)
	J9_SIGNAL_MAP_ENTRY("FPE", SIGFPE),
#endif /* defined(SIGFPE) */
#if defined(SIGHUP)
	J9_SIGNAL_MAP_ENTRY("HUP", SIGHUP),
#endif /* defined(SIGHUP) */
#if defined(SIGILL)
	J9_SIGNAL_MAP_ENTRY("ILL", SIGILL),
#endif /* defined(SIGILL) */
#if defined(SIGINT)
	J9_SIGNAL_MAP_ENTRY("INT", SIGINT),
#endif /* defined(SIGINT) */
#if defined(SIGIO)
	J9_SIGNAL_MAP_ENTRY("IO", SIGIO),
#endif /* defined(SIGIO) */
#if defined(SIGPIPE)
	J9_SIGNAL_MAP_ENTRY("PIPE", SIGPIPE),
#endif /* defined(SIGPIPE) */
#if defined(SIGPROF)
	J9_SIGNAL_MAP_ENTRY("PROF", SIGPROF),
#endif /* defined(SIGPROF) */
#if defined(SIGQUIT)
	J9_SIGNAL_MAP_ENTRY("QUIT", SIGQUIT),
#endif /* defined(SIGQUIT) */
#if defined(SIGSEGV)
	J9_SIGNAL_MAP_ENTRY("SEGV", SIGSEGV),
#endif /* defined(SIGSEGV) */
#if defined(SIGSYS)
	J9_SIGNAL_MAP_ENTRY("SYS", SIGSYS),
#endif /* defined(SIGSYS) */
#if defined(SIGTERM)
	J9_SIGNAL_MAP_ENTRY("TERM", SIGTERM),
#endif /* defined(SIGTERM) */
#if defined(SIGTRAP)
	J9_SIGNAL_MAP_ENTRY("TRAP", SIGTRAP),
#endif /* defined(SIGTRAP) */
#if defined(SIGTSTP)
	J9_SIGNAL_MAP_ENTRY("TSTP", SIGTSTP),
#endif /* defined(SIGTSTP) */
#if defined(SIGTTIN)
	J9_SIGNAL_MAP_ENTRY("TTIN", SIGTTIN),
#endif /* defined(SIGTTIN) */
#if defined(SIGTTOU)
	J9_SIGNAL_MAP_ENTRY("TTOU", SIGTTOU),
#endif /* defined(SIGTTOU) */
#if defined(SIGURG)
	J9_SIGNAL_MAP_ENTRY("URG", SIGURG),
#endif /* defined(SIGURG) */
#if defined(SIGUSR1)
	J9_SIGNAL_MAP_ENTRY("USR1", SIGUSR1),
#endif /* defined(SIGUSR1) */
#if defined(SIGUSR2)
	J9_SIGNAL_MAP_ENTRY("USR2", SIGUSR2),
#endif /* defined(SIGUSR2) */
#if defined(SIGVTALRM)
	J9_SIGNAL_MAP_ENTRY("VTALRM", SIGVTALRM),
#endif /* defined(SIGVTALRM) */
#if defined(SIGWINCH)
	J9_SIGNAL_MAP_ENTRY("WINCH", SIGWINCH),
#endif /* defined(SIGWINCH) */
#if defined(SIGXCPU)
	J9_SIGNAL_MAP_ENTRY("XCPU", SIGXCPU),
#endif /* defined(SIGXCPU) */
#if defined(SIGXFSZ)
	J9_SIGNAL_MAP_ENTRY("XFSZ", SIGXFSZ),
#endif /* defined(SIGXFSZ) */
#if defined(SIGRECONFIG)
	J9_SIGNAL_MAP_ENTRY("RECONFIG", SIGRECONFIG),
#endif /* defined(SIGRECONFIG) */
#if defined(SIGKILL)
	J9_SIGNAL_MAP_ENTRY("KILL", SIGKILL),
#endif /* defined(SIGKILL) */
#if defined(SIGSTOP)
	J9_SIGNAL_MAP_ENTRY("STOP", SIGSTOP),
#endif /* defined(SIGSTOP) */
#if defined(SIGINFO)
	J9_SIGNAL_MAP_ENTRY("INFO", SIGINFO),
#endif /* defined(SIGINFO) */
#if defined(SIGIOT)
	J9_SIGNAL_MAP_ENTRY("IOT", SIGIOT),
#endif /* defined(SIGIOT) */
#if defined(SIGPOLL)
	J9_SIGNAL_MAP_ENTRY("POLL", SIGPOLL),
#endif /* defined(SIGPOLL) */
	{NULL, J9_SIG_ERR}
};

const char *
signalValueToName(jint signal)
{
	const J9SignalMapping *mapping = NULL;
	for (mapping = signalMap; NULL != mapping->signalName; mapping++) {
		if (signal == mapping->signalValue) {
			return mapping->signalName;
		}
	}
	return NULL;
}

jint
signalNameToValue(const char *signalName)
{
	const J9SignalMapping *mapping = NULL;
	for (mapping = signalMap; NULL != mapping->signalName; mapping++) {
		if (0 == strcmp(signalName, mapping->signalName)) {
			return mapping->signalValue;
		}
	}
	return J9_SIG_ERR;
}
