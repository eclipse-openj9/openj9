/*******************************************************************************
 * Copyright (c) 2023, 2023 IBM Corp. and others
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

import static org.junit.jupiter.api.Assertions.assertEquals;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Order;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;

/* Tests that have dependencies must be labelled as @Order(2), @Order(3)... */
/* Tests with no dependencies are labelled as @Order(1) */

/* The resulting class file for this java class will be thrown away. This is only necessary for JUnit to
 * be able to find and interpret the tests. The class file that will be used for testing MicroJIT will be swapped in
 * after initial compilation. Said class file was created using Recaf, so that the bytecodes could be manually injected
 * into the class file. After running run.sh, the decompiled bytecodes can be seen in the mjit.log file.
 */

public class Dup {

    @Tag("dup2Form1<II>I")
    @Order(1)
    public static int dup2Form1(int x, int y) {
        int n = x;
        int n2 = y;
        return n + (n2 + (n + n2));
    }

    @Tag("dup_x1<II>I")
    @Order(1)
    public static int dup_x1(int x, int y) {
        int n = y;
        return n + (x + n);
    }

    @Tag("dup_x2Form1<III>I")
    @Order(1)
    public static int dup_x2Form1(int x, int y, int z) {
        int n = z;
        return n + (x + (y + n));
    }

    @Tag("dup_x2Form2<JI>I")
    @Order(1)
    public static int dup_x2Form2(long x, int y) {
        int n = y;
        return n + (int)(x + (long)n);
    }

    @Tag("dup2_x1Form1<III>I")
    @Order(1)
    public static int dup2_x1Form1(int x, int y, int z) {
        int n = y;
        int n2 = z;
        return n + (n2 + (x + (n + n2)));
    }

    @Tag("dup2_x1Form2<IJ>J")
    @Order(1)
    public static long dup2_x1Form2(int x, long y) {
        long l = y;
        return l + (long)(x + (int)l);
    }

    @Tag("dup2_x2Form1<IIII>I")
    @Order(1)
    public static int dup2_x2Form1(int w, int x, int y, int z) {
        int n = y;
        int n2 = z;
        return n + (n2 + (w + (x + (n + n2))));
    }

    @Tag("dup2_x2Form2<IIJ>J")
    @Order(1)
    public static long dup2_x2Form2(int x, int y, long z) {
        long l = z;
        return l + (long)(x + (y + (int)l));
    }

    @Tag("dup2_x2Form3<JII>I")
    @Order(1)
    public static int dup2_x2Form3(long x, int y, int z) {
        int n = y;
        int n2 = z;
        return n + (n2 + (int)(x + (long)(n + n2)));
    }

    @Tag("dup2_x2Form4<JJ>J")
    @Order(1)
    public static long dup2_x2Form4(long x, long y) {
        long l = y;
        return l + (x + l);
    }

}
