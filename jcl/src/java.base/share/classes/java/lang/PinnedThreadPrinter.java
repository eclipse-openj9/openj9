/*[INCLUDE-IF JAVA_SPEC_VERSION >= 21]*/
/*******************************************************************************
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
 *******************************************************************************/
package java.lang;

import java.io.PrintStream;
import java.lang.StackWalker.StackFrame;
import java.lang.StackWalker.StackFrameImpl;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;
import static java.lang.StackWalker.Option.*;

/**
 * Prints the stack trace of Pinned Thread that is attempting to yield.
 */
class PinnedThreadPrinter {
    private static StackWalker STACKWALKER;

    static {
        STACKWALKER = StackWalker.getInstance(Set.of(SHOW_REFLECT_FRAMES, RETAIN_CLASS_REFERENCE));
        STACKWALKER.setGetMontiorFlag();
    }

    static void printStackTrace(PrintStream out, boolean printAll) {
        out.println(Thread.currentThread());
        List<StackFrame> stackFrames = STACKWALKER.walk(s -> s.collect(Collectors.toList()));
        for (int i = 0; i < stackFrames.size(); i++) {
            StackFrameImpl sti = (StackFrameImpl)stackFrames.get(i);
            Object[] monitors = sti.getMonitors();

            if (monitors != null) {
                out.println("    " + sti.toString() + " <== monitors:" + monitors.length);
            } else if (sti.isNativeMethod() || (printAll && (sti.getDeclaringClass() != PinnedThreadPrinter.class))) {
                out.println("    " + sti.toString());
            }
        }
    }
}
