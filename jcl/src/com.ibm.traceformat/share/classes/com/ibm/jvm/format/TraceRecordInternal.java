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

import java.io.IOException;
import java.math.BigInteger;

/** 
 * Extends Trace Record. Special processing needed for internal trace records.
 *
 * @author Tim Preece
 */
final public class TraceRecordInternal extends TraceRecord {

    private int     lastEntry;

    protected TraceRecordInternal(TraceFile traceFile, long start)  throws IOException
    {
        super(traceFile, start);
    }

    /**
     *  reads in the record data for this TraceRecord
     *
     *
     */
    final protected void prime() throws IOException
    {
        buffer = new byte[bufferSize - headerSize];
        int oldestEntry;
        int length;
        int longEntryID = 0;
        int longEntryLength = 0;
        boolean newestEntry = true;
        boolean checkEntries = false;
        long lastTime = Long.MAX_VALUE;

        /*
         *  If the buffer has wrapped, read in the older data first
         */
        if ((bufferSize - nextEntry) > 1) {
            traceFile.seek((long)(start + nextEntry + 1)); // skip to the oldest part of the buffer
            traceFile.read(buffer, 0, bufferSize - (int)nextEntry - 1);
        }
        traceFile.seek((long)(start + headerSize));
        traceFile.read(buffer, bufferSize - (int)nextEntry - 1, (int)nextEntry - headerSize + 1);
        int entry = (int)bufferSize - headerSize - 1;

        Util.Debug.println("TraceBuffer: reading buffer size=:        " + (bufferSize - headerSize));
        Util.Debug.println("TraceBuffer: nextEntry        :           " + (int)nextEntry);
        Util.Debug.println("TraceBuffer: buffer[nextEntry]:           " + buffer[entry]);
        Util.Debug.println("TraceBuffer: File offset:                 " + start);

        // Util.printDump(buffer,bufferSize - headerSize);

        // patch up buffer so we can process it in time order
        int entryLength = Util.constructUnsignedByte(buffer,entry);
        int temp = 0;
        while (true) {
            entry = entry - entryLength;      // step back to previous entry
            // Util.Debug.println("TraceBuffer: entry = " + entry + " " + entryLength );

            if (entry < 0 ) {                 // Top of buffer ?
                entry = entry + entryLength;  // step back up to first entry in record
                break;                        // our exit point from this loop
            }

            // if we have a time wrap add it to the stack.
           if ( entryLength == 8 ) {
                if ( buffer[entry+3]==0
                     && buffer[entry+2]==0
                     && buffer[entry+1]==0 ) {  // trying to be quick 3 bytes zero means time wrap
                                                // since we will read the records forward swap this time with the calculated upperWord
                                                //upperWord = Util.constructUnsignedLong(buffer, entry + TIMESTAMP_OFFSET, Util.INT).shiftLeft(32);
                    wrapTimes.push(upperWord);
                    upperWord = Util.constructUnsignedLong(buffer, entry + TIMESTAMP_OFFSET, Util.INT);
                    Util.Debug.println("TraceBuffer: timewrap entry=" +entry+ " upperWord=" + upperWord);
                    lastTime = Long.MAX_VALUE;
                }
            }

            /*35363  starts..................*/
            if (entryLength == 4) {
               if (buffer[entry+2] == 0 && buffer[entry+1]==0) {
                  Util.Debug.println("Entry with data length>256");
                  longEntryID = buffer[entry+3];
                  longEntryLength = Util.constructUnsignedByte(buffer,entry);
                  if ((entry - (longEntryLength + longEntryID*256)) < 0) {
                      Util.Debug.println("entry < 0 must be a partial entry");
                      entry = entry + entryLength; // step back to first complete entry
                      break;
                  }
                  entry = entry - (longEntryLength + longEntryID*256);
                  longEntryTraceIDs.push(new Integer(Util.constructTraceID(buffer,entry+1)));
                  buffer[entry+1] = 0;
                  buffer[entry+2] = 0;
                  buffer[entry+3] = (byte)longEntryID;
                  entryLength = longEntryLength;
               }
            }

            if (newestEntry) {
                long lowerWord = Util.constructUnsignedLong(buffer, entry + TIMESTAMP_OFFSET, Util.INT).longValue();
                if (lowerWord != timeStamp.and(BigInteger.valueOf(0xffffffffL)).longValue()) {
                    checkEntries = true;
                    Util.Debug.println("Possible damage to first trace entry");
                    Util.Debug.println("Trace time = " + lowerWord + " header = " + timeStamp.and(BigInteger.valueOf(0xffffffffL)).longValue());
                }
                newestEntry = false;
            }

            if (checkEntries) {


                int traceId = Util.constructTraceID(buffer, entry + TRACEID_OFFSET);
                long lowerWord = Util.constructUnsignedLong(buffer, entry + TIMESTAMP_OFFSET, Util.INT).longValue();
                if (traceId > 256) {
		    /* if (tracefile level < 5.0){
                    	if (MessageFile.getMessageFromID(traceId) == null) {
                        	Util.Debug.println("Bad trace identifier: " + traceId);
	                        Util.Debug.println("Ignoring " + (entry + entryLength) + " bytes of the buffer");
        	                entry = entry + entryLength;
                	        break;
	                    }
		    } else {
		    	don't!
		    }*/
                    if (lowerWord > lastTime) {
                        Util.Debug.println("Bad trace timeStamp :" + lowerWord);
                        Util.Debug.println("Ignoring " + (entry + entryLength) + "bytes of the buffer");
                        entry = entry + entryLength;
                        break;
                    }
                    lastTime = lowerWord;
                }
            }

            /*35363  ends........................  */
            if (entryLength == 0 ) {
                // verify this is the first record!
                if(longEntryID == 0){
                    // We have reached a zero length record.
                    Util.Debug.println("TraceRecord: Hit 0 length entry");
                    break;
                } else {
                    // We have encountered a long record which is an exact multiple
                    // of 256 in length.  This is quite possible and acceptable.
                    // Do nothing.
                    Util.Debug.println("TraceRecord: 0 length entry "+
                                       "(long record length is exact multiple of 256)");
                }
             }

            temp = Util.constructUnsignedByte(buffer,entry);
            buffer[entry] = (byte)entryLength;
            entryLength = temp;
            // Util.Debug.println("entry " + entry + " now " + (byte)buffer[entry]);

        }

        offset = entry;  // this is the first ( oldest) entry in this Record
        currentTimeStamp = upperWord.shiftLeft(32).or(Util.constructUnsignedLong(buffer, entry + TIMESTAMP_OFFSET, Util.INT));
        //currentTimeStamp = Util.constructUnsignedLong(buffer, entry + TIMESTAMP_OFFSET, Util.INT);
        //Util.Debug.println("TraceBuffer: oldest offest = " + offset + " timeStamp = " + currentTimeStamp);
        //Util.printDump(buffer,bufferSize - headerSize);
        return;
    }

    /** Gets the next trace entry
     *  returns 0=no more entries in this buffer 1=ok to format
     *
     */
    final protected int getNextEntry() throws IOException
    {
        int rc;
        int endPoint;

        // Util.Debug.println("TraceRecord: currentLength "+currentLength);
        // Util.Debug.println("TraceRecord: offset        "+offset);

        if (notFormatted) {   // in case we haven't formatted the last entry yet.
            return 1;
        }
        notFormatted = true;

        do {
            //Util.Debug.println(" ");
            //Util.Debug.println("TraceRecordInternal: getNextEntry >>>>>>>>>>>>>>>>>>>>>>>>>>");
           if (offset >= bufferSize - headerSize - 1) {
               //Util.Debug.println("TraceRecord: no more entries");
               rc = 0;
           } else {
               rc = processNextEntryHeader(buffer,offset);
               if ( currentLength >= 256 ) offset += 4; // step over excess entry
               offset += currentLength;                // for next entry
           }
        } while ( rc == 2 );
        return rc;
    }

}

