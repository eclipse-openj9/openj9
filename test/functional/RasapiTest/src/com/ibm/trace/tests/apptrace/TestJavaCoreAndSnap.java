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

/*
 * Compare the current thread trace from a javacore and a snap trace file
 * (formatted to only contain that thread) to make sure the data is the same.
 * 
 * The javacore and snap dump should have been taken as close together as possible.
 */
import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.LinkedList;
import java.util.List;

public class TestJavaCoreAndSnap { 

    private static final String XEHSTTYPE = "3XEHSTTYPE     ";

	public static void main(String[] args) {

        System.out.println("TestJavaCoreAndSnap: started");

        if( args.length != 2 ) {
        	System.err.println("Expected two arguments, javacore and formatted snap trace.");
        	exit(1);
        }

        String javacoreFileName = args[0];
        String traceFileName = args[1];
        
        System.out.printf("Comparing current thread trace in %s and %s\n", javacoreFileName, traceFileName);
        
        String[] jcTrace = extractJCTrace(javacoreFileName);
        
        String lastTPid = getTracePointId(jcTrace[0]);
        
        //System.err.println("Last tpid is: " + lastTPid);
        
        String[] snapTrace = extractFormattedTrace(traceFileName);
        
        int jcTraceIndex = 0;
        int snapTraceIndex = 0;
        boolean found = false;
        
        /* The test expects the snap trace to have been taken after the javacore.
         * To enable us to match the trace points up skip the first few tracepoints in the
         * snap file until we get to the one that mentions the javacore (and which should be
         * the last trace point in the javacore).
         */
        for( ; snapTraceIndex < snapTrace.length; snapTraceIndex++) {
        	//System.err.println("Checking id: " + getTracePointId(snapTrace[snapTraceIndex]) + " from " + snapTrace[snapTraceIndex]);
        	if( "j9dmp.9".equals(getTracePointId(snapTrace[snapTraceIndex])) &&
        			snapTrace[snapTraceIndex].contains(javacoreFileName) ) {
        		found = true;
        		//System.err.println("snapTraceIndex is: " + snapTraceIndex);
        		break;
        	}
        }
        
        if( found == false ) {
        	System.err.println("Couldn't match start of trace in java core to start of trace in snap.");
        	exit(2);
        }
        
        for( ; snapTraceIndex <  snapTrace.length && jcTraceIndex < jcTrace.length; snapTraceIndex++, jcTraceIndex++) {
        	String snapTraceLine = snapTrace[snapTraceIndex];
        	String jcTraceLine = jcTrace[jcTraceIndex];
        	String snapTraceID = getTracePointId(snapTraceLine);
        	String jcTraceID = getTracePointId(jcTraceLine);
        	if( !snapTraceID.equals(jcTraceID) ) {
        		System.err.println("Trace point lines did not match:" + jcTraceID + " vs " + snapTraceID );
        		System.err.println("Javacore: " + jcTraceLine);
        		System.err.println("Snap trace: " + snapTraceLine);
        		
        		exit(3);
        	} else {
//        		System.err.println("Trace point lines matched:" + jcTraceID + " vs " + snapTraceID );
        	}
        	// Checking the content would be good *but* things like number formatting (with or without leading
        	// 0's) make it impractical even if we ignore the case.
/*        	String snapTraceDetail = getTracePointDetail(snapTraceLine);
        	String jcTraceDetail = getTracePointDetail(jcTraceLine);
        	if( !snapTraceDetail.equalsIgnoreCase(jcTraceDetail) ) {
        		System.err.println("Trace point lines did not match:" + jcTraceID + " vs " + snapTraceID );
        		System.err.println("Javacore: '" + jcTraceDetail + "'");
        		System.err.println("Snap trace: '" + snapTraceDetail + "'");
        		
        		exit(3);
        	} else {
        		System.err.println("Trace point lines matched:" + jcTraceDetail + " vs " + snapTraceDetail );
        	}*/
        }
        
        exit(0);
        
    }

    private static void exit(int code) {
    	if( code == 0 ) {
    		System.out.println("TestJavaCoreAndSnap: passed");
    	} else {
    		System.out.println("TestJavaCoreAndSnap: failed");
    	}
    	System.out.println("TestJavaCoreAndSnap: finished");
    	System.exit(code);
    }
    
    private static String[] extractJCTrace(String javacoreFileName) {

    	BufferedReader in = null;
    	List<String> traceLines = new LinkedList<String>();
    			
    	try {
    		try {
    			in = new BufferedReader(new FileReader(javacoreFileName));
    		} catch (FileNotFoundException e) {
    			System.err.println("File: " + javacoreFileName + " not found.");
    			return null;
    		}

    		String str;
    		try {
    			while( (str = in.readLine()) != null ) {
    				if( str.startsWith(XEHSTTYPE) ) {
    					traceLines.add(str.substring(XEHSTTYPE.length()));
    				}
    			}
    		} catch (IOException e) {
    			System.err.println("Failed to read from file: " + javacoreFileName);
    			return null;
    		}
    	} finally {
    		try {
    			in.close();
    		} catch (IOException e) {
    			// Not much we can do about this.
    		}
    	}
    	
    	return traceLines.toArray(new String[0]);

    }

    private static String[] extractFormattedTrace(String traceFileName) {

    	BufferedReader in = null;
    	List<String> traceLines = new LinkedList<String>();
    			
    	try {
    		try {
    			in = new BufferedReader(new FileReader(traceFileName));
    		} catch (FileNotFoundException e) {
    			System.err.println("File: " + traceFileName + " not found.");
    			return null;
    		}

    		String str;
    		try {
    			while( (str = in.readLine()) != null ) {
    				if( str.startsWith("Time") ) {
    					// The next line is the start of the actual trace,
    					// we're past the header information.
    					break;
    				}
    			}
    			while( (str = in.readLine()) != null ) {
    				// Add these to the start of the list to reverse them into the same order as
    				// the ones from javacore.
    				traceLines.add(0, str);
    			}
    		} catch (IOException e) {
    			System.err.println("Failed to read from file: " + traceFileName);
    			return null;
    		}
    	} finally {
    		try {
    			in.close();
    		} catch (IOException e) {
    			// Not much we can do about this.
    		}
    	}
    	
    	return traceLines.toArray(new String[0]);

    }
    
    // This works for both javacore trace and snap trace because javacore has the timezone in
    // position 1 and formatted snap trace has the thread id in the same column.
    private static String getTracePointId(String traceLine) {
    	return traceLine.split("\\s+")[2];
    }
    
    private static String getTracePointDetail(String traceLine) {
    	return traceLine.split("\\s+",5)[4].trim();
    }
}

