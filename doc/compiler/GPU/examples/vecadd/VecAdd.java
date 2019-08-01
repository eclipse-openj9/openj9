/*******************************************************************************
 * Copyright (c) 2019 IBM Corp. and others
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

import java.util.Random;

import java.util.stream.IntStream;

public class VecAdd {
    static float[] A;
    static float[] B;
    static float[] C;
    final static int T = 100;
    final static int ROWS = 4*1024*1024;

    public static void main (String[] args) {
        A = new float [ROWS];
        B = new float [ROWS];
        C = new float [ROWS];

        for (int i = 0; i < ROWS; i++) {
            A[i] = (float)i;
            B[i] = (float)i*2;
        }

        for (int iter  = 0; iter < T; iter++) {
            runGPULambda();
        }
        for (int i = 0; i < 10; i++) {
            System.out.println(C[i]);
        }
    }
    
    public static void runGPULambda() {
        IntStream.range(0, ROWS).parallel().forEach( i -> {
                C[i] = A[i] + B[i];
            } );        
    }
}
