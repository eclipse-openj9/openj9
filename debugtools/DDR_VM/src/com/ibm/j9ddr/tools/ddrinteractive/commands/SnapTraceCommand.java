/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

/**
 * Debug extension to extract trace buffers from a core dump
 * so they can be formatted.
 * 
 * @author Howard Hellyer
 *
 */
public class SnapTraceCommand extends SnapBaseCommand {

	public SnapTraceCommand()
	{
		addCommand("snaptrace", "<filename>", "extract trace buffers to a file");
	}
	
	private String fileName = null;
	
	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException {
		// Parse args, should be: 
		// Usage: !snaptrace <filename>
		if( args.length > 0 ) {
			fileName = args[0];
		} else {
			fileName = DDRInteractive.getPath();
			if( fileName == null ) {
				fileName = "snap.trc";
			} else {
				File coreFile = new File(fileName);
				fileName = coreFile.getParentFile().getPath() + File.separator + "Snap." + coreFile.getName() + ".trc";
			}
		}

		out.println("Writing snap trace to: " + fileName);
		
		extractTraceData(context, out);
	}

	protected void writeBytesToTrace(Context context, long address,
			int bufferSize, PrintStream out) {
		if( fileName != null ) {
			File snapfile = new File(fileName);
			byte[] data = new byte[bufferSize];
			
			try {
				context.process.getBytesAt(address, data);
			} catch (CorruptDataException e) {
				// Although we got a CDE some data may have been copied to the buffer.
				// This appears to happen on z/OS when some of the buffer space is in a page of
				// uninitialized memory. (See defect 185780.) In that case the missing data is
				// all 0's anyway.
				out.println("Problem reading " + bufferSize + " bytes from 0x" +  Long.toHexString(address) + ". Trace file may contain partial or damaged data.");
			}
			
			FileOutputStream fOut = null;
			try {
				fOut = new FileOutputStream(snapfile,true);
				fOut.write(data);
			} catch (FileNotFoundException e) {
				out.println("FileNotFound " + fileName);
			} catch (IOException e) {
				out.println("IO Error writing to file " + fileName);
			} finally {
				if(fOut != null) {
					try {
						fOut.close();
					} catch (IOException e) {
						// Close failed, can't do much!
					}
				}
			}
		}
	}
	
	public String getCStringAtAddress(IProcess process, long address) throws CorruptDataException {
		return getCStringAtAddress(process, address, Long.MAX_VALUE);
	}
	
	public String getCStringAtAddress(IProcess process, long address, long maxLength) throws CorruptDataException {
		int length = 0;
		while(0 != process.getByteAt(address + length) && length < maxLength) {
			length++;
		}
		byte[] buffer = new byte[length];
		process.getBytesAt(address, buffer);
		
		try {
			return new String(buffer,"UTF-8");
		} catch (UnsupportedEncodingException e) {
			throw new RuntimeException(e);
		}
	}

	@Override
	protected void writeHeaderBytesToTrace(Context context, byte[] headerBytes,
			PrintStream out) {
		if( fileName != null ) {
			File snapfile = new File(fileName);
						
			FileOutputStream fOut = null;
			try {
				fOut = new FileOutputStream(snapfile,true);
				fOut.write(headerBytes);
			} catch (FileNotFoundException e) {
				out.println("FileNotFound " + fileName);
			} catch (IOException e) {
				out.println("IO Error writing to file " + fileName);
			} finally {
				if(fOut != null) {
					try {
						fOut.close();
					} catch (IOException e) {
						// Close failed, can't do much!
					}
				}
			}
		}
		
	}
}
