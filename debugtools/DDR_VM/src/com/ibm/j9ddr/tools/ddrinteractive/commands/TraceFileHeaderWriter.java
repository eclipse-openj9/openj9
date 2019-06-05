/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive.commands;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class TraceFileHeaderWriter {
	
	private static final int UT_VERSION = 5;
	private static final int UT_MODIFICATION = 0;
	private ByteBuffer headerBytes = null;
	private final int bufferSize;
	private final boolean bigEndian;
	private final int cpuCount;
	private final int wordSize;
	private int arch = 0;
	private int subtype = 0;
	// This is always 7, J9 timer from the trCounter list below.
	private static final int traceCounter = 7; 
	private String serviceLevel = "";
	private String startupOptions = "";
	private String traceConfig = "";
	private long startPlatform = 0;
	private long startSystem = 0;
	private int type = 0;
	private int generations = 0;
	
	private final static String[] Archs = { "Unknown", "x86", "S390", "Power", "IA64", "S390X", "AMD64" };
	private final static String[] SubTypes = { "i486", "i586", "Pentium II", "Pentium III", "Merced", "McKinley", "PowerRS", "PowerPC", "GigaProcessor", "ESA", "Pentium IV", "T-Rex", "Opteron" };
	private final static String[] trCounter = { "Sequence Counter", "Special", "RDTSC Timer", "AIX Timer", "MFSPR Timer", "MFTB Timer", "STCK Timer", "J9 timer" };
	
	/**
	 * This class generates a trace file header. It is intended to be used when
	 * a core dump has been taken before the JVM has written out any trace, the
	 * trace header is not initialized until that happens.
	 * 
	 * There are a lot of arguments to pass but this data can be obtained from
	 * the dump and prevents this code needing to exist in 23, 24 and 26
	 * flavours.
	 * 
	 * @param snapTraceFileName the filename to write the trace header to
	 * @param bigEndian true if the dump came from a big endian system.
	 * @param cpuCount the number of cpu's on the system the dump came from
	 * @param wordSize the wordsize of the system the dump came from
	 * @param bufferSize the size of the trace buffers in the dump
	 * @param arch a string denoting the architecture of the system the dump came from
	 * @param processorSubType a string denoting the cpu subtype of the system the dump came from
	 * @param serviceLevel the service level of the jvm the dump came from
	 * @param startupOptions a string containing the jvm startup options
	 * @param generations 
	 * @param type 
	 */
	public TraceFileHeaderWriter(String snapTraceFileName,
			boolean bigEndian, int cpuCount, int wordSize, int bufferSize,
			String arch, String processorSubType, String serviceLevel,
			String startupOptions, String traceConfig, long startPlatform,
			long startSystem, int type, int generations
			) throws IOException {

        
        // Fix the size on this once we know how big it really will be.
		try {
			headerBytes = ByteBuffer.allocate(16*1024);
		} catch (Exception e) {
			e.printStackTrace();
			throw new IOException();
		}

		this.bigEndian = bigEndian;
        this.cpuCount = cpuCount;
        this.wordSize = wordSize;
        this.bufferSize = bufferSize;
        
        // Set the byte order on the ByteBuffer.
        if( !this.bigEndian ) {
        	headerBytes.order(ByteOrder.LITTLE_ENDIAN);
        } else {
        	headerBytes.order(ByteOrder.BIG_ENDIAN);
        }
        
        // Try looking up the architecture.
        for( int i = 0; i < Archs.length; i++ ) {
        	// See if we can find the arch in the list.
        	if( Archs[i].equalsIgnoreCase(arch)) {
        		this.arch = i;
        	}
        }
        
        // Try to find the cpu type.
        for( int i = 0; i < SubTypes.length; i++ ) {
        	// See if we can find the arch in the list.
        	if( SubTypes[i].equalsIgnoreCase(processorSubType)) {
        		this.subtype = i;
        	}
        }

        this.serviceLevel = serviceLevel;        
        this.startupOptions = startupOptions;
        this.traceConfig = traceConfig;
        
        this.startPlatform = startPlatform;
        this.startSystem = startSystem;
        this.type = type;
        this.generations = generations;

	}
	
    /**
     * Reassembles the trace file header from scratch (because there wasn't one in the core dump
     * we could reuse).
     * 
     * Based on initTraceHeader in ut_trace.c
     * @throws IOException 
     */
    public byte[] createAndWriteTraceFileHeader() throws IOException {
    	// This is the structure we are assembling see: ute/ute_internal.h
    	/*
    	 * 	#define UT_TRACEFILE_HEADER_NAME "UTTH"
			struct  utTraceFileHdr {
				UtDataHeader      header;               * Eyecatcher, version etc        *
				UT_I32            bufferSize;           * Trace buffer size              *
				UT_I32            endianSignature;      * 0x12345678 in host order       *
														* If any of these offsets are    *
														* zero, the section isn't        *
														* present.                       *
				UT_I32            traceStart;           * Offset UtTraceSection          *
				UT_I32            serviceStart;         * Offset UtServiceSection        *
				UT_I32            startupStart;         * Offset UtStartupSection        *
				UT_I32            activeStart;          * Offset UtActiveSection         *
				UT_I32            processorStart;       * Offset UtProcSection           *
				UtTraceSection    traceSection;
				UtServiceSection  serviceSection;
				UtStartupSection  startupSection;
				UtActiveSection   activeSection;
				UtProcSection     procSection;
			};
    	 */
    	

    	// Work through all the elements of the trace file header,
    	// nested structures are handled inside their own functions.
        int length = writeTraceFileHeader();

        byte[] bytesToWrite = new byte[length];
        headerBytes.position(0);
        headerBytes.get(bytesToWrite);
        return bytesToWrite;
    }
        
    private int writeTraceFileHeader() {
    	int utDataHeaderPos =  headerBytes.position();
    	int offset = writeUtDataHeader("UTTH",  0xDEADBEEF);		// header;              * Eyecatcher, version etc

    	headerBytes.putInt(bufferSize);		// bufferSize;          * Trace buffer size
    	offset+=4;
    	headerBytes.putInt(0x12345678);		// endianSignature;     * 0x12345678 in host order       *
    	offset+=4;
    	//						* If any of these offsets are    *
    	//						* zero, the section isn't        *
    	//						* present.            
    	int traceStart_pos = headerBytes.position();
    	headerBytes.putInt(0xDEADBEEF);			// traceStart;          * Offset UtTraceSection          *
    	offset+=4;
    	int serviceStart_pos = headerBytes.position();
    	headerBytes.putInt(0xDEADBEEF);			// serviceStart;        * Offset UtServiceSection        *
    	offset+=4;
    	int startupStart_pos = headerBytes.position();
    	headerBytes.putInt(0xDEADBEEF);			// startupStart;        * Offset UtStartupSection        *
    	offset+=4;
    	int activeStart_pos = headerBytes.position();
    	headerBytes.putInt(0xDEADBEEF);			// activeStart;         * Offset UtActiveSection         *
    	offset+=4;
    	int processorStart_pos = headerBytes.position();
    	headerBytes.putInt(0xDEADBEEF);			// processorStart;		* Offset UtProcSection           *
    	offset+=4;
    	headerBytes.putInt(traceStart_pos, offset); 
    	offset += writeTraceSection();		// traceSection
    	headerBytes.putInt(serviceStart_pos, offset);
    	offset += writeServiceSection(); 	// serviceSection
    	headerBytes.putInt(startupStart_pos, offset);
    	offset += writeStartupSection();	// startupSection
    	headerBytes.putInt(activeStart_pos, offset);
    	offset += writeActiveSection();	// activeSection
    	headerBytes.putInt(processorStart_pos, offset);
    	offset += writeProcSection();		// procSection

    	int length = headerBytes.position() - utDataHeaderPos;
    	// Now re-write the header at position 0 with the right length.
    	headerBytes.position(utDataHeaderPos);
    	writeUtDataHeader("UTTH",  offset);
    	return length;
    }

	private int writeProcSection() {
//		#define UT_TRACE_PROC_SECTION_NAME "UTPR"
//		struct  utProcSection {
//			UtDataHeader     header;                /* Eyecatcher, version etc        */
//			UtProcessorInfo processorInfo;          /* Processor info                 */
//		};
       	int utDataHeaderPos =  headerBytes.position();
    	int offset = writeUtDataHeader("XXXX",  0xDEADBEEF);		// header;              * Eyecatcher, version etc

    	
    	writeUtProcessorInfo();
    	
    	int length = headerBytes.position() - utDataHeaderPos;
    	// Now re-write the header at position 0 with the right length.
    	headerBytes.position(utDataHeaderPos);
    	writeUtDataHeader("UTPR",  length);
    	headerBytes.position(utDataHeaderPos+length);
    	return length;
	}

	private int writeUtProcessorInfo() {
		int utDataHeaderPos =  headerBytes.position();
		int offset = writeUtDataHeader("XXXX",  0xDEADBEEF);		// header;
//		#define UT_PROC_SECTION_NAME          "UTPR"
//		typedef struct  utProcessorInfo {
//			UtDataHeader     header;
//			UtArchitecture   architecture;
//			UT_BOOL          isBigEndian;
//			UT_U32           wordsize;
//			UT_U32           onlineProcessors;
//			UtProcInfo       procInfo;
//			UT_U32           pagesize;
//			UT_BOOL          cpuCountVaries;
//	} UtProcessorInfo;

    	// The trace formatter skips this;
		headerBytes.putInt(arch);				// architecture
    	headerBytes.putInt(bigEndian?1:0);	// isBigEndian
    	headerBytes.putInt(wordSize);			// wordSize
    	headerBytes.putInt(cpuCount);			// onlineProcessors
    	
    	writeUtProcInfo();
    	// The trace formatter skips pagesize.
    	headerBytes.putInt(4096);		// pagesize
    	// cpuCountVaries is unused everywhere.
    	headerBytes.putInt(0);			// cpuCountVaries
    	
    	int length = headerBytes.position() - utDataHeaderPos;
    	// Now re-write the header at position 0 with the right length.
    	headerBytes.position(utDataHeaderPos);
    	writeUtDataHeader("PINF",  length);
    	headerBytes.position(utDataHeaderPos+length);
    	return length;
		
	}

	private int writeUtProcInfo() {
		int utDataHeaderPos =  headerBytes.position();
		int offset = writeUtDataHeader("XXXX",  0xDEADBEEF);		// header;

//	typedef struct {
//			UtDataHeader     header;
//			UtSubtype        subtype;
//			UtTraceCounter   traceCounter;
//	} UtProcInfo;
    	
		headerBytes.putInt(subtype);
		headerBytes.putInt(traceCounter);
		
    	int length = headerBytes.position() - utDataHeaderPos;
    	// Now re-write the header at position 0 with the right length.
    	headerBytes.position(utDataHeaderPos);
    	writeUtDataHeader("PIN", length);
    	headerBytes.position(utDataHeaderPos+length);
    	return length;
		
	}

	private int writeActiveSection() {
       	int utDataHeaderPos =  headerBytes.position();
    	int offset = writeUtDataHeader("XXXX",  0xDEADBEEF);		// header;              * Eyecatcher, version etc

//    	#define UT_TRACE_ACTIVE_SECTION_NAME "UTTA"
//    	struct  utActiveSection {
//    		UtDataHeader   header;                  /* Eyecatcher, version etc        */
//    		char           active[1];               /* Trace activation commands      */
//    	};

    	try {
    		headerBytes.put(traceConfig.getBytes("UTF-8"));	// options
			headerBytes.put(new byte[] {(byte)0}); // null terminate.
    	} catch (UnsupportedEncodingException e) {
			// UTF-8 will be supported.
		}	
    	
    	int length = headerBytes.position() - utDataHeaderPos;
    	// Now re-write the header at position 0 with the right length.
    	headerBytes.position(utDataHeaderPos);
    	writeUtDataHeader("UTTA",  length);
    	headerBytes.position(utDataHeaderPos+length);
    	return length;
	
	}

	private int writeStartupSection() {
       	int utDataHeaderPos =  headerBytes.position();
    	int offset = writeUtDataHeader("XXXX",  0xDEADBEEF);		// header;              * Eyecatcher, version etc

//    	#define UT_TRACE_STARTUP_SECTION_NAME "UTSO"
//    	struct  utStartupSection {
//    		UtDataHeader   header;                  /* Eyecatcher, version etc        */
//    		char           options[1];              /* Startup options                */
//    	};

    	try {
			headerBytes.put(startupOptions.getBytes("UTF-8"));	// options
			headerBytes.put(new byte[] {(byte)0}); // null terminate.
		} catch (UnsupportedEncodingException e) {
			// UTF-8 will be supported.
		}		
    	
    	int length = headerBytes.position() - utDataHeaderPos;
    	// Now re-write the header at position 0 with the right length.
    	headerBytes.position(utDataHeaderPos);
    	writeUtDataHeader("UTSO", length);
    	headerBytes.position(utDataHeaderPos+length);
    	return length;
		}

	private int writeServiceSection() {
       	int utDataHeaderPos =  headerBytes.position();
    	int offset = writeUtDataHeader("XXXX",  0xDEADBEEF);		// header;              * Eyecatcher, version etc

//    	#define UT_TRACE_SERVICE_SECTION_NAME "UTSS"
//    	struct  utServiceSection {
//    		UtDataHeader   header;                  /* Eyecatcher, version etc        */
//    		char           level[1];                /* Service level info             */
//    	};
    	
    	try {
			headerBytes.put(serviceLevel.getBytes("UTF-8"));	// level
			headerBytes.put(new byte[] {(byte)0}); // null terminate.
		} catch (UnsupportedEncodingException e) {
			// UTF-8 will be supported.
		}	
    	
    	int length = headerBytes.position() - utDataHeaderPos;
    	// Now re-write the header at position 0 with the right length.
    	headerBytes.position(utDataHeaderPos);
    	writeUtDataHeader("UTSS",  length);
    	headerBytes.position(utDataHeaderPos+length);
    	return length;
	}



    private int writeTraceSection() {
//    	#define UT_TRACE_SECTION_NAME "UTTS"
//    	struct  utTraceSection {
//    		UtDataHeader header;                    /* Eyecatcher, version etc        */
//    		UT_U64       startPlatform;             /* Platform timer                 */
//    		UT_U64       startSystem;               /* Time relative 1/1/1970         */
//    		UT_I32       type;                      /* Internal / External trace      */
//    		UT_I32       generations;               /* Number of generations          */
//    		UT_I32       pointerSize;               /* Size in bytes of a pointer     */
//    	};
       	int utDataHeaderPos =  headerBytes.position();
    	int offset = writeUtDataHeader("XXXX",  0xDEADBEEF);		// header;              * Eyecatcher, version etc

    	headerBytes.putLong(startPlatform);	// startPlatform
    	headerBytes.putLong(startSystem);	// startSystem
    	headerBytes.putInt(type);	// type
    	headerBytes.putInt(generations);	// generations
    	headerBytes.putInt(wordSize/8);	// pointerSize
    	
    	int length = headerBytes.position() - utDataHeaderPos;
    	// Now re-write the header at position 0 with the right length.
    	headerBytes.position(utDataHeaderPos);
    	writeUtDataHeader("UTTS",  offset);
    	headerBytes.position(utDataHeaderPos+length);
    	return length;
	}

	
	/*
     * Writes a header and returns it's length
     */
	private int writeUtDataHeader(String eyeCatcher, int length ) {
		//		typedef struct  utDataHeader {
		//			char         eyecatcher[4];
		//			UT_I32       length;
		//			UT_I32       version;
		//			UT_I32       modification;
		//	    } UtDataHeader;
		int startPos = headerBytes.position();
		try {
			byte[] eyeBytes = new byte[4];
			System.arraycopy(eyeCatcher.getBytes("US-ASCII"), 0, eyeBytes, 0, eyeCatcher.length());
			headerBytes.put(eyeBytes);
		} catch (UnsupportedEncodingException e) {
			// UTF-8 will be supported.
		} 
		headerBytes.putInt(length);
		headerBytes.putInt(UT_VERSION);
		headerBytes.putInt(UT_MODIFICATION);
		return headerBytes.position() - startPos;
	}
}
