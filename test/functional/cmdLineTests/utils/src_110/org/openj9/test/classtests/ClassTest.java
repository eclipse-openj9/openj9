/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.openj9.test.classtests;

import java.io.*;
import org.openj9.test.classtests.ClassTestHelper;

/* 
 * Calls the helper class ClassTestHelper to generate the Anonymous class and the Unsafe class. 
 * A duplicate file is in the utils2/src/org/openj9/classtests folder, please also modify that file if this one is modified.
 * */

public class ClassTest {
    public static void main(String[] args) throws Throwable {
        ClassTest ct = new ClassTest();
        ClassTestHelper ch = new ClassTestHelper();
        InputStream is = ct.getClass().getResourceAsStream("AnonClassAndUnsafeClassTest.class");
        ch.func(ct.getClass(), is);
    }
}
