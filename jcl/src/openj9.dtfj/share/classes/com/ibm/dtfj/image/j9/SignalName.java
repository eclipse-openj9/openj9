/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2022
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
 */
package com.ibm.dtfj.image.j9;

/**
 * Methods for mapping OS-specific signal numbers to human-readable strings.
 */
public final class SignalName {

	public static String forAIX(int signal) {
		/* Derived from the output of "kill -l":
		 *
		 *  1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL       5) SIGTRAP
		 *  6) SIGABRT      7) SIGEMT       8) SIGFPE       9) SIGKILL     10) SIGBUS
		 * 11) SIGSEGV     12) SIGSYS      13) SIGPIPE     14) SIGALRM     15) SIGTERM
		 * 16) SIGURG      17) SIGSTOP     18) SIGTSTP     19) SIGCONT     20) SIGCHLD
		 * 21) SIGTTIN     22) SIGTTOU     23) SIGIO       24) SIGXCPU     25) SIGXFSZ
		 * 27) SIGMSG      28) SIGWINCH    29) SIGPWR      30) SIGUSR1     31) SIGUSR2
		 * 32) SIGPROF     33) SIGDANGER   34) SIGVTALRM   35) SIGMIGRATE  36) SIGPRE
		 * 37) SIGVIRT     38) SIGALRM1    39) SIGWAITING  50) SIGRTMIN    51) SIGRTMIN+1
		 * 52) SIGRTMIN+2  53) SIGRTMIN+3  54) SIGRTMAX-3  55) SIGRTMAX-2  56) SIGRTMAX-1
		 * 57) SIGRTMAX    60) SIGKAP      61) SIGRETRACT  62) SIGSOUND    63) SIGSAK
		 */

		switch (signal) {
		case 1:
			return "SIGHUP";
		case 2:
			return "SIGINT";
		case 3:
			return "SIGQUIT";
		case 4:
			return "SIGILL";
		case 5:
			return "SIGTRAP";
		case 6:
			return "SIGABRT";
		case 7:
			return "SIGEMT";
		case 8:
			return "SIGFPE";
		case 9:
			return "SIGKILL"; // included for completeness, even though it can't be caught
		case 10:
			return "SIGBUS";
		case 11:
			return "SIGSEGV";
		case 12:
			return "SIGSYS";
		case 13:
			return "SIGPIPE";
		case 14:
			return "SIGALRM";
		case 15:
			return "SIGTERM";
		case 16:
			return "SIGURG";
		case 17:
			return "SIGSTOP";
		case 18:
			return "SIGTSTP";
		case 19:
			return "SIGCONT";
		case 20:
			return "SIGCHLD";
		case 21:
			return "SIGTTIN";
		case 22:
			return "SIGTTOU";
		case 23:
			return "SIGIO";
		case 24:
			return "SIGXCPU";
		case 25:
			return "SIGXFSZ";
		case 27:
			return "SIGMSG";
		case 28:
			return "SIGWINCH";
		case 29:
			return "SIGPWR";
		case 30:
			return "SIGUSR1";
		case 31:
			return "SIGUSR2";
		case 32:
			return "SIGPROF";
		case 33:
			return "SIGDANGER";
		case 34:
			return "SIGVTALRM";
		case 35:
			return "SIGMIGRATE";
		case 36:
			return "SIGPRE";
		case 37:
			return "SIGVIRT";
		case 38:
			return "SIGTALRM1";
		case 39:
			return "WAITING";
		case 60:
			return "SIGJAP";
		case 61:
			return "SIGRETRACT";
		case 62:
			return "SIGSOUND";
		case 63:
			return "SIGSAK";
		default:
			return null;
		}
	}

	public static String forLinux(int signal) {
		/* Derived from the output of "kill -l":
		 *
		 *  1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL       5) SIGTRAP
		 *  6) SIGABRT      7) SIGBUS       8) SIGFPE       9) SIGKILL     10) SIGUSR1
		 * 11) SIGSEGV     12) SIGUSR2     13) SIGPIPE     14) SIGALRM     15) SIGTERM
		 * 16) SIGSTKFLT   17) SIGCHLD     18) SIGCONT     19) SIGSTOP     20) SIGTSTP
		 * 21) SIGTTIN     22) SIGTTOU     23) SIGURG      24) SIGXCPU     25) SIGXFSZ
		 * 26) SIGVTALRM   27) SIGPROF     28) SIGWINCH    29) SIGIO       30) SIGPWR
		 * 31) SIGSYS      34) SIGRTMIN    35) SIGRTMIN+1  36) SIGRTMIN+2  37) SIGRTMIN+3
		 * 38) SIGRTMIN+4  39) SIGRTMIN+5  40) SIGRTMIN+6  41) SIGRTMIN+7  42) SIGRTMIN+8
		 * 43) SIGRTMIN+9  44) SIGRTMIN+10 45) SIGRTMIN+11 46) SIGRTMIN+12 47) SIGRTMIN+13
		 * 48) SIGRTMIN+14 49) SIGRTMIN+15 50) SIGRTMAX-14 51) SIGRTMAX-13 52) SIGRTMAX-12
		 * 53) SIGRTMAX-11 54) SIGRTMAX-10 55) SIGRTMAX-9  56) SIGRTMAX-8  57) SIGRTMAX-7
		 * 58) SIGRTMAX-6  59) SIGRTMAX-5  60) SIGRTMAX-4  61) SIGRTMAX-3  62) SIGRTMAX-2
		 * 63) SIGRTMAX-1  64) SIGRTMAX
		 */

		switch (signal) {
		case 1:
			return "SIGHUP";
		case 2:
			return "SIGINT";
		case 3:
			return "SIGQUIT";
		case 4:
			return "SIGILL";
		case 5:
			return "SIGTRAP";
		case 6:
			return "SIGABRT";
		case 7:
			return "SIGBUS";
		case 8:
			return "SIGFPE";
		case 9:
			return "SIGKILL"; // included for completeness, even though it can't be caught
		case 10:
			return "SIGUSR1";
		case 11:
			return "SIGSEGV";
		case 12:
			return "SIGUSR2";
		case 13:
			return "SIGPIPE";
		case 14:
			return "SIGALRM";
		case 15:
			return "SIGTERM";
		case 16:
			return "SIGSTKFLT";
		case 17:
			return "SIGCHLD";
		case 18:
			return "SIGCONT";
		case 19:
			return "SIGSTOP";
		case 20:
			return "SIGTSTP";
		case 21:
			return "SIGTTIN";
		case 22:
			return "SIGTTOU";
		case 23:
			return "SIGURG";
		case 24:
			return "SIGXCPU";
		case 25:
			return "SIGXFSZ";
		case 26:
			return "SIGVTALRM";
		case 27:
			return "SIGPROF";
		case 28:
			return "SIGWINCH";
		case 29:
			return "SIGIO";
		case 30:
			return "SIGPWR";
		case 31:
			return "SIGSYS";
		default:
			return null;
		}
	}

	public static String forOSX(int signal) {
		/* Derived from the output of "kill -l".
		 *
		 *  1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL       5) SIGTRAP
		 *  6) SIGABRT      7) SIGEMT       8) SIGFPE       9) SIGKILL     10) SIGBUS
		 * 11) SIGSEGV     12) SIGSYS      13) SIGPIPE     14) SIGALRM     15) SIGTERM
		 * 16) SIGURG      17) SIGSTOP     18) SIGTSTP     19) SIGCONT     20) SIGCHLD
		 * 21) SIGTTIN     22) SIGTTOU     23) SIGIO       24) SIGXCPU     25) SIGXFSZ
		 * 26) SIGVTALRM   27) SIGPROF     28) SIGWINCH    29) SIGINFO     30) SIGUSR1
		 * 31) SIGUSR2
		 */

		switch (signal) {
		case 1:
			return "SIGHUP";
		case 2:
			return "SIGINT";
		case 3:
			return "SIGQUIT";
		case 4:
			return "SIGILL";
		case 5:
			return "SIGTRAP";
		case 6:
			return "SIGABRT";
		case 7:
			return "SIGEMT";
		case 8:
			return "SIGFPE";
		case 9:
			return "SIGKILL"; // included for completeness, even though it can't be caught
		case 10:
			return "SIGBUS";
		case 11:
			return "SIGSEGV";
		case 12:
			return "SIGSYS";
		case 13:
			return "SIGPIPE";
		case 14:
			return "SIGALRM";
		case 15:
			return "SIGTERM";
		case 16:
			return "SIGURG";
		case 17:
			return "SIGSTOP";
		case 18:
			return "SIGTSTP";
		case 19:
			return "SIGCONT";
		case 20:
			return "SIGCHLD";
		case 21:
			return "SIGTTIN";
		case 22:
			return "SIGTTOU";
		case 23:
			return "SIGIO";
		case 24:
			return "SIGXCPU";
		case 25:
			return "SIGXFSZ";
		case 26:
			return "SIGVTALRM";
		case 27:
			return "SIGPROF";
		case 28:
			return "SIGWINCH";
		case 29:
			return "SIGINFO";
		case 30:
			return "SIGUSR1";
		case 31:
			return "SIGUSR2";
		default:
			return null;
		}
	}

	public static String forWindows(int signal) {
		switch (signal) {
		case 0xc0000005:
			return "EXCEPTION_ACCESS_VIOLATION";
		case 0xc000001d:
			return "EXCEPTION_ILLEGAL_INSTRUCTION";
		case 0xc000008d:
			return "EXCEPTION_FLT_DENORMAL_OPERAND";
		case 0xc000008e:
			return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
		case 0xc000008f:
			return "EXCEPTION_FLT_INEXACT_RESULT";
		case 0xc0000090:
			return "EXCEPTION_FLT_INVALID_OPERATION";
		case 0xc0000091:
			return "EXCEPTION_FLT_OVERFLOW";
		case 0xc0000092:
			return "EXCEPTION_FLT_STACK_CHECK";
		case 0xc0000093:
			return "EXCEPTION_FLT_UNDERFLOW";
		case 0xc0000094:
			return "EXCEPTION_INT_DIVIDE_BY_ZERO";
		case 0xc0000095:
			return "EXCEPTION_INT_OVERFLOW";
		case 0xc0000096:
			return "EXCEPTION_PRIV_INSTRUCTION";
		case 0xc00000fd:
			return "EXCEPTION_STACK_OVERFLOW";
		default:
			// answer a more easily readable string for other codes
			return String.format("Unknown signal number 0x%08x", signal);
		}
	}

	public static String forZOS(int signal) {
		/* Derived from the output of "kill -l":
		 *
		 *  1) SIGHUP       2) SIGINT       3) SIGABRT      4) SIGILL       5) SIGPOLL
		 *  6) SIGURG       7) SIGSTOP      8) SIGFPE       9) SIGKILL     10) SIGBUS
		 * 11) SIGSEGV     12) SIGSYS      13) SIGPIPE     14) SIGALRM     15) SIGTERM
		 * 16) SIGUSR1     17) SIGUSR2     19) SIGCONT     20) SIGCHLD     21) SIGTTIN
		 * 22) SIGTTOU     23) SIGIO       24) SIGQUIT     25) SIGTSTP     26) SIGTRAP
		 * 28) SIGWINCH    29) SIGXCPU     30) SIGXFSZ     31) SIGVTALRM   32) SIGPROF
		 * 33) SIGDANGER
		 */

		switch (signal) {
		case 1:
			return "SIGHUP";
		case 2:
			return "SIGINT";
		case 3:
			return "SIGABRT";
		case 4:
			return "SIGILL";
		case 5:
			return "SIGPOLL";
		case 6:
			return "SIGURG";
		case 7:
			return "SIGSTOP";
		case 8:
			return "SIGFPE";
		case 9:
			return "SIGKILL"; // included for completeness, even though it can't be caught
		case 10:
			return "SIGBUS";
		case 11:
			return "SIGSEGV";
		case 12:
			return "SIGSYS";
		case 13:
			return "SIGPIPE";
		case 14:
			return "SIGALRM";
		case 15:
			return "SIGTERM";
		case 16:
			return "SIGUSR1";
		case 17:
			return "SIGUSR2";
		case 19:
			return "SIGCONT";
		case 20:
			return "SIGCHLD";
		case 21:
			return "SIGTTIN";
		case 22:
			return "SIGTTOU";
		case 23:
			return "SIGIO";
		case 24:
			return "SIGQUIT";
		case 25:
			return "SIGTSTP";
		case 26:
			return "SIGTRAP";
		case 28:
			return "SIGWINCH";
		case 29:
			return "SIGXCPU";
		case 30:
			return "SIGXFSZ";
		case 31:
			return "SIGVTALRM";
		case 32:
			return "SIGPROF";
		case 33:
			return "SIGDANGER";
		default:
			return null;
		}
	}

}
