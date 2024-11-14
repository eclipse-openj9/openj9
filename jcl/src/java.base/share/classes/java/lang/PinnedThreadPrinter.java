/*[INCLUDE-IF (JAVA_SPEC_VERSION >= 21) & (JAVA_SPEC_VERSION < 24)]*/
/*
 * Copyright IBM Corp. and others 2023
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
package java.lang;

import java.io.PrintStream;
import java.lang.StackWalker.StackFrame;
import java.lang.StackWalker.StackFrameImpl;
import java.util.List;
import java.util.stream.Collectors;
import jdk.internal.vm.Continuation;

/**
 * Prints the stack trace of a pinned thread that is attempting to yield.
 */
final class PinnedThreadPrinter {

	private static final StackWalker STACKWALKER = StackWalker.newInstanceWithMonitors();

	static void printStackTrace(PrintStream out, boolean printAll) {
		out.println(Thread.currentThread());
		printStackTraceHelper(out, printAll);
	}

	private static void printStackTraceHelper(PrintStream out, boolean printAll) {
		List<StackFrame> stackFrames = STACKWALKER.walk(s -> s.collect(Collectors.toList()));
		for (int i = 0; i < stackFrames.size(); i++) {
			StackFrameImpl sti = (StackFrameImpl) stackFrames.get(i);
			Object[] monitors = sti.getMonitors();

			if (monitors != null) {
				out.println("    " + sti.toString() + " <== monitors:" + monitors.length); //$NON-NLS-1$ //$NON-NLS-2$
			} else if (sti.isNativeMethod() || (printAll && (sti.getDeclaringClass() != PinnedThreadPrinter.class))) {
				out.println("    " + sti.toString()); //$NON-NLS-1$
			}
		}
	}

	static void printStackTrace(PrintStream out, Continuation.Pinned reason, boolean printAll) {
		out.println(Thread.currentThread() + " reason:" + reason); //$NON-NLS-1$
		printStackTraceHelper(out, printAll);
	}
}
