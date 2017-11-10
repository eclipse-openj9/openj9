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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.jvm.format;

import java.io.BufferedWriter;
import java.io.IOException;

/** 
 * Processor section of a file header
 *
 * @author Tim Preece
 */
public class ProcessorSection {
    private                String    eyecatcher_string;
    private                int       length;
    private                int       version;
    private                int       modification;
    private                int       arch;
    private                boolean   big;
    private                int       word;
    private                int       procs;
    private                int       subType;
    private                int       counter;

    public ProcessorSection (TraceFile traceFile, int start ) throws IOException
    {
        // Version 1.1
        traceFile.seek((long)start);
        eyecatcher_string  =  Util.convertAndCheckEyecatcher(traceFile.readI());
        length        = traceFile.readI();
        version       = traceFile.readI();
        modification  = traceFile.readI();
        traceFile.skipBytes(16); // Skip over HPI header information
        arch          = traceFile.readI();
        big           = ( traceFile.readI() != 0 );
        word          = traceFile.readI();
        procs         = traceFile.readI();
        traceFile.skipBytes(16); // Skip over HPI header information
        subType       = traceFile.readI();
        counter       = traceFile.readI();

        Util.setTimerType(counter);

        Util.Debug.println("ProcessorSection: eyecatcher:      " + eyecatcher_string);
        Util.Debug.println("ProcessorSection: length:          " + length);
        Util.Debug.println("ProcessorSection: version:         " + version);
        Util.Debug.println("ProcessorSection: modification:    " + modification);
        Util.Debug.println("ProcessorSection: arch:            " + arch);
        Util.Debug.println("ProcessorSection: big:             " + big);
        Util.Debug.println("ProcessorSection: word:            " + word);
        Util.Debug.println("ProcessorSection: procs:           " + procs);
        Util.Debug.println("ProcessorSection: subType:         " + subType);
        Util.Debug.println("ProcessorSection: counter:         " + counter);

    }

    protected void summary(BufferedWriter out) throws IOException
    {
        /*
         * These String arrays refer to enum values in entries of the
         * ProcessorInfo structure defined in xhpi.h .
         */
        String[] Archs      = { "Unknown", "x86", "S390", "Power", "IA64", "S390X", "AMD64"};
        String[] SubTypes   = { "i486", "i586", "Pentium II", "Pentium III",
            "Merced","McKinley", "PowerRS", "PowerPC", "GigaProcessor", "ESA", "Pentium IV", "T-Rex", "Opteron"};
        String[] trCounter  = { "Sequence Counter", "Special", "RDTSC Timer", "AIX Timer",
                "MFSPR Timer", "MFTB Timer", "STCK Timer", "J9 timer"};
        out.write("Sys Processor Info :");
        out.newLine();
        out.write((Util.SUM_TAB + "Arch family "+Archs[arch]));
        out.newLine();
        out.write((Util.SUM_TAB + "Processor Sub-type " + SubTypes[subType]));
        out.newLine();
        out.write((Util.SUM_TAB + "Num Processors " +procs));
        out.newLine();
        out.write((Util.SUM_TAB + "Big Endian " + big));
        out.newLine();
        out.write((Util.SUM_TAB + "Word size " + word));
        out.newLine();
        out.write((Util.SUM_TAB + "Using Trace Counter " + trCounter[counter]));
        out.newLine();
        out.newLine();
    }
}
