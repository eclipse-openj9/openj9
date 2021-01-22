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

public class TestTraceTypes { 

    public static void main(String[] args) {

        String[] templates = new String[5];

        System.out.println("TestMultipleApplications: started");
        templates[0] = Trace.EVENT;
        templates[1] = Trace.EVENT + "Tracepoint #01 - Event Type";
        templates[2] = Trace.ENTRY+ "Tracepoint #02 - Entry Type";
        templates[3] = Trace.EXIT + "Tracepoint #03 - Exit Type";
        templates[4] = Trace.EXCEPTION_EXIT + "Tracepoint #04 - Exception Exit Type";

        int componentId = Trace.registerApplication("TestTraceTypes",templates);

        Trace.set("print=TestTraceTypes");
        
        // Trace from both components.
        Trace.trace(componentId, 1);
        Trace.trace(componentId, 2);        
        Trace.trace(componentId, 3);
        Trace.trace(componentId, 4);
        
        System.out.println("TestTraceTypes: finished");
        
    }

}
