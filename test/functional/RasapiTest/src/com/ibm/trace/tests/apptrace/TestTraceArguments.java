/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.trace.tests.apptrace;

import com.ibm.jvm.Trace;

public class TestTraceArguments { 

    public static void main(String[] args) {

        String[] templates = new String[54];
        Object anyOldObject = new java.lang.Thread();
        Object anotherOldObject = new java.lang.Thread();

        System.out.println("TestTraceArguments: started");

        templates[0] = Trace.EVENT;
        templates[1] = Trace.EVENT + "Tracepoint #1 ";
        templates[2] = Trace.EVENT + "Tracepoint #2 insert1=%s";
        templates[3] = Trace.EVENT + "Tracepoint #3 insert1=%s insert2=%s";
        templates[4] = Trace.EVENT + "Tracepoint #4 insert1=%s insert2=%s insert3=%s";
        templates[5] = Trace.EVENT + "Tracepoint #5 insert1=%s insert2=%p";
        templates[6] = Trace.EVENT + "Tracepoint #6 insert1=%p insert2=%s";
        templates[7] = Trace.EVENT + "Tracepoint #7 insert1=%s insert2=%d";
        templates[8] = Trace.EVENT + "Tracepoint #8 insert1=%d insert2=%s";
        templates[9] = Trace.EVENT + "Tracepoint #9 insert1=%s insert2=%lld";
        templates[10] = Trace.EVENT + "Tracepoint #10 insert1=%lld insert2=%s";     // mkomine@77003
        templates[11] = Trace.EVENT + "Tracepoint #11 insert1=%s insert2=%d";
        templates[12] = Trace.EVENT + "Tracepoint #12 insert1=%d insert2=%s";
        templates[13] = Trace.EVENT + "Tracepoint #13 insert1=%s insert2=%c";
        templates[14] = Trace.EVENT + "Tracepoint #14 insert1=%c insert2=%s";
        templates[15] = Trace.EVENT + "Tracepoint #15 insert1=%s insert2=%f";
        templates[16] = Trace.EVENT + "Tracepoint #16 insert1=%e insert2=%s";
        templates[17] = Trace.EVENT + "Tracepoint #17 insert1=%s insert2=%e";
        templates[18] = Trace.EVENT + "Tracepoint #18 insert1=%e insert2=%s";
        templates[19] = Trace.EVENT + "Tracepoint #19 insert1=%p";
        templates[20] = Trace.EVENT + "Tracepoint #20 insert1=%p insert2=%p";
        templates[21] = Trace.EVENT + "Tracepoint #21 insert1=%d";
        templates[22] = Trace.EVENT + "Tracepoint #22 insert1=%d insert2=%d";
        templates[23] = Trace.EVENT + "Tracepoint #23 insert1=%d insert2=%d insert3=%d";
        templates[24] = Trace.EVENT + "Tracepoint #24 insert1=%lld";
        templates[25] = Trace.EVENT + "Tracepoint #25 insert1=%lld insert2=%lld";
        templates[26] = Trace.EVENT + "Tracepoint #26 insert1=%lld insert2=%lld insert3=%lld";
        templates[27] = Trace.EVENT + "Tracepoint #27 insert1=%d";
        templates[28] = Trace.EVENT + "Tracepoint #28 insert1=%d insert2=%d";
        templates[29] = Trace.EVENT + "Tracepoint #29 insert1=%d insert2=%d insert3=%d";
        templates[30] = Trace.EVENT + "Tracepoint #30 insert1=%c";
        templates[31] = Trace.EVENT + "Tracepoint #31 insert1=%c insert2=%c";
        templates[32] = Trace.EVENT + "Tracepoint #32 insert1=%c insert2=%c insert3=%c";
        templates[33] = Trace.EVENT + "Tracepoint #33 insert1=%f";
        templates[34] = Trace.EVENT + "Tracepoint #34 insert1=%e insert2=%e";
        templates[35] = Trace.EVENT + "Tracepoint #35 insert1=%f insert2=%f insert3=%f";
        templates[36] = Trace.EVENT + "Tracepoint #36 insert1=%d";
        templates[37] = Trace.EVENT + "Tracepoint #37 insert1=%d insert2=%d";
        templates[38] = Trace.EVENT + "Tracepoint #38 insert1=%f insert2=%f insert3=%f";
        templates[39] = Trace.EVENT + "Tracepoint #39 insert1=%s insert2=%p insert3=%s";
        templates[40] = Trace.EVENT + "Tracepoint #40 insert1=%p insert2=%s insert3=%p";
        templates[41] = Trace.EVENT + "Tracepoint #41 insert1=%s insert2=%d insert3=%s";
        templates[42] = Trace.EVENT + "Tracepoint #42 insert1=%d insert2=%s insert3=%d";
        templates[43] = Trace.EVENT + "Tracepoint #43 insert1=%s insert2=%lld insert3=%s";
        templates[44] = Trace.EVENT + "Tracepoint #44 insert1=%lld insert2=%s insert3=%lld";
        templates[45] = Trace.EVENT + "Tracepoint #45 insert1=%s insert2=%d insert3=%s";
        templates[46] = Trace.EVENT + "Tracepoint #46 insert1=%d insert2=%s insert3=%d";
        templates[47] = Trace.EVENT + "Tracepoint #47 insert1=%s insert2=%c insert3=%s";
        templates[48] = Trace.EVENT + "Tracepoint #48 insert1=%c insert2=%s insert3=%c";
        templates[49] = Trace.EVENT + "Tracepoint #49 insert1=%s insert2=%f insert3=%s";
        templates[50] = Trace.EVENT + "Tracepoint #50 insert1=%f insert2=%s insert3=%f";
        templates[51] = Trace.EVENT + "Tracepoint #51 insert1=%s insert2=%d insert3=%s";
        templates[52] = Trace.EVENT + "Tracepoint #52 insert1=%d insert2=%s insert3=%d";
        templates[53] = Trace.EVENT + "Tracepoint #53 TEST PASSED";

        int componentId = Trace.registerApplication("TestTraceArguments",templates);

        Trace.set("print=TestTraceArguments");
        
        // Tests for all the different Trace.trace() methods

        // public static void trace(int, int)
        Trace.trace(componentId,0);
        // public static void trace(int, int)
        Trace.trace(componentId,1);
        // public static void trace(int, int, java.lang.String)
        Trace.trace(componentId,2,"hello");
        // public static void trace(int, int, java.lang.String, java.lang.String)
        Trace.trace(componentId,3,"hello1","hello2");
        // public static void trace(int, int, java.lang.String, java.lang.String, java.lang.String)
        Trace.trace(componentId,4,"hello1","hello2","hello3");
        // public static void trace(int, int, java.lang.String, java.lang.Object)
        Trace.trace(componentId,5,"hello",anyOldObject);
        // public static void trace(int, int, java.lang.Object, java.lang.String)
        Trace.trace(componentId,6,anyOldObject,"hello");
        // public static void trace(int, int, java.lang.String, int)
        Trace.trace(componentId,7,"hello",9);
        // public static void trace(int, int, int, java.lang.String)
        Trace.trace(componentId,8,9,"hello");
        // public static void trace(int, int, java.lang.String, long)
        Trace.trace(componentId,9,"hello",9L);
        // public static void trace(int, int, long, java.lang.String)
        Trace.trace(componentId,10,Long.MAX_VALUE,"hello");
        // public static void trace(int, int, java.lang.String, byte)
        Trace.trace(componentId,11,"hello",Byte.MAX_VALUE);
        // public static void trace(int, int, byte, java.lang.String)
        Trace.trace(componentId,12,Byte.MAX_VALUE,"hello");
        // public static void trace(int, int, java.lang.String, char)
        Trace.trace(componentId,13,"hello",'X');
        // public static void trace(int, int, char, java.lang.String)
        Trace.trace(componentId,14,'X',"hello");
        // public static void trace(int, int, java.lang.String, float)
        Trace.trace(componentId,15,"hello",1.0f);
        // public static void trace(int, int, float, java.lang.String)
        Trace.trace(componentId,16,Double.MAX_VALUE,"hello");
        // public static void trace(int, int, java.lang.String, double)
        Trace.trace(componentId,17,"hello",Double.MAX_VALUE);
        // public static void trace(int, int, double, java.lang.String)
        Trace.trace(componentId,18,Double.MAX_VALUE,"hello");
        // public static void trace(int, int, java.lang.Object)
        Trace.trace(componentId,19,anyOldObject);
        // public static void trace(int, int, java.lang.Object, java.lang.Object)
        Trace.trace(componentId,20,anyOldObject,anotherOldObject);
        // public static void trace(int, int, int)
        Trace.trace(componentId,21,9);
        // public static void trace(int, int, int, int)
        Trace.trace(componentId,22,-1,9);
        // public static void trace(int, int, int, int, int)
        Trace.trace(componentId,23,999999,0,9);
        // public static void trace(int, int, long)
        Trace.trace(componentId,24,9L);
        // public static void trace(int, int, long, long)
        Trace.trace(componentId,25,0L,9L);
        // public static void trace(int, int, long, long, long)
        Trace.trace(componentId,26,Long.MAX_VALUE,Long.MIN_VALUE,9L);
        // public static void trace(int, int, byte)
        Trace.trace(componentId,27,9);
        // public static void trace(int, int, byte, byte)
        Trace.trace(componentId,28,-1,0);
        // public static void trace(int, int, byte, byte, byte)
        Trace.trace(componentId,29,Byte.MAX_VALUE,Byte.MIN_VALUE,0);
        // public static void trace(int, int, char)
        Trace.trace(componentId,30,'A');
        // public static void trace(int, int, char, char)
        Trace.trace(componentId,31,'A','B');
        // public static void trace(int, int, char, char, char)
        Trace.trace(componentId,32,'A','B','C');
        // public static void trace(int, int, float)
        Trace.trace(componentId,33,1.0f);
        // public static void trace(int, int, float, float)
        Trace.trace(componentId,34,Float.MAX_VALUE,Float.MIN_VALUE);
        // public static void trace(int, int, float, float, float)
        Trace.trace(componentId,35,Float.POSITIVE_INFINITY,Float.NEGATIVE_INFINITY,Float.NaN);
        // public static void trace(int, int, double)
        Trace.trace(componentId,36,999999);
        // public static void trace(int, int, double, double)
        Trace.trace(componentId,37,0,-1);
        // public static void trace(int, int, double, double, double)
        Trace.trace(componentId,38,Double.MAX_VALUE,Double.MIN_VALUE,0);
        // public static void trace(int, int, java.lang.String, java.lang.Object, java.lang.String)
        Trace.trace(componentId,39,"hello1",anyOldObject,"hello2");
        // public static void trace(int, int, java.lang.Object, java.lang.String, java.lang.Object)
        Trace.trace(componentId,40,anyOldObject,"hello",anotherOldObject);
        // public static void trace(int, int, java.lang.String, int, java.lang.String)
        Trace.trace(componentId,41,"hello1",9,"hello2");
        // public static void trace(int, int, int, java.lang.String, int)
        Trace.trace(componentId,42,9,"hello1",9);
        // public static void trace(int, int, java.lang.String, long, java.lang.String)
        Trace.trace(componentId,43,"hello1",9L,"hello2");
        // public static void trace(int, int, long, java.lang.String, long)
        Trace.trace(componentId,44,9L,"hello",9L);
        // public static void trace(int, int, java.lang.String, byte, java.lang.String)
        Trace.trace(componentId,45,"hello1",0,"hello2");
        // public static void trace(int, int, byte, java.lang.String, byte)
        Trace.trace(componentId,46,1,"hello",1);
        // public static void trace(int, int, java.lang.String, char, java.lang.String)
        Trace.trace(componentId,47,"hello1",'A',"hello2");
        // public static void trace(int, int, char, java.lang.String, char)
        Trace.trace(componentId,48,'A',"hello",'B');
        // public static void trace(int, int, java.lang.String, float, java.lang.String)
        Trace.trace(componentId,49,"hello1",1.0f,"hello2");
        // public static void trace(int, int, float, java.lang.String, float)
        Trace.trace(componentId,50,-1.0f,"hello",1.0f);
        // public static void trace(int, int, java.lang.String, double, java.lang.String)
        Trace.trace(componentId,51,"hello1",1,"hello2");
        // public static void trace(int, int, double, java.lang.String, double)
        Trace.trace(componentId,52,-1,"hello",1);

        Trace.trace(componentId,53); // test passed message, validation in script
        System.out.println("TestTraceArguments: finished");
        
    }

}
