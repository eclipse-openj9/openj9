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
 * Service section of a file header. 
 *
 * @author Tim Preece
 */
public class ServiceSection {

    private                String    eyecatcher_string;
    private                int       length;
    private                int       version;
    private                int       modification;
    private                long      level_offset;
    private                String    serviceString;

    private                TraceFile traceFile;

    public ServiceSection (TraceFile traceFile, int start ) throws IOException
    {
        int temp = 0;
        // Version 1.1
        this.traceFile = traceFile;
        traceFile.seek((long)start);
        eyecatcher_string  =  Util.convertAndCheckEyecatcher(traceFile.readI());
        length        = traceFile.readI();
        version       = traceFile.readI();
        modification  = traceFile.readI();
        level_offset  = traceFile.getFilePointer();

        serviceString = "";
        traceFile.seek(level_offset);
        while ((temp = traceFile.readByte()) != 0) {
            serviceString = serviceString + (char)temp;
        }

        Util.Debug.println("ServiceSection: eyecatcher:        " + eyecatcher_string);
        Util.Debug.println("ServiceSection: length:            " + length);
        Util.Debug.println("ServiceSection: version:           " + version);
        Util.Debug.println("ServiceSection: modification:      " + modification);
        Util.Debug.println("ServiceSection: string:            " + serviceString);

    }

    protected void summary(BufferedWriter out) throws IOException
    {
            Util.Debug.println("ServiceSection: level_offset:        " + level_offset);

            out.write("Service Level :");
            out.newLine();
            out.write(Util.SUM_TAB + serviceString);
            out.newLine();
            out.newLine();
    }
    protected String formatFileName()
    {
    		/*
    		 * Legacy search for J9TraceFormat<vm_level>.dat files.
    		 */
            if (serviceString.indexOf(" IBM J9 ") != -1 ||
                serviceString.indexOf("Unknown version") != -1 ||    //ibm@77980
                TraceArgs.j9) {
            	/* this is a J9 dat file - they have the vm level encoded in the dat file name */
            	String[] vmLevelParts = getVMLevel().split("[.]");
            	String fileExtension = ""; /* not null! - see fileName below */
            	for (int i = 0; (vmLevelParts != null) && (i < vmLevelParts.length); i++){
            		fileExtension += vmLevelParts[i];
            	}
            	String fileName = "J9TraceFormat" + fileExtension + ".dat";
            	Util.Debug.println("ServiceSection: formatFile:  "+ fileName);
            	return fileName;
            }
            Util.Debug.println("ServiceSection: formatFile unknown");
            return null;
    }
    
    /**
     * Method to search forward through dat files. Trace is forward compatible,
     * so a newer JVM should always be able to format the trace from an older
     * JVM. This means that, for example, a 2.3 tracefile can use J9TraceFormat24.dat
     * correctly.
     * @param previous - the last filename tried
     * @return the next filename to try, or null if all options are exhausted
     */
    protected String getNextFormatFileName(String previous){
    	if (previous == null){
    		return null;
    	}
    	if (previous.equals("J9TraceFormat22.dat")){
    		return "J9TraceFormat23.dat";
    	} else if (previous.equals("J9TraceFormat23.dat")){
    		return "J9TraceFormat24.dat";
    	} else if (previous.equals("J9TraceFormat24.dat")){
    		return "J9TraceFormat25.dat";
    	} else if (previous.equals("J9TraceFormat25.dat")){
    		return "J9TraceFormat26.dat";
    	} else {
	    	return null;
	    }
    }
    
    protected String getServiceLevel()
    {
    	return serviceString;
    }
    
    protected String getVMLevel()
    {
    	if (serviceString.indexOf("2.2") != -1){
    		return "2.2";
    	}
    	if (serviceString.indexOf("2.3") != -1){
    		return "2.3";
    	}
    	if (serviceString.indexOf("2.4") != -1){
    		return "2.4";
    	}
    	if (serviceString.indexOf("2.5") != -1){
    		return "2.5";
    	}
    	if (serviceString.indexOf("2.6") != -1){
    		return "2.6";
    	}
    	return "unknown";
    }
}
