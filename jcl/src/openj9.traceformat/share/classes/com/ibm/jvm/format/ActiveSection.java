/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
package com.ibm.jvm.format;

import java.io.BufferedWriter;
import java.io.IOException;

/** 
 * Active section of a file header.
 *
 * @author Tim Preece
 */
public class ActiveSection {
    private                String    eyecatcher_string;
    private                int       length;
    private                int       version;
    private                int       modification;
    private                long      active_offset;
    private                long      active_end;

    private                TraceFile traceFile;

    public ActiveSection (TraceFile traceFile, int start ) throws IOException
    {
        // Version 1.1
        this.traceFile = traceFile;
        traceFile.seek((long)start);
        eyecatcher_string  =  Util.convertAndCheckEyecatcher(traceFile.readI());
        length        = traceFile.readI();
        version       = traceFile.readI();
        modification  = traceFile.readI();
        active_offset = traceFile.getFilePointer();
        active_end    = start + length;

        Util.Debug.println("ActiveSection: eyecatcher:         " + eyecatcher_string);
        Util.Debug.println("ActiveSection: length:             " + length);
        Util.Debug.println("ActiveSection: version:            " + version);
        Util.Debug.println("ActiveSection: modification:       " + modification);

    }

    protected void summary(BufferedWriter out) throws IOException
    {
        traceFile.seek(active_offset);
        byte[] activeBuffer = new byte[(int)(active_end - active_offset)];
        Util.Debug.println("ActiveSection: active_offset:        " + active_offset);
        Util.Debug.println("ActiveSection: active_end:           " + active_end);

        out.write("Activation Info :");
        out.newLine();

        StringBuffer buf;
        traceFile.read(activeBuffer);
        // Util.printDump(activeBuffer,activeBuffer.length);
        for (int i=0, j=0; (i<activeBuffer.length) ; i++) {
            for ( ; (activeBuffer[i]!=0) ; i++) {               // look for null terminator
            }
            if (i == j) {
                break;                                          // a null terminates the list
            }
            out.write(Util.SUM_TAB);                            // indent the new string
            out.write(new String(activeBuffer, j, i - j, "UTF-8"));
            out.newLine();
            j = i + 1;
        }
        out.newLine();
    }
}
