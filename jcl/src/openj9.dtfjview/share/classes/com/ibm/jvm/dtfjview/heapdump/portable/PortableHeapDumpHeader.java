/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.heapdump.portable;

import java.io.DataOutput;
import java.io.IOException;


/**
 * @author andhall
 *
 */
public class PortableHeapDumpHeader {

	private final String _version;
	private final boolean _is64Bit;
	private final boolean _is32BitHash;
	
	public PortableHeapDumpHeader(String version, boolean is64Bit, boolean is32BitHash) 
	{
		_version = version;
		_is64Bit = is64Bit;
		_is32BitHash = is32BitHash;
	}

	public int writeHeapDump(DataOutput dos) throws IOException 
	{
	    dos.writeUTF(PortableHeapDumpFormatter.MAGIC_STRING);
		dos.writeInt(PortableHeapDumpFormatter.PHD_VERSION_NUMBER);

		//Flags about dump
		int headerFlag = 0;
		
		if (_is64Bit) {
			headerFlag = PortableHeapDumpFormatter.IS_64_BIT_HEADER_FLAG;
		}
		if (!_is32BitHash) {
			// JVM prior to 2.6, all object records have a 16-bit hashcode
			headerFlag += PortableHeapDumpFormatter.ALL_OBJECTS_HASHED_FLAG;
		}
		//indicate that this is j9 dump
		headerFlag += PortableHeapDumpFormatter.IS_J9_HEADER_FLAG;
		dos.writeInt(headerFlag);

		//now write header fields
		dos.writeByte(PortableHeapDumpFormatter.START_OF_HEADER_TAG);
		dos.writeByte(PortableHeapDumpFormatter.FULL_VERSION_TAG);
		dos.writeUTF(_version);
		
		dos.writeByte(PortableHeapDumpFormatter.END_OF_HEADER_TAG);
		dos.writeByte(PortableHeapDumpFormatter.START_OF_DUMP_TAG);
		return 0;
	}
	
}
