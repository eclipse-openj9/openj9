/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

import java.math.BigInteger;
import java.util.Iterator;
import java.util.Vector;

/**  
 * Vector containing all trace records for a specific thread.
 *
 * @author Tim Preece
 */
final public class TraceThread extends Vector implements com.ibm.jvm.trace.TraceThread {

    protected                     long           threadID      =     0;
    protected                     String         threadName;

    private                       TracePoint     tp = null;
    private                       boolean        primed = false;

    private                       TraceRecord50  currentTraceRecord = null;
    private                       int            currentIndent = 0;
    /** construct a new trace thread vector
     *
     * @param   ID ( hex value of threadID )
     * @param   threadName
     */
    protected TraceThread(long ID, String threadName)
    {
        super();
        this.threadID = ID;
        this.threadName = threadName;
    }

    public static int numBufs = 0;

    public static int getBuffersProcessed(){
        return numBufs;
    }

    private static synchronized void incrementBuffersProcessed(){
        numBufs++;
    }

    private synchronized void prime(){
        if (!primed) {
            popTopTraceRecord();
            primed = true;
        }
    }

    private void popTopTraceRecord(){        
        TraceRecord50 oldTraceRecord = currentTraceRecord;
        if (isEmpty()){
        	Util.Debug.println("last trace record popped from trace thread");
            Util.Debug.println("TraceThread " + Util.formatAsHexString( threadID ) + " emptied");
            currentTraceRecord = null;
            return;
        }
        currentTraceRecord = (TraceRecord50) elementAt( 0 );
        byte[] extraData = null;
        BigInteger lastUpperWord = null;
        if (oldTraceRecord != null) {
            lastUpperWord = oldTraceRecord.getLastUpperWord();
            extraData = oldTraceRecord.getExtraData();
        }
        while ( currentTraceRecord != null && currentTraceRecord.isMiddleOfTracePoint() ) {
            byte[] temp = null;
            byte[] current = extraData;
            
            Util.Debug.println("\nTraceThread has found a pure middle of tracepoint buffer\n");                

            temp = currentTraceRecord.getExtraData();
            Util.Debug.println("It's on thread " + Util.formatAsHexString(currentTraceRecord.getThreadIDAsLong()) );
            Util.printDump(temp, temp.length );
            if (extraData == null) {
                Util.Debug.println("Adding the middle in - temp.length " + temp.length);
                extraData = new byte[temp.length];
                System.arraycopy( temp, 0, extraData, 0, temp.length);
            } else {
                extraData = new byte[ current.length + temp.length];
                Util.Debug.println("Adding the middle in - current " + current.length + " temp.length " + temp.length);
                System.arraycopy(current, 0, extraData, 0, current.length );
                System.arraycopy(temp, 0, extraData, current.length, temp.length);
            }

            if (size() > 1) {
            	removeElementAt(0);
            	incrementBuffersProcessed();
            	currentTraceRecord = (TraceRecord50) elementAt( 0 );
            } else {
            	currentTraceRecord = null;
            }
        }
        if ( currentTraceRecord != null ) {
            if (extraData != null) {
                currentTraceRecord.addOverspillData( extraData, lastUpperWord );
            } 
            tp = currentTraceRecord.getNextTracePoint();
            while ((tp == null) && (size() > 1)) {
                /* skip e.g. empty buffers or buffers containing only control points */                                    
                removeElementAt( 0 );            
                incrementBuffersProcessed();
                currentTraceRecord = (TraceRecord50) elementAt( 0 );                    
                if ( currentTraceRecord != null ) {
                    tp = currentTraceRecord.getNextTracePoint();
                }                    
            }                
        }
        /* remove it so it can be taken off the heap - by this time it will have been heavily populated with data! */
        removeElementAt( 0 );            
        incrementBuffersProcessed();
        return;        
    }

    public TracePoint getNextTracePoint(){
        TracePoint ret = tp;
        if (!primed) {
            prime();
        }
        if (currentTraceRecord != null) {
            tp = currentTraceRecord.getNextTracePoint();
            if (tp == null) {
                /* currentTraceRecord is exhausted */
                popTopTraceRecord();
            }
        } else {
            tp = null;
        }
        return ret;
    }

    public int getIndent(){
        return currentIndent;
    }

    public void indent(){
        currentIndent++;
    }

    public void outdent(){
        currentIndent--;
        if (currentIndent < 0) {
            currentIndent = 0;
        }
    }

    /*
     * return the timestamp of the next tracepoint in the buffer, or null if there is no tracepoint
     */
    public BigInteger getTimeOfNextTracePoint(){
        if (!primed) {
            prime();
        }
        if (tp != null) {
            return tp.getRawTimeStamp();
        } else {
            /* occasionally we get duped by a corrupt or empty trace record
               this clause will pick those instances up */
            if (size() > 0) {
                popTopTraceRecord();
                if (tp != null) {
                    return tp.getRawTimeStamp();
                } /* else fall through to return null below */
            }
            return null;
        }
    }  
    
    /* methods implementing the com.ibm.jvm.trace.TraceThread interface */
    public Iterator getChronologicalTracePointIterator(){
    	return new com.ibm.jvm.trace.TracePointThreadChronologicalIterator(this);
    }
    
	public String getThreadName(){
		return threadName;
	}
	public long getThreadID(){
		return threadID;
	}
}
