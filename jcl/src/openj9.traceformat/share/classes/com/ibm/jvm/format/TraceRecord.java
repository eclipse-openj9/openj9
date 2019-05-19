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
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Stack;

/** 
 * The Trace Record corresponds to a runtime Trace Buffer, it is responsible for formatting 
 * individual trace entries.
 *
 * @author Tim Preece
 */
public class TraceRecord implements Comparable {

    /*ibm@52623-start*/
    // Class Variables
    protected   static        Hashtable   indentLevels;                        /*ibm@53159*/
    protected   static        long        lastThread;                          /*ibm@53159*/
    
    protected   static final byte        EVENT_TYPE       = 0;
    protected   static final byte        EXCEPTION_TYPE   = 1;
    protected   static final byte        ENTRY_TYPE       = 2;
    protected   static final byte        ENTRY_EXCPT_TYPE = 3;
    protected   static final byte        EXIT_TYPE        = 4;
    protected   static final byte        EXIT_EXCPT_TYPE  = 5;
    protected   static final byte        MEM_TYPE         = 6;
    protected   static final byte        MEM_EXCPT_TYPE   = 7;
    protected   static final byte        DEBUG_TYPE       = 8;
    protected   static final byte        DEBUG_EXCPT_TYPE = 9;
    protected   static final byte        PERF_TYPE        = 10;
    protected   static final byte        PERF_EXCPT_TYPE  = 11;
    protected   static final byte        ASSERT_TYPE      = 12;
    protected   static final byte        APP_TYPE         = 13;
    protected   static final byte        ERROR_TYPE         = 14;
    
    protected   static final String[]    Chars = new String[] {
    	"-",
    	">",
    	">",
    	"<",
    	"<",
    	" ",
    	" ",
    	" ",
    	" ",
    	" ",
    	" ",
    	"*",
    	" ",
    	"E"
    };
    
    protected   static final String[]    types = new String[] {
        "Event     ",
        "Exception ",
        "Entry     ",
        "Entry     ",
        "Exit      ",
        "Exit      ",
        "Mem       ",
        "Mem       ",
        "Debug     ",
        "Debug     ",
        "Perf      ",
        "Perf      ",
        "Assert    ",
        "AppTrace  ",
        "ERROR     "
    };


    // Class Constants
    protected   static final  StringBuffer BASE_INDENT      = new StringBuffer();
    protected   static final  String      TAB               = " ";
    protected   static final  int         TRACEID_OFFSET    = 1;
    protected   static final  int         TIMESTAMP_OFFSET  = 4;

    // Instance Variables
    protected                 byte[]      buffer              =  null;
    protected                 byte[]      nextEight           =  new byte[8];   //ibm@56594

    protected                 BigInteger  timeStamp;
    protected                 BigInteger  wrapTime;
    protected                 BigInteger  writePlatform = BigInteger.ZERO;
    protected                 BigInteger  writeSystem   = BigInteger.ZERO;
    protected                 long        threadID   = 0;
    protected                 long        threadSyn1 = 0; /*ibm@67471*/
    protected                 long        threadSyn2 = 0; /*ibm@67471*/
    protected                 String      threadName;
    protected                 long        nextEntry;

    private                   String      padding;
    private                   boolean     doIndent;
    private                   String      threadIDString;

    protected                 boolean     recordFinished = false;

    protected                 TraceThread traceThread = null;
    protected                 TraceFile   traceFile;
    protected                 int         bufferSize;
    protected                 long        start;
    protected                 int         offset;

    protected                 byte[]      currentBuffer;
    protected                 int         currentOffset;
    protected                 int         currentTraceID;
    protected                 int         currentLength;
    protected                 BigInteger  currentTimeStamp;
    private                   Message     currentMessage;
    private                   int         currentType;
    private                   String      currentComponent;

    protected                 BigInteger  upperWord         = BigInteger.ZERO;
    protected                 Stack       wrapTimes = new Stack();
    protected                 Stack       longEntryTraceIDs = new Stack();   /*ibm@26172*/
    protected                 boolean     notFormatted = false;
    private                   int         lastErrorRecord = -1;              /*ibm@50367*/

    //Set default size for old trace versions
    protected                 int         headerSize = 72;                   /*ibm@67471*/

    protected TraceRecord(TraceFile traceFile, long start)  throws IOException
    {
   		traceFile.seek(start);
        timeStamp     = traceFile.readBigInteger(8);
        wrapTime      = traceFile.readBigInteger(8);
        writePlatform = traceFile.readBigInteger(8);
        writeSystem   = traceFile.readBigInteger(8);
        threadID      = traceFile.readL();
        if (TraceFileHeader.isUTE()) {                                       /*ibm@67471*/
            threadSyn1    = traceFile.readL();                               /*ibm@67471*/
            threadSyn2    = traceFile.readL();                               /*ibm@67471*/
            headerSize    = traceFile.readI();                               /*ibm@67471*/
            nextEntry     = traceFile.readI();                               /*ibm@67471*/
            threadName    = traceFile.readString(headerSize - 64);           /*ibm@67471*/
        } else {                                                             /*ibm@67471*/
            threadName    = traceFile.readString(28);
            nextEntry     = traceFile.readI();
        }                                                                    /*ibm@67471*/
       traceFile.read(nextEight, 0, 8);                             //ibm@56594

        upperWord     = timeStamp.shiftRight(32);
        wrapTimes.push(upperWord);

        threadIDString = Util.formatAsHexString(threadID);

        Util.Debug.println("reading timeStamp     " + timeStamp);
        Util.Debug.println("reading wrapTime      " + wrapTime);
        Util.Debug.println("reading writePlatform " + writePlatform);
        Util.Debug.println("reading writeSystem   " + writeSystem);

        this.bufferSize    = traceFile.traceFileHeader.getBufferSize();
        this.traceFile     = traceFile;
        this.start         = start;
        this.currentTimeStamp = timeStamp; // for initial sort

        // Validity check the nextEntry field. Note that a value of -1                      ibm@51252
        // indicates that this record does not contain the start of a trace                 ibm@51252
        // entry. i.e it is a middle segment of a spanned entry                             ibm@51252
        if ( nextEntry >= 0 && (nextEntry < headerSize || nextEntry > bufferSize) ) {     /*ibm@51252*/
            Util.Debug.println("Invalid Buffer - nextEntry = " + nextEntry + ", headerSize = " + headerSize +", bufferSize = " + bufferSize ); //ibm@51252
            TraceFormat.invalidBuffers++;
            return;
        }

        // record if this is the lastest time but why not compare system
        if (writePlatform.compareTo(TraceFormat.lastWritePlatform) > 0) {
            Util.Debug.println("updating lastWritePlatform" + writePlatform);
            Util.Debug.println("updating lastWriteSystem  " + writeSystem);
            TraceFormat.lastWritePlatform = writePlatform;
            TraceFormat.lastWriteSystem   = writeSystem;
        }

        if (wrapTime.compareTo(TraceFormat.first) < 0) {
            TraceFormat.first = wrapTime;
        }
        if (timeStamp.compareTo(TraceFormat.last) > 0) {
            TraceFormat.last = timeStamp;
        }

        if (Integer.valueOf(Util.getProperty("POINTER_SIZE")).intValue() == 4) {
            padding = "00000000";
        } else {
            padding = "0000000000000000";
        }

        doIndent = TraceArgs.indent;

        Util.Debug.println("*********************************************************");
        Util.Debug.println("TraceBufferHeader: timeStamp : 0x" + timeStamp.toString(16));
        Util.Debug.println("TraceBufferHeader: threadID  : 0x" + Long.toString(threadID, 16));
        Util.Debug.println("TraceBufferHeader: threadName: " + threadName + "\n");
        Util.Debug.println("TraceBufferHeader: nextEntry : " + nextEntry);
        Util.Debug.println("*********************************************************");

        boolean     foundThreadID  =     false;
        Util.Debug.println("Processing Record Header");
        foundThreadID = false;

        if ( Util.findThreadID(Long.valueOf(threadID)) == false ) {
            return;
        }

        for (Iterator i=TraceFormat.threads.iterator(); i.hasNext();) {
            traceThread = (TraceThread)i.next();
            if (threadID == traceThread.threadID  && threadName.equals(traceThread.threadName)) { /*ibm@67471*/
                foundThreadID = true;
                Util.Debug.println("Found existing threadID " + threadID );
                break ;
            }
        }
        if (foundThreadID == false) {
            traceThread = new TraceThread(threadID,threadName);
            TraceFormat.threads.addElement(traceThread);
        }
        traceThread.addElement(this);
    }

    /** Initializes static variables.
     *
     *  <p>This is called each time the TraceFormatter is started, from the initStatics()
     *     method in TraceFormat.java</p>
     */                                                              /* added by ibm@53159 */
    final protected static void initStatics()
    {
        indentLevels      = null;
        lastThread        = 0;
    }

    /** returns the latest time stamp read so far
     *
     * @param   info
     * @return  a biginteger that is that timestamp
     */
    final protected BigInteger getCurrentTimeStamp()
    {
        return currentTimeStamp;
    }

    /** returns the latest time stamp read so far
     *
     * @param   info
     * @return  a biginteger that is that timestamp
     */
    final protected TraceRecord getNextRecord()
    {
        int next = traceThread.indexOf(this) + 1;

        if (next >= traceThread.size()) {
            return null; // not more records for this thread.
        } else {
           /* ibm@50367:
            * There is a defect (51252) which can cause a trace record to be written
            * out twice.  If the timestamps of the current and next record are
            * identical, then this must have happened.  This code is inserted to
            * skip over the repeated record.
            */
            BigInteger thisTimeStamp = ((TraceRecord)traceThread.elementAt(next-1)).timeStamp ; /*ibm@50367*/
            BigInteger nextTimeStamp = ((TraceRecord)traceThread.elementAt(next)).timeStamp ;   /*ibm@50367*/
                                                                                                /*ibm@50367*/
            if(thisTimeStamp.equals(nextTimeStamp))  {                                          /*ibm@50367*/
                /* Only issue message once per duplicate record */                              /*ibm@50367*/
                if(lastErrorRecord != next) {                                                   /*ibm@50367*/
                    lastErrorRecord = next;                                                     /*ibm@50367*/
                    TraceFormat.outStream.println("\nWARNING: duplicate trace record discarded "+  /*ibm@52623*/
                                       "(record "+(next+1)+                                     /*ibm@50367*/
                                       " for thread " + Util.formatAsHexString(threadID)+")");        /*ibm@50367*/
                    // One less record to expect...
                    TraceFormat.expectedRecords--;                                              /*ibm@52623*/
                }                                                                               /*ibm@50367*/
                                                                                                /*ibm@50367*/
               /*
                *  return the next but one record rather than next record.
                *  If it doesn't exist, return null.
                */
                if (next+1 >= traceThread.size()) {                                             /*ibm@50367*/
                    return null;    // last two were the same, so we've finished
                } else {                                                                        /*ibm@50367*/
                    return(TraceRecord)traceThread.elementAt(next+1);                           /*ibm@50367*/
                }
            } else {
                return(TraceRecord)traceThread.elementAt(next);
            }
        }
    }

    /**
     * sets up current* fields for later formatting
     */
    final protected int processNextEntryHeader(byte[] entry,int start) throws IOException
    {

        currentLength    = Util.constructUnsignedByte(entry, start);
        currentTraceID   = Util.constructTraceID(entry, start + TRACEID_OFFSET);

        /*
         *  Check for timestamp wrap. Ignore record if it is not 8 bytes long
         */
        if (currentTraceID == 0) {                                 /*ibm@38118*/
            if (currentLength == 8) {                              /*ibm@38118*/
                upperWord = (BigInteger)wrapTimes.pop();
                Util.Debug.println("TraceRecord: timewrap new upperWord=" + upperWord);
            }                                                      /*ibm@38118*/
            //notFormatted = false; // no need to print and format this one now never attempted
            return 2; // special
        } else {
            if ((currentTraceID == 256) && (currentLength == 8)) {   // special trace entry
                currentTimeStamp = wrapTime;
                Util.Debug.println("TraceRecord: lost records");
            } else {
                currentTimeStamp = upperWord.shiftLeft(32).or(Util.constructUnsignedLong(entry, start + TIMESTAMP_OFFSET, Util.INT));
            }
        }

        if (currentTraceID < 256) {                   /*ibm@26172*/
            Util.Debug.println("processing long record: start                   " + start);               /*ibm@26172*/
            Util.Debug.println("processing long record: previous currentLength  " + currentLength);       /*ibm@26172*/
            Util.Debug.println("processing long record: previous currentTraceID " + currentTraceID);      /*ibm@26172*/
            currentLength = currentLength + currentTraceID*256;                                           /*ibm@26172*/
            currentTraceID = ((Integer)longEntryTraceIDs.pop()).intValue();                               /*ibm@26172*/
            Util.Debug.println("processing long record: currentLength           " + currentLength);       /*ibm@26172*/
            Util.Debug.println("processing long record: currentTraceID          " + currentTraceID);      /*ibm@26172*/
        }

        if ( (currentMessage = MessageFile.getMessageFromID(currentTraceID)) == null ) {
            // Handle preformatted application trace records. Added by ibm@62941
            if (TraceFormat.messageFile != null) {
            	TraceFormat.messageFile.addMessage("ApplicationTrace");
            }
            if (currentLength == 8) {
                Util.Debug.println("TraceRecord: Adding unrecognized event message format for tracepoint: " + currentTraceID);
                TraceFormat.messageFile.addMessage(Util.formatAsHexString(currentTraceID) + " 1 01 1 N ApplicationTraceEntry \"Unrecognized tracepoint\"");
                currentMessage = TraceFormat.messageFile.getMessageFromID(currentTraceID);
            } else {
                switch (entry[start + 10]) {
                    case '>': {
                        Util.Debug.println("TraceRecord: Adding entry message format for tracepoint: " + currentTraceID);
                        TraceFormat.messageFile.addMessage(Util.formatAsHexString(currentTraceID) + " 2 01 1 N ApplicationTraceEntry \"%s\"");
                        currentMessage = TraceFormat.messageFile.getMessageFromID(currentTraceID);
                        break;
                    }
                    case '<': {
                        int exception = entry[start + 8] == '*' ? 1 : 0;
                        Util.Debug.println("TraceRecord: Adding exit message format for tracepoint: " + currentTraceID);
                        TraceFormat.messageFile.addMessage(Util.formatAsHexString(currentTraceID) + " " + (exception + 4) + " 01 1 N ApplicationTraceExit \"%s\"");
                        currentMessage = TraceFormat.messageFile.getMessageFromID(currentTraceID);
                        break;
                    }
                    case '-': {
                        Util.Debug.println("TraceRecord: Adding event message format for tracepoint: " + currentTraceID);
                        TraceFormat.messageFile.addMessage(Util.formatAsHexString(currentTraceID) + " 0 01 1 N ApplicationTraceEvent \"%s\"");
                        currentMessage = TraceFormat.messageFile.getMessageFromID(currentTraceID);
                        break;
                    }
                    case '*': {
                        Util.Debug.println("TraceRecord: Adding exception message format for tracepoint: " + currentTraceID);
                        TraceFormat.messageFile.addMessage(Util.formatAsHexString(currentTraceID) + " 1 00 0 N ApplicationTraceException \"%s\"");
                        currentMessage = TraceFormat.messageFile.getMessageFromID(currentTraceID);
                        break;
                    }
                    default: {
                        Util.Debug.println("TraceRecord: message is null ");
                        Util.Debug.println("TraceRecord: currentTraceID  " + currentTraceID);
                        Util.Debug.println("TraceRecord: currentLength   " + currentLength);
                        Util.Debug.println("TraceRecord: notFormatted    " + notFormatted);
                        Util.Debug.println("TraceRecord: start           " + start);           /*ibm@26172*/
                        Util.printDump(entry,start + currentLength);                           /*ibm@26172*/
                        TraceFormat.outStream.println(" ");                                    /*ibm@52623*/
                        TraceFormat.outStream.println("*** Invalid trace entry TraceID="+      /*ibm@52623*/
                                                      Util.formatAsHexString(currentTraceID)+" found in Trace Buffer");
                        return 0;
                        //System.exit(1);
                        //return -1; // is this an error?
                    }
                }
            }
        }
        currentType        =  currentMessage.getType();
        currentComponent   =  currentMessage.getComponent();
        currentBuffer      = entry;
        currentOffset      = start;

        //Util.Debug.println("TraceRecord: length             " + currentLength);
        //Util.Debug.println("TraceRecord: traceID            " + currentTraceID);
        //Util.Debug.println("TraceRecord: timeStamp          " + currentTimeStamp);
        //Util.Debug.println("TraceRecord: f(timeStamp)       " + Util.getFormattedTime(currentTimeStamp));
        //Util.Debug.println("TraceRecord: upperWord->32      " + upperWord.shiftLeft(32));
        //Util.Debug.println("TraceRecord: f(upperWord->32)   " + Util.getFormattedTime(upperWord.shiftLeft(32)));
        //Util.Debug.println("TraceRecord: start              " + start);
        //Util.Debug.println("TraceRecord: type               " + currentType);
        //Util.Debug.println("TraceRecord: component          " + currentComponent);

        if (currentLength == 0) {
            Util.Debug.println("TraceRecord: currentLength 0 start=" + start);      /*ibm@26172*/
            Util.printDump(entry,16);                                               /*ibm@26172*/
            TraceFormat.outStream.println("Internal Error");                        /*ibm@52623*/
            throw new IOException();
        }

        if (Util.findComponentAndType(currentComponent.toLowerCase(), types[currentType].toLowerCase()) == false) {
            return 2;
        }

        return 1;
    }

    /**
     *  return: the formatted trace entry.
     */
    final protected String formatCurrentEntry() throws IOException
    {

        boolean        threadSwitch      =  (threadID != lastThread);
        lastThread                       =  threadID; // lastThread is a static should go to merge?
        StringBuffer   indent            =  getIndent(threadIDString, doIndent);
        StringBuffer   sBuffer           =  new StringBuffer(Util.getFormattedTime(currentTimeStamp));
        String         entryData;
        String         stringId;

        try {                                                                                    /*ibm@28983*/
           if (currentTraceID == 256) {
               entryData         =  currentMessage.getMessage(currentBuffer, currentOffset + 4, currentOffset+8);
           } else {
               entryData         =  currentMessage.getMessage(currentBuffer, currentOffset + 8, currentOffset + currentLength); /*ibm@4945*/
           }
        } catch ( ArrayIndexOutOfBoundsException e ) {                                           /*ibm@28983*/
          // e.printStackTrace();                                                                /*ibm@28983*/
          // Util.Debug.println("TraceRecord - tep : currentOffset    " + currentOffset);        /*ibm@28983*/
          // Util.Debug.println("TraceRecord - tep : sBuffer          " + sBuffer.toString());   /*ibm@28983*/
          // Util.Debug.println("TraceRecord - tep : currentTraceID   " + currentTraceID);       /*ibm@28983*/
          // Util.Debug.println("TraceRecord - tep : currentLength    " + currentLength);        /*ibm@28983*/
          // Util.Debug.println("TraceRecord - tep : currentType      " + currentType);          /*ibm@28983*/
          // Util.Debug.println("TraceRecord - tep : currentComponent " + currentComponent);     /*ibm@28983*/
          // Util.printDump(currentBuffer,currentOffset+8);                                      /*ibm@28983*/
          entryData="*** Invalid data in Trace Entry ***";                                       /*ibm@28983*/
        }                                                                                        /*ibm@28983*/

        sBuffer.ensureCapacity(100);

        stringId = Util.formatAsHexString(currentTraceID);
        stringId = "        ".substring(stringId.length()) + stringId;
        if (TraceFormat.verMod >= 1.1) {
            sBuffer.append(threadSwitch?"*":" ");
            sBuffer.append(padding.substring(Math.min(threadIDString.length(), padding.length())) + threadIDString);
            sBuffer.append(" ").append(stringId);
            sBuffer.append(((currentType & EXCEPTION_TYPE) == EXCEPTION_TYPE)?"*":" ");
            sBuffer.append(types[currentType]);
            //Util.Debug.println("TraceBuffer: sBuffer1a            " + sBuffer.toString());
        } else {
            sBuffer.append(threadSwitch?"*":" ");
            sBuffer.append(padding.substring(Math.min(threadIDString.length(), padding.length())) + threadIDString);
            sBuffer.append(" ").append(stringId);
            sBuffer.append((currentType == EXCEPTION_TYPE)?Chars[currentType]:" ");
            sBuffer.append(types[currentType]);
            //Util.Debug.println("TraceBuffer: sBuffer1b            " + sBuffer.toString());
        }

        if ( doIndent && ((currentType == EXIT_TYPE) ||(currentType == EXIT_EXCPT_TYPE)) ) {                                             // need to decrease the indent for this thread id
            indent.delete(0, TAB.length());
            setIndent(threadIDString, indent);
        }

        //Util.Debug.println("TraceBuffer: sBuffer2             " + sBuffer.toString());
        sBuffer.append(BASE_INDENT.toString()).append(doIndent?indent.toString():"").append(Chars[currentType]).append(" ").append(entryData);

        if ( doIndent && ((currentType == ENTRY_TYPE) || (currentType == ENTRY_EXCPT_TYPE))) {                                            // need to increase the indent for this thread id
            indent.append(TAB);
            setIndent(threadIDString, indent);
        }

        //Util.Debug.println("TraceBuffer: sBuffer              " + sBuffer.toString());
        //Util.Debug.println(" ");
        notFormatted = false;
        return sBuffer.toString();
    }

    /**
     *  release the record data for this TraceRecord so GC can do it's stuff
     *
     */
    final protected void release()
    {
        buffer = null;
        currentBuffer = null;                              /*ibm@55702*/
        return;
    }

    /** compares this TraceRecord to another based on the time stamp.
     *
     *  @return    an int which is negative if this entry is older than other, 0,
     *             if they are the same age, and positive if this is newer
     */
    final public int compareTo(Object other)
    {
        // Util.Debug.println("TraceRecord compareTo: currentTimeStamp " + currentTimeStamp );
        // Util.Debug.println("TraceRecord compareTo: ((TraceRecord)other).getCurrentTimeStamp() " + ((TraceRecord)other).getCurrentTimeStamp() );
        return currentTimeStamp.compareTo(((TraceRecord)other).getCurrentTimeStamp());
    }

    /** sets the indent associated with a given thread id
     *
     * @param   threadid  a string that is the id of the thread which is to be indented
     * @return  a stringbuffer of spaces that is the amount to indent the entry associates with the specified thread id
     */
    final protected static StringBuffer getIndent(String threadID, boolean doIndent)
    {
        if (!doIndent) {
            return BASE_INDENT;
        } else {
            if ( indentLevels == null ) {
                indentLevels = new Hashtable();
            }
            StringBuffer sb = (StringBuffer)indentLevels.get(threadID);
            return(sb == null ) ? new StringBuffer() : sb;
        }
    }

    /** sets the indent associated with a given thread id
     *
     * @param   threadid  a string that is the id of the thread which is to have its indent level set
     * @param   buffer    a stringbuffer of spaces that is the amount to indent the entries associates with the specified thread id
     */
    final protected static void setIndent(String threadID, StringBuffer buffer)
    {
        if ( indentLevels == null ) {
            indentLevels = new Hashtable();
        }
        indentLevels.remove(threadID);
        indentLevels.put(threadID, buffer);
    }

    protected void prime() throws IOException {} // this method is overridden
    protected int getNextEntry() throws IOException { return 0;} // this method is overridden

}
