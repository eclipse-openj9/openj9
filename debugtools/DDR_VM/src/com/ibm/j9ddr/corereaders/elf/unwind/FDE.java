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

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.elf.unwind.Unwind.DW_EH_PE;

/**
 * Class to hold a FDE (Frame Description Entry).
 * 
 * See:
 * http://refspecs.linuxfoundation.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
 * 
 * @author hhellyer
 *
 */
public class FDE {
		
		/**
		 * 
		 */
		private final Unwind unwind;
		CIE cie;


		private long pcBegin, pcBeginAddress;
		private long pcRange;
		private byte callFrameInstructions[];
		long startPos;
		long length;
		
		public FDE(Unwind unwind, ImageInputStream cfiStream, CIE parentCIE, long startPos, long length) throws IOException, CorruptDataException {
			
			this.unwind = unwind;
			if( parentCIE == null ) {
				throw new CorruptDataException("An FDE record must have a parent CIE");
			}
			
			this.cie = parentCIE;
			this.startPos = startPos;
			this.length = length;

//			System.err.printf("Expecting PC Begin encoded as type: 0x%x\n", parentCIE.fdePointerEncoding);
			
			pcBegin = 0;
			pcRange = 0;
			// Size and how to read these is determined by encodingType.
			// There's a lot of difference encodings!
			pcBegin = this.unwind.readEncodedPC(cfiStream, parentCIE.fdePointerEncoding);
			pcRange = this.unwind.readEncodedPC(cfiStream, parentCIE.fdePointerEncoding);
			pcBeginAddress = startPos + 8l; // (This is right unless we've got an extended length!)
//			System.err.printf("FDE is for pcBegin: 0x%x (%1$d) for 0x%x bytes\n", pcBegin, pcRange);
			
			// Augmentation data, if the parent CIE augmentation string starts with "z"
			if( parentCIE.augmentationStr.startsWith("z")) {
				// We have augmentation data if the string starts with 'z'.
				long augmentationLength = Unwind.readUnsignedLEB128(cfiStream);
//				System.err.printf("Got augmentationLength: %d\n", augmentationLength);
				byte[] augmentationData = new byte[(int)augmentationLength];
				cfiStream.read(augmentationData, 0, augmentationData.length);
//				System.err.printf("Got augmentationData %s with length: %d\n", byteArrayToHexString(augmentationData), augmentationData.length);
			}
			// TODO Need to read the LSDA here if L in augmentation string?

			
			callFrameInstructions = new byte[(int)((length) - (cfiStream.getStreamPosition() - startPos))];
			
//			System.err.printf("Reading %d bytes of initial instructions\n", callFrameInstructions.length);
			cfiStream.read(callFrameInstructions, 0, callFrameInstructions.length);
//			System.err.printf("Call frame instructions: %s\n", byteArrayToHexString(callFrameInstructions));
			// Advance to finish on a word boundary.
			int remainder = (int)(cfiStream.getStreamPosition() % this.unwind.process.bytesPerPointer());
//			System.err.printf("Skipping %d bytes to end on word boundary\n", remainder);
			cfiStream.read(new byte[remainder], 0, remainder); // TODO shouldn't need to skip but printing 0 nicely
			// confirms we ended up in the right place.
		}
		
		// Work out if this FDE applies to this (unconverted) exception address.
		public boolean contains(long instructionAddress) throws CorruptDataException, IOException {

			long base = getBaseAddress();
			long end = base + pcRange;
//			System.err.printf("Checking 0x%x is in range 0x%x to 0x%x\n", instructionAddress, base, end);
			return (instructionAddress >= base) && (instructionAddress <= end);
			
		}
		
		/**
		 * @return The lowest instruction address that this FDE (and it's associated CIE) apply to.
		 */
		public long getBaseAddress() throws IOException {
			long base = 0;
			if((cie.fdePointerEncoding & DW_EH_PE.pcrel) == DW_EH_PE.absptr) {
				// Absolute value, should just return pcBegin.
//				System.err.printf("Base address is: 0x%x (%1$d)\n", pcBegin);
				base = pcBegin;
			} else if((cie.fdePointerEncoding & DW_EH_PE.pcrel) == DW_EH_PE.pcrel) {
				// pcRelative values are relative to the address we found this value at. Nyargh.
//				System.err.printf("Base address is: 0x%x (%1$d) + 0x%x (%2$d)\n", pcBeginAddress, pcBegin);
				base = pcBeginAddress + pcBegin;
//				System.err.printf("Checking 0x%x is in range 0x%x to 0x%x\n", instructionAddress, base, end);
			} else {
				throw new IOException(String.format( "Unsupported address modifier 0x%x", cie.fdePointerEncoding));
			}
			return base;
		}
		
		public void dump(PrintStream out) throws IOException {
			out.printf("Position 0x%x, length 0x%x, CIE ptr 0x%x\n", startPos, length, 0);
			out.printf("\tpcStart 0x%x...0x%x\n", pcBegin, pcBegin+pcRange);
			out.printf("\t(pcStart 0x%x...0x%x)\n", getBaseAddress(), getBaseAddress()+pcRange);
			UnwindTable.dumpInstructions(out, callFrameInstructions, this.cie);
			out.println();
		}
		
		public CIE getCIE() {
			return cie;
		}

		public byte[] getCallFrameInstructions() {
			return callFrameInstructions;
		}
		
	}
