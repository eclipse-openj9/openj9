/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.corereaders;

import java.io.IOException;

import com.ibm.dtfj.addressspace.DumpReaderAddressSpace;
import com.ibm.dtfj.addressspace.IAbstractAddressSpace;

/**
 * @author jmdisher
 *
 */
public abstract class CoreReaderSupport implements ICoreFileReader {
	private DumpReader _reader;
	private IAbstractAddressSpace _addressSpace = null;
	protected J9RASReader _j9rasReader = null;

	protected abstract MemoryRange[] getMemoryRangesAsArray();
	protected abstract boolean isLittleEndian();
	protected abstract boolean is64Bit();

	public CoreReaderSupport(DumpReader reader) {
		_reader = reader;
	}
	
	protected int coreReadInt() throws IOException {
		return _reader.readInt();
	}
	
	protected void coreSeek(long position) throws IOException {
		_reader.seek(position);
	}
	
	protected long coreReadLong() throws IOException {
		return _reader.readLong();
	}
	
	protected long coreReadAddress() throws IOException {
		return _reader.readAddress();
	}
	
	protected short coreReadShort() throws IOException {
		return _reader.readShort();
	}

	protected byte coreReadByte() throws IOException {
		return _reader.readByte();
	}
	
	protected byte[] coreReadBytes(int n) throws IOException {
		return _reader.readBytes(n);
	}

	public IAbstractAddressSpace getAddressSpace() {
		if (null == _addressSpace) {
			MemoryRange[] ranges = getMemoryRangesAsArray();
			if (null == ranges) {
				_addressSpace = null;
			} else {
				_addressSpace = new DumpReaderAddressSpace(ranges, _reader, isLittleEndian(), is64Bit());
			}
		}
		
		//initialize the J9RAS structure reader
		if (_addressSpace != null) {
			_j9rasReader = new J9RASReader(_addressSpace,is64Bit());
		}
		return _addressSpace;
	}
	
	public boolean isTruncated() {
		return false;
	}
	
	protected long coreGetPosition() throws IOException {
		return _reader.getPosition();
	}
	
	protected boolean coreCheckOffset (long location) throws IOException {
		boolean canRead;
		long currentPos = coreGetPosition();
		try {
			coreSeek(location);
			coreReadByte();
			canRead = true;
		} catch (IOException ioe) {
			canRead = false;
		}
		coreSeek(currentPos);
		return canRead;
	}
	
	public void releaseResources() throws IOException {
		_reader.releaseResources();
	}
}
