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

import java.io.IOException;
import java.math.BigInteger;
import java.util.*;

/**  
 * Merges the trace entries
 *
 * @author Tim Preece
 */
final public class Merge {

    private Vector      threads;
    private TraceRecord currentTraceRecord=null;
    private BigInteger  nextOldest;
    private LinkedList  traceRecordList = new LinkedList();
    private boolean     oneThreadLeft = false;
    private int         numberOfRecordsProcessed = 0;

    /** construct the Merge object
     *
     * @param   List of threads with trace records to merge
     */
    protected Merge(Vector threads) throws IOException
    {
        this.threads = threads;

        // prime the oldest Trace Record in each thread
        TraceThread traceThread;
        TraceRecord traceRecord;
        for (Iterator t=threads.iterator(); t.hasNext();) {
            traceThread = (TraceThread)t.next();
            traceRecord = (TraceRecord)traceThread.firstElement();
            traceRecord.prime();
            traceRecordList.add(traceRecord);
        }
        TraceFormat.outStream.println("Number of Trace Buffers Processed:");
        TraceFormat.outStream.print("     0 ");

        getCurrentTraceRecordAndUpdateNextOldest();
        Util.Debug.println("Merge: nextOldest after constructor " + nextOldest);
    }

    /** get the current Trace Record and update the nextOldest.
     *
     * @param   void
     * @return  void
     */
    final protected void getCurrentTraceRecordAndUpdateNextOldest()
    {
        Collections.sort(traceRecordList);
        currentTraceRecord = (TraceRecord)traceRecordList.get(0);
        if (traceRecordList.size() > 1) {
            nextOldest =  ((TraceRecord)traceRecordList.get(1)).getCurrentTimeStamp();
        }
        else {
            nextOldest = BigInteger.ZERO;
            oneThreadLeft = true;
        }
        return ;
    }

    /** get the next trace entry ( the oldest )
     *
     * @param   void
     * @return  the formatted trace entry ( null means we are finished )
     */
    final protected String getNextEntry() throws IOException
    {
        BigInteger  timeStamp;      // timeStamp of the next entry
        TraceRecord traceRecord;

        while (currentTraceRecord.getNextEntry() == 0) {  // 0 means we have no more entries on this record
            Util.Debug.println("Merge: no more entries in this Record");

            numberOfRecordsProcessed++;
            if (numberOfRecordsProcessed%10 == 0 ||
                numberOfRecordsProcessed == TraceFormat.expectedRecords)
            {
                StringBuffer tempBuffer = new StringBuffer(Integer.toString(numberOfRecordsProcessed));
                Util.padBuffer(tempBuffer, 6, ' ', false);    // right justify - field width 6 (at least)
                TraceFormat.outStream.print(tempBuffer+" ");
            }

            if ((numberOfRecordsProcessed+10)%100 == 0) {     // throw new line BEFORE the 100 multiples
                TraceFormat.outStream.println("");
            }

            currentTraceRecord.release();                     // release the large buffer for GC
            traceRecordList.remove(currentTraceRecord);       // remove Record from list of current Records
            traceRecord = currentTraceRecord.getNextRecord(); // get this threads next record
            if (traceRecord != null ) {                       // if we have one ....
                Util.Debug.println("Merge: priming next Record on this thread");
                traceRecord.prime();                          // prime it ....
                traceRecordList.add(traceRecord);             // and add to list of current Records
            }

            if (traceRecordList.size() == 0 ) {               // if current record list is empty .....
                Util.Debug.println("Merge: Finished");
                TraceFormat.outStream.println(" ");
                return null;                                  // we have finished
            }
            getCurrentTraceRecordAndUpdateNextOldest();       // update currentTraceRecord and nextOldest
        }
        timeStamp = currentTraceRecord.getCurrentTimeStamp();

        //Util.Debug.println("Merge: timeStamp  = " + timeStamp);
        //Util.Debug.println("Merge: nextOldest = " + nextOldest);
        //Util.Debug.println("Merge: compareTo  = " + timeStamp.compareTo(nextOldest));

        // is current TimeStamp older than cached ( nextOldest )
        if (timeStamp.compareTo(nextOldest) != -1 && oneThreadLeft == false ) {
            //Util.Debug.println("Merge: swapping records      " );
            getCurrentTraceRecordAndUpdateNextOldest();
            //Util.Debug.println("Merge: nextOldest after swap " + nextOldest);
            currentTraceRecord.getNextEntry();
        }

        return currentTraceRecord.formatCurrentEntry();
    }
}
