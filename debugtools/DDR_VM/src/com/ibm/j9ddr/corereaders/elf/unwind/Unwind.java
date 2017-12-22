/*******************************************************************************
 * Copyright (c) 2014, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.elf.unwind;

import java.io.IOException;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.elf.LinuxProcessAddressSpace;
import com.ibm.j9ddr.corereaders.elf.ProgramHeaderEntry;
import com.ibm.j9ddr.corereaders.memory.IMemoryImageInputStream;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;

/**
 * Class for parsing the unwind information from a .eh_frame header
 * 
 * It creates a list of FDE's (Frame Description Entry) objects and
 * the associated CIE's (Common Information Entry) for the process
 * when created.
 * 
 * It can then be used to get the unwind table for a given instruction
 * address. The table can then be applied to the current register state
 * to create the previous register state (or at least the register state
 * that matters for further unwinding.) 
 * 
 * See:
 * http://refspecs.linuxfoundation.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
 * 
 * @author hhellyer
 *
 */
public class Unwind {
	
	private static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
	
	LinuxProcessAddressSpace process;
	
	// A list of FDE's, the CIE's are also needed but each FDE will has
	// a reference to it's parent CIE.
	private List<FDE> frameDescriptionEntries = new LinkedList<FDE>();

	/**
	 * Create an unwinder for a process. eh_frame information is added later when
	 * the program header entries containing it are found.
	 * 
	 * @param _process The process the unwinder will work with.
	 */
	public Unwind(LinuxProcessAddressSpace _process)  {
		this.process = _process;
	}
	
	public void addCallFrameInformation(long libraryBaseAddress, ProgramHeaderEntry ph, String libName) throws CorruptDataException, IOException {
		// Locate and parse the .eh_frame information for the process so we
		// can find the right unwind data for instruction addresses we are passed later on.
		if( !ph.isEhFrame() ) {
			throw new CorruptDataException("Unexpected program header type, expected GNU_EH_FRAME");
		}
		
		long cfiAddress = getCFIAddress(libraryBaseAddress, ph, libName);
		if( cfiAddress != 0 ) {
			parseCallFrameInformation(cfiAddress, libName);
		}
	}
	
	/*
	 * Parse the call frame information from a module into CIE and FDE entries.
	 * See: http://refspecs.linuxfoundation.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
	 */
//	private void parseCallFrameInformation(IMemoryRange r) throws CorruptDataException, IOException {
	private void parseCallFrameInformation(long cfiAddress, String libName) throws CorruptDataException, IOException {

		boolean dumpData = false;

		Map<Long, CIE> cieTable = new HashMap<Long, CIE>();

		//		if( m.getName().endsWith("libpthread.so.0") ) {
		//		if( m.getName().endsWith("linux-gate.so.1") ) {
		//			dumpData = true;
		//		}

		if( dumpData ) {
			System.err.printf("Dumping data for %s!\n", libName);
		}

		try {
			if( dumpData ) {
				System.err.printf("Reading cfi info from 0x%x\n", cfiAddress);
			}

			ImageInputStream cfiStream = new IMemoryImageInputStream(process, cfiAddress);

			// Each CFI within the .eh_frame section contains multiple CIE's each followed by one or more FDE
			while( true ) {
				if( dumpData ) {
					System.err.printf("Reading length at position %d\n", cfiStream.getStreamPosition());
				}
				long cieAddress = cfiStream.getStreamPosition();
				long length = (long) cfiStream.readInt() & 0x00000000ffffffffl;
//				if( (length != 0xffffffff) && (length + cieAddress > cfiBytes.length) ) {
//					throw new CorruptDataException(String.format("CFI record contained length that exceeded available data, length: 0x%x, start pos 0x%x, data size: 0x%x",  length, cieAddress, r.getSize()));
//				}
				long startPos = cfiStream.getStreamPosition();
				if( dumpData ) {
					System.err.printf("Got length: %x (%1$d)\n", length);
				}
				if( length == 0 ) {
					if( dumpData ) {
						System.err.printf("Length = 0, end of cfi.\n");
					}
					break;
				}
				if( length == 0xffffffff ) {
					length = cfiStream.readLong();
					if( dumpData ) {
						System.err.printf("Got extended length: %x\n", length);
					}
					// Throw an exception as we've never found one of these to test with.
					// (The length would have to be >4Gb!)
					throw new CorruptDataException("CFI record contained unhandled extended length field.");
				}
				if( dumpData ) {
					System.err.printf("Reading ciePointer at position %d\n", cfiStream.getStreamPosition());
				}
				int ciePointer = cfiStream.readInt();
				if( dumpData ) {
					System.err.printf("Got ciePointer: %x\n", ciePointer);
				}
				// TODO Add a try catch and put the reader where start + length say we should be so 
				// we can skip bad records. (Or ones we just don't know how to parse.)
				if( ciePointer == 0 ) {
					if( dumpData ) {
						System.err.printf("CIE!\n");
					}
					// This is a CIE.
					CIE cie = new CIE(this, cfiStream, startPos, length);
					cieTable.put(cieAddress, cie);
					if( dumpData ) {
						cie.dump(System.err);
						System.err.printf("--- End CIE\n");
					}

				} else {
					// This is an FDE.
					if( dumpData ) {
						System.err.printf("FDE!\n");
					}
					// The parent cie is not always the last one we read.
					// The cie pointer is a relative offset (backwards)
					CIE cie = cieTable.get(startPos-ciePointer);
					if( cie == null ) {
						throw new IOException(String.format("Missing CIE record @0x%x (0x%x - 0x%x) for FDE@0x%x", ciePointer-startPos, ciePointer, startPos, startPos));
					}
					FDE fde = new FDE(this, cfiStream, cie, startPos, length);
					frameDescriptionEntries.add(fde);
					if( dumpData ) {
						fde.dump(System.err);
						System.err.printf("--- End FDE\n");
					}
				}
			}

		} catch (MemoryFault e) {
			logger.log(Level.FINER,"MemoryFault in parseCallFrameInformation for " + libName, e);
		} catch (IOException e) {
			logger.log(Level.FINER,"IOException in parseCallFrameInformation for " + libName, e);
		} catch (CorruptDataException e) {
			logger.log(Level.FINER,"CorruptDataException in parseCallFrameInformation for " + libName, e);
		}
		
		
	}
	
	/* Find the offset to the eh_frame table and return the address.
	 */
	private long getCFIAddress(long libraryBaseAddress, ProgramHeaderEntry ph, String libName) throws IOException, MemoryFault {
	
		// System.err.printf("Reading data for %s @0x%x (0x%x, + 0x%x\n", libName, libraryBaseAddress + ph.fileOffset, libraryBaseAddress, ph.fileOffset);
		
		ImageInputStream headerStream = new IMemoryImageInputStream(process, libraryBaseAddress + ph.fileOffset);
		
		long startPos = headerStream.getStreamPosition();
		
		byte version = headerStream.readByte();
		if( version != 1 ) {
			headerStream.close();
			throw new IOException(String.format("Invalid version number for .eh_frame_hdr @ 0x%x in library %s, was %d not 1", libraryBaseAddress, libName, version));
		}
		byte ptrEncoding = headerStream.readByte();
		byte countEncoding = headerStream.readByte();
		byte tableEncoding = headerStream.readByte();
		
		long ehFramePtrPos = headerStream.getStreamPosition() - startPos;
		long ehFramePtr = readEncodedPC(headerStream, ptrEncoding);		
		
		long cfiAddress = 0;
		long cfiOffset = 0;
		if((ptrEncoding & DW_EH_PE.pcrel) == DW_EH_PE.pcrel) {
			// The pointer is relative to the eh_frame_ptr field address.
			cfiOffset = ehFramePtrPos + ehFramePtr;
		}
		// The cfiOffset is relative to the file offset, once loaded that's relative to the baseAddress of this
		// module.
		cfiAddress = libraryBaseAddress + ph.fileOffset + cfiOffset;

		return cfiAddress;
		
	}

	static String byteArrayToHexString(byte[] ba) {
		String s = "";
		for( byte b: ba ) {
			s += String.format("0x%x ", b);
		}
		return s;
	}
	
	// Code to read little endian base 128 encoded numbers (unsigned).
	// Assuming for our purposes that no value will ever overflow a long.
	public static long readUnsignedLEB128(ImageInputStream cfiStream) throws IOException {
		byte b = cfiStream.readByte();
		long value = 0;
		int shift = 0;
//		System.err.printf("Got LEB128 byte %x\n", b);
		while( (b & 0x80) == 0x80 ) {
			value += (b & 0x7f) << shift;
			shift += 7;
			b = cfiStream.readByte();
//			System.err.printf("Got LEB128 byte %x\n", b);
		}
		value += (b & 0x7f) << shift;
//		System.err.printf("Returning LEB128 value %x\n", value);
		return value;
	}
	
	// Code to read little endian base 128 encoded numbers (signed).
	// Assuming for our purposes that no value will ever overflow a long.
	public static long readSignedLEB128(ImageInputStream cfiStream) throws IOException {
		long value = 0;
		int shift = 0;
		byte b;
		do {
//			System.err.printf("Got LEB128 byte %x\n", b);
			b = cfiStream.readByte();
			value += (b & 0x7f) << shift;
			shift += 7;
//			System.err.printf("Got LEB128 byte %x\n", b);
		} while( (b & 0x80) == 0x80 );
		if( (b&0x40) == 0x40 ) {
			value |= -(0x1l << shift);
		}
//		System.err.printf("Returning LEB128 value %x\n", value);
		return value;
	}
	
	/* Information for GNU unwind information, data types and number formats.. 
	 * http://refspecs.linuxfoundation.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/dwarfehencoding.html
	 */
	protected class DW_EH_PE {
		
	    static final int absptr =0x00;
	    static final int omit = 0xff;

	    /* FDE data encoding.  */
	    static final int uleb128 = 0x01;
	    static final int udata2 = 0x02;
	    static final int udata4 = 0x03;
	    static final int udata8 = 0x04;
	    static final int sleb128 = 0x09;
	    static final int sdata2 = 0x0a;
	    static final int sdata4 = 0x0b;
	    static final int sdata8 = 0x0c;
	    static final int signed = 0x08;

	    /* FDE flags.  */
	    static final int pcrel = 0x10;
	    static final int textrel = 0x20;
	    static final int datarel = 0x30;
	    static final int funcrel = 0x40;
	    static final int aligned = 0x50;

	    static final int indirect = 0x80;
	    
	  };

	
	long readEncodedPC(ImageInputStream stream, byte pcEncodingType) throws IOException {
		// Encoding is in the bottom half of the pcEncodingType.
		// Modifier flags are in the top half.
		// See: /usr/include/dwarf.h
		process.bytesPerPointer();
		switch( pcEncodingType & 0x0F ) {
			case DW_EH_PE.absptr:
				if( process.bytesPerPointer() == 8) {
					return stream.readLong();
				} else {
					return stream.readInt();
				}
			case DW_EH_PE.uleb128:
				return readUnsignedLEB128(stream);
			case DW_EH_PE.sleb128:
				return readSignedLEB128(stream);
			case DW_EH_PE.sdata2:
			case DW_EH_PE.udata2:
				return stream.readShort();
			case DW_EH_PE.sdata4:
			case DW_EH_PE.udata4:
				return stream.readInt();
			case DW_EH_PE.sdata8:
			case DW_EH_PE.udata8:
				return stream.readLong();
			default:
				throw new IOException(String.format("Unsupported encoding 0x%x", (pcEncodingType & 0x0F)));
		}
	}


	public UnwindTable getUnwindTableForInstructionAddress(long instructionPointer) throws CorruptDataException, IOException {
		for( FDE f: frameDescriptionEntries ) {
			try {
				if( f.contains(instructionPointer) ) {
					// System.err.printf("Found fde record at offset 0x%x length 0x%x\n", f.startPos, f.length);
					return new UnwindTable(f, this, instructionPointer);
				}
			} catch ( IOException e ) {
				// Ignore errors - we do not want to fail to find the right FDE because of a problem with
				// a different FDE.
			} catch ( CorruptDataException e ) {
				// Ignore.
			}
		}
		return null;
	}

}
