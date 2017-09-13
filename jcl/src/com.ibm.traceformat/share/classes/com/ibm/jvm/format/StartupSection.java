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
import java.util.*;

/** 
 * Startup section of a file header. 
 *
 * @author Tim Preece
 */
public class StartupSection {

    private                String    eyecatcher_string;
    private                int       length;
    private                int       version;
    private                int       modification;
    private                long      options_offset;

    private                long      options_end;
    private                TraceFile traceFile;
    private                String[] startupOptions;

    public StartupSection (TraceFile traceFile, int start ) throws IOException
    {
        // Version 1.1
        this.traceFile = traceFile;
        traceFile.seek((long)start);
        eyecatcher_string  =  Util.convertAndCheckEyecatcher(traceFile.readI());
        length        = traceFile.readI();
        version       = traceFile.readI();
        modification  = traceFile.readI();
        options_offset  = traceFile.getFilePointer();

        options_end = start + length;

        Util.Debug.println("StartupSection: eyecatcher:        " + eyecatcher_string);
        Util.Debug.println("StartupSection: length:            " + length);
        Util.Debug.println("StartupSection: version:           " + version);
        Util.Debug.println("StartupSection: modification:      " + modification);
        
        startupOptions = readOptions(traceFile, options_offset);
    }
    
    private String[] readOptions(TraceFile traceFile, long options_offset) throws IOException{
    	String opts[] = null;
    	Vector optsVector = new Vector();
        traceFile.seek(options_offset);
        byte[] optionsBuffer = new byte[(int)(options_end - options_offset)];
        Util.Debug.println("StartupSection: options_offset:        " + options_offset);
        Util.Debug.println("StartupSection: options_end:           " + options_end);
        StringBuffer buf;
        int i = 0;
        traceFile.read(optionsBuffer);
        // Util.printDump(optionsBuffer,optionsBuffer.length);
        for (i=0; (i<optionsBuffer.length) ; i++) {
            buf = new StringBuffer();
            for ( ; (optionsBuffer[i]!=0) ; i++) {              // null terminates each string
                buf.append((char)optionsBuffer[i]);
            }
            if (buf.length()==0) {
                break;                                          // a null terminates the list
            }
            String newOpt = buf.toString();
            optsVector.add(newOpt);
        }
        opts = new String[optsVector.size()];
        for (i=0; i < optsVector.size(); i++){
        	opts[i] = (String)optsVector.elementAt(i);
        }
        return opts;
    }



    protected void summary(BufferedWriter out) throws IOException
    {
        out.write("JVM Start-up Params :");
        out.newLine();

        for (int i = 0; i < startupOptions.length; i++){
            out.write(startupOptions[i]);
            out.newLine();
        }
        out.newLine();
    }
    
    protected String[] getStartUpOptions(){
    	return startupOptions;
    }
}
