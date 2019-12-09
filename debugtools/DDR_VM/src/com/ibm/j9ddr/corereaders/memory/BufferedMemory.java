/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.memory;

import java.nio.ByteOrder;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Properties;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;

/**
 * Object representing a single live
 * process model where a section of
 * memory in a the process can be represented
 * in a byte buffer as its source
 * @see com.ibm.j9ddr.tools.ddrinteractive.BufferedMemorySource
 *
 * Can be used for inspection of the
 * scc, without the need of a core file
 * For example in the following, it is
 * used to inspect romMethods:
 *
 * <code>
 * BufferedMemory memory = new BufferedMemory(ByteOrder.nativeOrder());
 * IVMData aVMData = VMDataFactory.getVMData((IProcess)memory);
 * //this address denotes the beginning of range of interest, not shown here
 * memory.addMemorySource(new BufferedMemorySource(sourceStartAddress, size));
 * aVMData.bootstrap("com.ibm.j9ddr.vm29.SomeDebugHandler");
 *
 * public class SomeDebugHandler {
 *	 //can fetch this address from a ROMClass Cookie
 *	 J9ROMClassPointer pointer = J9ROMClassPointer.cast(romStartAddress);
 *	 J9ROMMethodPointer romMethod = pointer.romMethods();
 *	 long dumpFlags = (ByteOrder.nativeOrder() == ByteOrder.BIG_ENDIAN) ? 1 : 0;
 *	 J9BCUtil.j9bcutil_dumpRomMethod(System.out, romMethod, pointer, dumpFlags, J9BCUtil.BCUtil_DumpAnnotations);
 * }
 * </code>
 *
 * Behaviour of the model is not defined
 * for the case where the underlying source's
 * ByteBuffer is modified (address or capacity)
 * during use.
 *
 * @author knewbury01
 */
public class BufferedMemory extends AbstractMemory implements IProcess, IAddressSpace
{
	private final int bytesPerPointer;

	public BufferedMemory(ByteOrder byteOrder) {
		super(byteOrder);
		bytesPerPointer = Integer.parseInt(System.getProperty("sun.arch.data.model")) / 8;
	}

	@Override
	public Platform getPlatform() {
		String platform = System.getProperty("os.name").toLowerCase();
		if (platform.contains("aix")) {
			return Platform.AIX;
		} else if (platform.contains("windows")) {
			return Platform.WINDOWS;
		} else if (platform.contains("z/os")) {
			return Platform.ZOS;
		} else if (platform.contains("linux")) {
			return Platform.LINUX;
		} else if (platform.contains("mac")) {
			return Platform.OSX;
		} else {
			//do not expect to reach here
			throw new InternalError("Unsupported platform");
		}
	}

	/**
	 *
	 * @return Address space this process uses.
	 */
	@Override
	public IAddressSpace getAddressSpace() {
		return this;
	}

	@Override
	public long getPointerAt(long address) throws MemoryFault {
		if (bytesPerPointer == 8) {
			return getLongAt(address);
		} else {
			return 0xFFFFFFFFL & getIntAt(address);
		}
	}

	/**
	 *
	 * @return Number of bytes in a pointer
	 */
	@Override
	public int bytesPerPointer() {
		return bytesPerPointer;
	}

	/**
	 * @return Process command line or null if the data is unavailable
	 * @throws CorruptDataException
	 */
	@Override
	public String getCommandLine() throws DataUnavailableException, CorruptDataException {
		throw new DataUnavailableException("DDR not invoked via command line.");
	}

	/**
	 *
	 * @return Properties containing environment variables name=value pairs
	 * @throws CorruptDataException
	 */
	@Override
	public Properties getEnvironmentVariables() throws DataUnavailableException, CorruptDataException {
		Properties env = new Properties();
		System.getenv().forEach((k, v) -> {
				env.setProperty(k, v);
			});
		return env;
	}

	@Override
	public Collection<? extends IModule> getModules() throws CorruptDataException {
		return Collections.emptySet();
	}

	@Override
	public IModule getExecutable() throws CorruptDataException {
		return null;
	}

	@Override
	public long getProcessId() throws CorruptDataException {
		return -1;
	}

	/**
	 * Equivalent to getProcedureNameForAddress(address, false).
	 * Default behaviour is to return DDR format strings for symbols.
	 */
	@Override
	public String getProcedureNameForAddress(long address) throws DataUnavailableException, CorruptDataException {
		return getProcedureNameForAddress(address, false);
	}

	@Override
	public String getProcedureNameForAddress(long address, boolean dtfjFormat) throws DataUnavailableException, CorruptDataException {
		return SymbolUtil.getProcedureNameForAddress(this, address, dtfjFormat);
	}

	@Override
	public Collection<? extends IOSThread> getThreads() throws CorruptDataException {
		return Collections.emptySet();
	}

	@Override
	public int getSignalNumber() throws DataUnavailableException {
		return 0;
	}

	@Override
	public boolean isFailingProcess() throws DataUnavailableException {
		return false;
	}

	@Override
	public ICore getCore() {
		//not backed by underlying core file
		return null;
	}

	@Override
	public List<IProcess> getProcesses() {
		return Collections.singletonList(this);
	}

	@Override
	public int getAddressSpaceId() {
		return 0;
	}
}
