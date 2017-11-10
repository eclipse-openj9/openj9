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
 * Encapsulates the header for a trace file.
 *
 * @author Tim Preece
 */
final public class TraceFileHeader implements com.ibm.jvm.trace.TraceFileHeader {

    private               String            eyecatcher_string;
    private               long              length;
    private               int               version;
    private               int               modification;
    private               int               bufferSize;
    private               int               endianSignature;

    private               int               traceStart;
    private               int               serviceStart;
    private               int               startupStart;
    private               int               activeStart;
    private               int               processorStart;

    private        final  int               BIG_ENDIAN_SIG    = 0x12345678;
    private        final  int               LITTLE_ENDIAN_SIG = 0x78563412;
    private               TraceFile         traceFile;
    private        static boolean           UTEfile = false;
    private        static boolean           validHeader = false;

    protected             TraceSection      traceSection;
    private               ServiceSection    serviceSection;
    private               StartupSection    startupSection;
    private               ActiveSection     activeSection;
    private               ProcessorSection  processorSection;

    protected static final  int         INTERNAL          = 0;
    protected static final  int         EXTERNAL          = 1;

    protected TraceFileHeader(TraceFile traceFile) throws IOException
    {
        // version 1.1
        this.traceFile = traceFile;
 
        try {
            eyecatcher_string  =  Util.convertAndCheckEyecatcher(traceFile.readI());
            if (eyecatcher_string.substring(0, 4).equals("UTTH")) {
                UTEfile = true;
            }
            int ilength        =  traceFile.readI();
            version            =  traceFile.readI();
            modification       =  traceFile.readI();
            bufferSize         =  traceFile.readI();
            endianSignature    =  traceFile.readI();

            switch (this.endianSignature) {
            case BIG_ENDIAN_SIG:
                Util.Debug.println("Endian is BIG");
                traceFile.setBigEndian(true);
                Util.setBigEndian(true);
                length = ilength;
                break;
            case LITTLE_ENDIAN_SIG:
                Util.Debug.println("Endian is LITTLE");
                traceFile.setBigEndian(false);
                Util.setBigEndian(false);
                // convert to little endian
                length = Util.convertEndian(ilength);
                version = Util.convertEndian(version);
                modification = Util.convertEndian(modification);
                bufferSize = Util.convertEndian(bufferSize);
                break;
            default:
                TraceFormat.outStream.println("TraceFileHeader has invalid endian signature");
                TraceFormat.outStream.println("Please check that the input file is a binary trace file");
                return;
            }

            Util.Debug.println("TraceFileHeader: eyecatcher:       " + eyecatcher_string);
            Util.Debug.println("TraceFileHeader: length:           " + length);
            Util.Debug.println("TraceFileHeader: version:          " + version);
            Util.Debug.println("TraceFileHeader: modification:     " + modification);
            Util.Debug.println("TraceFileHeader: bufferSize:       " + bufferSize);

            TraceFormat.verMod = (float)version + (float)modification / 10;
            Util.Debug.println("TraceFileHeader: verMod:           " + TraceFormat.verMod);

            // read in the five variable sections
            traceStart        =  traceFile.readI();
            serviceStart      =  traceFile.readI();
            startupStart      =  traceFile.readI();
            activeStart       =  traceFile.readI();
            processorStart    =  traceFile.readI();

            traceSection     = new TraceSection    (traceFile,traceStart);
            serviceSection   = new ServiceSection  (traceFile,serviceStart);
            startupSection   = new StartupSection  (traceFile,startupStart);
            activeSection    = new ActiveSection   (traceFile,activeStart);
            processorSection = new ProcessorSection(traceFile,processorStart);

            validHeader = true;
        } catch(java.io.EOFException e) {
        	if (traceFile.length() == 0) {
                TraceFormat.outStream.println("Trace file " + traceFile + " is empty");
            } else {
            	TraceFormat.outStream.println("TraceFileHeader in " + traceFile + " is truncated or corrupt - cannot complete formatting");
        	}
            throw e;
        }
    }

    final protected void processTraceBufferHeaders() throws IOException
    {
        long bufferStart = length;
        int numberOfRecords=0;
        while ( bufferStart < traceFile.length()) {
            numberOfRecords++;
            if (traceSection.getTraceType() == INTERNAL) {
                new TraceRecordInternal(traceFile, bufferStart);  // this will add itself to the correct traceThread
            } else {
                new TraceRecordExternal(traceFile, bufferStart);  // this will add itself to the correct traceThread
            }
            bufferStart += bufferSize;
        }
        TraceFormat.outStream.println("Found " + numberOfRecords +
                                      " Trace Buffers in file " + traceFile.toString());
        if (TraceFormat.invalidBuffers > 0 ) {
            TraceFormat.outStream.println("*** WARNING *** found " + TraceFormat.invalidBuffers +
                                          " invalid Trace Buffers");
            TraceFormat.outStream.println("attempting to format the remainder");
        }
        Util.Debug.println("TraceFileHeader: number of buffers = " + numberOfRecords);
        TraceFormat.expectedRecords = numberOfRecords;
    }

    final protected void summarize(BufferedWriter out) throws IOException
    {
   //     traceSection.summary(out); not summarized yet - but contains no of generations
        if (!validHeader) {
            return;
        }
        serviceSection.summary(out);
        startupSection.summary(out);
        activeSection.summary(out);
        processorSection.summary(out);
    }

    /** returns the the length of this file header
     *
     * @return  a long
     */
    final protected long getLength()
    {
        return length;
    }

    /** returns the size of the buffers in the associated trace file
     *
     * @return  an int
     */
    final protected int getBufferSize()
    {
        return bufferSize;
    }

    final protected String formatFileName()
    {
        return serviceSection.formatFileName();
    }
    
    final protected String getVMLevel()
    {
    	return serviceSection.getVMLevel();
    }
    
    final protected String getNextFormatFileName(String previous)
    {
    	return serviceSection.getNextFormatFileName(previous);
    }

    /** returns true if a UTE trace file is being processed
     *
     * @return  a boolean
     */
    final static protected boolean isUTE()
    {
        return UTEfile;
    }

    final public long getTraceDataStart()
    {
    	return length;
    }

    /* methods to implement the com.ibm.jvm.trace.TraceFileHeader interface */
	public String getVMVersion(){
		return serviceSection.getServiceLevel();
	}
	public String[] getVMStartUpParameters(){
		return startupSection.getStartUpOptions();
	}
	public String[] getTraceParameters(){
		return null;
	}
	public String[] getSysProcessorInfo(){
		return null;
	}
	public long getJVMStartedMillis(){
		return 0L;
	}
	public long getLastBufferWriteMillis(){
		return 0L;
	}
	public long getFirstTracePointMillis(){
		return 0L;
	}
	public long getLastTracePointMillis(){
		return 0L;
	}
}
