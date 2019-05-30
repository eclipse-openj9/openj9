/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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
import java.io.PrintStream;
import java.nio.ByteOrder;
import javax.imageio.stream.ImageInputStream;

/**
 * Class to hold a CIE (Common Information Entry).
 * 
 * See:
 * http://refspecs.linuxfoundation.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
 * 
 * @author hhellyer
 *
 */
class CIE {
		
		private final Unwind unwind;
		protected String augmentationStr;
		protected byte[] augmentationData;
		private byte personalityRoutinePointerEncoding;
		private long personalityRoutinePointer;
		private byte lsdaPointerEncoding;
		byte fdePointerEncoding = 0x0;
		private byte initialInstructions[];
		private long startPos;
		private long length;
		private byte cieVersion;
		long codeAlignmentFactor;
		long dataAlignmentFactor;
		public final long returnAddressRegister;
		final ByteOrder byteOrder;
		final int wordSize;
		private boolean signalHandlerFrame;

		public CIE(Unwind unwind, ImageInputStream cfiStream, long startPos, long length) throws IOException {

			this.unwind = unwind;
			this.startPos = startPos-4; // Needs to be adjusted for extended length field, if it ever happens.
			this.length = length;
			this.byteOrder = this.unwind.process.getByteOrder();
			this.wordSize = this.unwind.process.bytesPerPointer();
			
			cieVersion = cfiStream.readByte();
//			System.err.printf("Got cieVersion: %x\n", cieVersion);
			if( cieVersion != 1 && cieVersion != 3 ) {
				throw new IOException(String.format("Invalid CIE version %d", cieVersion));
			}
			// Augmentation string (null terminated) - max length inc NULL should be 5.
			byte[] augmentationStrBytes = new byte[5];
			int augmentationStrIndex = 0;
			augmentationStrBytes[0] = cfiStream.readByte();
			while(augmentationStrBytes[augmentationStrIndex++] != 0) {
				augmentationStrBytes[augmentationStrIndex] = cfiStream.readByte();
			}
			augmentationStr = new String(augmentationStrBytes, "ASCII");
//			System.err.printf("Got augmentationStr: %s\n", augmentationStr);
			if( "eh".equals(augmentationStr) ) {
				// eh_data is only here if the augmentation string has the value "eh"
				// Need to read
				long ehData = this.unwind.process.bytesPerPointer()==8?cfiStream.readLong():cfiStream.readInt();
			}
			codeAlignmentFactor = Unwind.readUnsignedLEB128(cfiStream);
//			System.err.printf("Got codeAlignmentFactor: %d\n", codeAlignmentFactor);
			dataAlignmentFactor = Unwind.readSignedLEB128(cfiStream);
//			System.err.printf("Got dataAlignmentFactor: %d\n", codeAlignmentFactor);
			if( cieVersion == 1) {
				returnAddressRegister = cfiStream.readByte();
			} else {
				returnAddressRegister = Unwind.readUnsignedLEB128(cfiStream);
			}
//			System.err.printf("Got returnAddressRegister: %d\n", returnAddressRegister);
			if( augmentationStr.startsWith("z")) {
				// We have augmentation data if the string starts with 'z'.
				long augmentationLength = Unwind.readUnsignedLEB128(cfiStream);
//				System.err.printf("Got augmentationLength: %d\n", augmentationLength);
				parseAugmentationData(cfiStream, augmentationStr);
			}
			int initalInstructionsSize = (int)(length - (cfiStream.getStreamPosition() - startPos));
			if ( initalInstructionsSize < 0 ) {
				throw new IOException(String.format("Negative size %d for initial instructions in CIE", initalInstructionsSize));
			}
			initialInstructions = new byte[initalInstructionsSize];
//			System.err.printf("Reading %d bytes of initial instructions\n", initialInstructions.length);
			cfiStream.read(initialInstructions, 0, initialInstructions.length);
//			System.err.printf("Initial instructions: %s\n", Unwind.byteArrayToHexString(initialInstructions));
			
			// Advance to finish on a word boundary.
			int remainder = (int)(cfiStream.getStreamPosition() % this.unwind.process.bytesPerPointer());
			
			// We don't need to skip but printing 0 nicely
			// confirms we ended up in the right place.
//			System.err.printf("Skipping %d bytes to end on word boundary\n", remainder);
			cfiStream.read(new byte[remainder], 0, remainder); 
		}

		private void parseAugmentationData(ImageInputStream cfiStream, String augmentString) throws IOException {

			for( int i = 0; i < augmentString.length(); i++ ) {
				if( 'z' == augmentString.charAt(i) ) {
					// Skip this, it just means we have augmentation data. (We know that.)
					continue;
				}
				if( 'P' == augmentString.charAt(i) ) {
					personalityRoutinePointerEncoding = cfiStream.readByte();
					personalityRoutinePointer = this.unwind.readEncodedPC(cfiStream, personalityRoutinePointerEncoding);
				}
				if( 'L' == augmentString.charAt(i) ) {
					lsdaPointerEncoding = cfiStream.readByte();
				}
				if( 'R' == augmentString.charAt(i) ) {
					fdePointerEncoding = cfiStream.readByte();
				}
				if( 'S' == augmentString.charAt(i) ) {
					signalHandlerFrame = true;
				}
			}
			
		}
		
		public void dump(PrintStream out) throws IOException {
			out.printf("Position 0x%x, length 0x%x, CIE ptr 0x%x\n", startPos, length, 0);
			out.printf("\tVersion %d\n", cieVersion);
			out.printf("\tAugmentation String: %s\n", augmentationStr);
			out.printf("\tCode Alignment: %d\n", codeAlignmentFactor);
			out.printf("\tData Alignment: %d\n", dataAlignmentFactor);
			out.printf("\tReturn Addr Reg: %d\n", returnAddressRegister);
			UnwindTable.dumpInstructions(out, initialInstructions, this);
			out.println();
		}

		public byte[] getInitialInstructions() {
			return initialInstructions;
		}
		
	}
