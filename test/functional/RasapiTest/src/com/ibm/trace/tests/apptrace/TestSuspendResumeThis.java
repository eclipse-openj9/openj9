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

public class TestSuspendResumeThis { 
    

    public static void main(String[] args) {

        String[] templates = new String[4];

        System.out.println("TestSuspendResumeThis: started");
        templates[0] = Trace.EVENT;
        templates[1] = Trace.EVENT + "Tracepoint #01 Thread: %s";
        templates[2] = Trace.EVENT + "Tracepoint #02 Thread: %s";
        templates[3] = Trace.EVENT + "Tracepoint #03 Thread: %s";


        final int componentId = Trace.registerApplication("TestSuspendResumeThis",templates);
        Trace.set("print=TestSuspendResumeThis");
        Trace.trace(componentId, 1, Thread.currentThread().getName());
        
        // Test the Trace.suspendThis() method
        Trace.suspendThis();
        Trace.trace(componentId, 2, Thread.currentThread().getName()); // trace suspended, so this tracepoint should not appear
        
        /* Check that trace *is* still produced on another thread. */
        Thread t = new Thread() {

			public void run() {
				Trace.trace(componentId, 1, Thread.currentThread().getName());
				Trace.trace(componentId, 2, Thread.currentThread().getName());
				Trace.trace(componentId, 3, Thread.currentThread().getName());
			}
        	
        };
        t.start();
        try {
			t.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
        
        // Test the Trace.resumeThis() method
        Trace.resumeThis();
        Trace.trace(componentId, 3, Thread.currentThread().getName());

        System.out.println("TestSuspendResumeThis: finished");
        
    }

}

