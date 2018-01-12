/*******************************************************************************
 * Copyright (c) 2004, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.minidump;

import java.io.IOException;

class SystemInfoStream extends EarlyInitializedStream
{

	public SystemInfoStream(int dataSize, int location)
	{
		super(dataSize, location);
	}

	public void readFrom(MiniDumpReader dump) throws IOException
	{
		dump.seek(getLocation());
		short processorArchitecture = dump.readShort();
		short processorLevel = dump.readShort();
		short processorRevision = dump.readShort();
		byte numberOfProcessors = dump.readByte();
		byte productType = dump.readByte();
		int majorVersion = dump.readInt();
		int minorVersion = dump.readInt();
		int buildNumber = dump.readInt();
		// this probably shouldn't be formatted into a string here but it is the
		// only way that the data is currently used so it simplifies things a
		// bit
		// we need an updated key of what processorLevel means. It is something
		// like "Pentium 4", for example
		// we can, however, assume that processorRevision is in the xxyy layout
		// so the bytes can be interpreted
		byte model = (byte) ((processorRevision >> 8) & 0xFF);
		byte stepping = (byte) (processorRevision & 0xFF);
		String procSubtype = "Level " + processorLevel + " Model " + model
				+ " Stepping " + stepping;

		dump.setProcessorArchitecture(processorArchitecture, procSubtype,
				numberOfProcessors);
		dump.setWindowsType(productType, majorVersion, minorVersion,
				buildNumber);
	}

	public int readPtrSize(MiniDumpReader dump)
	{
		short processorArchitecture = 0;

		try {
			dump.seek(getLocation());
			byte[] buffer = new byte[2];
			dump.readFully(buffer);
			processorArchitecture = (short) (((0xFF & buffer[1]) << 8) | (0xFF & buffer[0]));
		} catch (IOException e) {
			return 0;
		}
		return (PROCESSOR_ARCHITECTURE_AMD64 == processorArchitecture
				|| PROCESSOR_ARCHITECTURE_IA64 == processorArchitecture
				|| PROCESSOR_ARCHITECTURE_ALPHA64 == processorArchitecture || PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 == processorArchitecture) ? 64
				: 32;
	}

}
