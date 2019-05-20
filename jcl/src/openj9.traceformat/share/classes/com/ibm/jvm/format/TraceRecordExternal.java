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

/** 
 * Extends Trace Record. Special processing needed for external trace records.
 *
 * @author Tim Preece
 */
final public class TraceRecordExternal extends TraceRecord {

    protected                 byte[]      spannedEntrySoFar;
    protected                 byte[]      spanEntry;
    private                   int         spannedEntrySizeSoFar = 0;

    protected TraceRecordExternal(TraceFile traceFile, long start)  throws IOException
    {
        super(traceFile, start);
    }

    /**
     *  reads in the record data for this TraceRecord
     *
     */
    protected void prime() throws IOException
    {
        // Allow room for the all trace entries including the spanned record
        buffer = new byte[bufferSize-headerSize+spannedEntrySizeSoFar];
        int oldestEntry;
        int length;
        int     longEntryID      = 0;
        int     longEntryLength  = 0;
        int     entry; // running pointer to each trace entry in buffer.

        // construct the buffer. The entry starts off as the next available slot.
        entry = (int)(nextEntry-headerSize+spannedEntrySizeSoFar);
        if ( spannedEntrySizeSoFar > 0 ) {
            System.arraycopy(spannedEntrySoFar,0,buffer,0,spannedEntrySizeSoFar);
        }
        traceFile.seek((long)(start+headerSize)); // skip over the header
        traceFile.read(buffer,spannedEntrySizeSoFar,bufferSize-headerSize);

        Util.Debug.println(" ");
        Util.Debug.println("TraceRecord: reading buffer size=:        " + bufferSize);
        Util.Debug.println("TraceRecord: nextEntry        :           " + (int)nextEntry);

        if (nextEntry < 0) {
            Util.Debug.println("TraceRecord: entry spans entire record");
            // anything left is part of a spanned record and needs to be copied to the next record
            copySpannedEntryForNextRecord(buffer,0,bufferSize-headerSize+spannedEntrySizeSoFar);
            return;
        } else {
            Util.Debug.println("TraceRecord: buffer[nextEntry]:           " + buffer[entry]);
        }


        //Util.printDump(buffer,bufferSize-headerSize+spannedEntrySizeSoFar);

        // anything left is part of a spanned record and needs to be copied to the next record
        copySpannedEntryForNextRecord(buffer,entry,bufferSize-headerSize+spannedEntrySizeSoFar);

        // patch up buffer so we can process it in time order
        // does not allow for > 256 entries
        int entryLength = Util.constructUnsignedByte(buffer,entry);
        int temp;
        while (entry != 0) {
            entry = entry - entryLength; // step back to previous entry
            //Util.Debug.println("TraceBuffer: entry = " + entry + " " + entryLength );

            // Since we now prepend the spanned buffer entry will always end at 0 unless it's a
            // partial trace entry in the very first buffer and has no span continuation
            if (entry < 0 ) {
                  Util.Debug.println("entry < 0 must be a partial entry in first record");
                  Util.Debug.println("spannedEntrySizeSoFar " + spannedEntrySizeSoFar);
                  if ( spannedEntrySizeSoFar != 0 ) {
                     throw new InvalidSpannedRecordException("Invalid spanned trace record");
                  }
                  entry = entry + entryLength; // step back to first complete entry
                  break;
            }

            // if we have a time wrap add it to the stack
            if ( entryLength == 8 ) {
                // If we have a time wrap and it is not the first entry
                // in the buffer
                if ( entry != 0
                     && buffer[entry+3]==0
                     && buffer[entry+2]==0
                     && buffer[entry+1]==0 ) { // trying to be quick, 3 bytes zero means time wrap
                    // since we will read the records forward swap this time with the calculated upperWord
                    //upperWord = Util.constructUnsignedLong(buffer, entry + TIMESTAMP_OFFSET, Util.INT).shiftLeft(32);
                    wrapTimes.push(upperWord);
                    upperWord = Util.constructUnsignedLong(buffer, entry + TIMESTAMP_OFFSET, Util.INT);
                    Util.Debug.println("TraceBuffer: timewrap entry =" +entry+ " upperWord=" + upperWord);
                }
            }

            // deal with the case the last entry was a special >256 entry
            // we have to copy the entire trace entry because entry contains the length
            // we have to push the original trace entry to cope with spanned buffers
            // if we have a > 256 entry lets deal with it
            if ( entryLength == 4 ) {
                if ( buffer[entry+2]==0 && buffer[entry+1]==0 ) {
                     Util.Debug.println("Entry with data length > 256");

                     // remember the ID and length of the special entry
                     longEntryID     = buffer[entry+3];
                     longEntryLength = Util.constructUnsignedByte(buffer,entry);

                     if ((entry - (longEntryLength + longEntryID*256)) < 0) {
                         Util.Debug.println("entry < 0 must be a partial entry in first record");
                         Util.Debug.println("spannedEntrySizeSoFar " + spannedEntrySizeSoFar);
                         if ( spannedEntrySizeSoFar != 0 ) {
                            throw new InvalidSpannedRecordException("Invalid spanned trace record");
                         }
                         entry = entry + entryLength; // step back to first complete entry
                         break;
                     }

                     buffer[entry]   = (byte)entryLength; // in the end this is not used

                     // step back to previous entry
                     entry = entry - (longEntryLength + longEntryID*256);

                     // push the real TraceID and copy in the special trace entry
                     longEntryTraceIDs.push(Integer.valueOf(Util.constructTraceID(buffer, entry + 1)));
                     buffer[entry+1]=0;
                     buffer[entry+2]=0;
                     buffer[entry+3]=(byte)longEntryID;
                     entryLength = longEntryLength;
                }
            }

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

            // patch up the lengths of the entries.
            temp = Util.constructUnsignedByte(buffer,entry);
            buffer[entry] = (byte)entryLength;
            entryLength = temp;
            //Util.Debug.println("entry " + entry + " now " + (byte)buffer[entry]);

        }

        // If the first record is a timewrap, ignore it as it belongs
        // to the previous record
        if (buffer[entry]  == 8   &&
            buffer[entry+1]==0 &&
            buffer[entry+2]==0 &&
            buffer[entry+3]==0 ) {
            entry += 8;
            Util.Debug.println("TraceBuffer: ignoring time wrap at offset 0");
        }
        offset = entry;  // this is the first ( oldest) entry in this Record
        currentTimeStamp = upperWord.shiftLeft(32).or(Util.constructUnsignedLong(buffer, entry + TIMESTAMP_OFFSET, Util.INT));
        //currentTimeStamp = Util.constructUnsignedLong(buffer, entry + TIMESTAMP_OFFSET, Util.INT);
        Util.Debug.println("TraceRecord: oldest offset for thread "+threadID+" = " + offset + " timeStamp = " + currentTimeStamp);
        //Util.printDump(buffer,bufferSize-headerSize+spannedEntrySizeSoFar);
        return;
    }

    /**  Get the next Trace Entry
     *   return 0=no more entries 1=last entry not yet formatted
     */
    protected int getNextEntry() throws IOException
    {
        int rc;
        if (notFormatted) {
            return 1;
        }
        notFormatted = true;

        do {
            //Util.Debug.println(" ");
            //Util.Debug.println("TraceRecordExternal: getNextEntry >>>>>>>>>>>>>>>>>>>>>>>>>>");
           if (nextEntry < 0 ||
               offset >= nextEntry-headerSize+spannedEntrySizeSoFar) {
               // Util.Debug.println("TraceRecord: no more entries");
               rc = 0;
           } else {
               rc = processNextEntryHeader(buffer,offset);
               if ( currentLength >= 256 ) offset += 4; // step over excess entry
               offset += currentLength;    // for next entry
           }
        } while ( rc == 2 );
        //Util.Debug.println("TraceRecordExternal: getNextEntry <<<<<<<<<<<<<<<<<<<<<<<<<< " +rc);
        //Util.Debug.println(" ");
        return rc;
    }

    /**
     *  This is called from previous TraceRecord and contains the start of the spanned entry
     *
     */
    final protected void spanStart(byte[] startofSpanEntry, int startofSpanEntrySize)
    {
        //Util.Debug.println("spanStart: startofSpanEntrySize " + startofSpanEntrySize);
        spannedEntrySoFar     = startofSpanEntry;
        spannedEntrySizeSoFar = startofSpanEntrySize;
        //Util.Debug.println("spanStart: spannedEntrySizeSoFar " + spannedEntrySizeSoFar);
    }

    /** The end of our Trace Record is copied to the next record
     *
     *
     */
    final protected void copySpannedEntryForNextRecord(byte[] buffer,int start,int end)
    {
        //Util.Debug.println("copySpannedEntryForNextRecord: start end " + start + " " + end);
        TraceRecordExternal nextTraceRecord=(TraceRecordExternal)getNextRecord();
        if (nextTraceRecord == null) {
            return; // no need to save as we are the last Trace Record for this thread
        }
        byte[] startofSpanEntry = new byte[end-start];
        System.arraycopy(buffer,start,startofSpanEntry,0,end-start);
        // pass this to the next TraceRecord for this thread
        //Util.Debug.println("spanStart: end-start " + (end-start));
        nextTraceRecord.spanStart(startofSpanEntry,end-start);

        // Need to check for a timewrap record that spans into the
        // next record, as it applies to this record.
        if (end - start < 8) {
            byte[] next9 = new byte[9];
            System.arraycopy(buffer, start, next9, 0, end-start);
            System.arraycopy(nextTraceRecord.nextEight, 0, next9,
                             end-start, 9 - (end - start));
            if (next9[8] == 8 &&
                next9[1] == 0 &&
                next9[2] == 0 &&
                next9[3] == 0 ) {
                upperWord = Util.constructUnsignedLong(next9, TIMESTAMP_OFFSET, Util.INT);
                Util.Debug.println("TraceBuffer: spanned timewrap entry=" +start+ " upperWord=" + upperWord);
            }
        }
        return;
    }

}
