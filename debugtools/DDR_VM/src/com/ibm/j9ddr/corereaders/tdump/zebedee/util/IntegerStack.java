/*******************************************************************************
 * Copyright (c) 2006, 2013 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders.tdump.zebedee.util;

/**
 * This class implements a simple stack of integers.
 */

public final class IntegerStack {

    /** The stack itself */
    int[] stack;
    /** The current depth of the stack */
    int depth;

    /**
     * Create a new stack with the given size. It will grow as necessary.
     */
    public IntegerStack(int size) {
        stack = new int[size];
    }

    /**
     * Push an integer on the stack.
     */
    public void push(int n) {
        if (depth == stack.length) {
            int[] tmp = new int[depth * 2];
            System.arraycopy(stack, 0, tmp, 0, depth);
            stack = tmp;
        }
        stack[depth++] = n;
    }

    /**
     * Pop an integer off the stack.
     */
    public int pop() {
        return stack[--depth];
    }

    /**
     * Return the integer at the top of the stack without popping it.
     */
    public int peek() {
        return stack[depth - 1];
    }

    /**
     * Return the depth of the stack.
     */
    public int depth() {
        return depth;
    }
}
