/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.image;

import java.io.IOException;
import java.net.URI;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.utils.ManagedImage;
import com.ibm.dtfj.utils.file.ManagedImageSource;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.ILibraryDependentCore;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.view.dtfj.DTFJCorruptDataException;
import com.ibm.j9ddr.view.dtfj.image.J9RASImageDataFactory.MachineData;

/**
 * Image implementation that wraps a J9DDR ICore
 * 
 * @author andhall
 *
 */
public class J9DDRImage implements ManagedImage {

	private final ICore coreFile;
	private MachineData j9MachineData;
	private boolean machineDataSet = false;
	private final URI source;
	private ManagedImageSource imageSource = null;
	private ImageInputStream meta = null;		//meta data, although DDR does nothing with it, it still has to close it
	private boolean closed = false;				//flag to indicate if the image has been closed
	
	public J9DDRImage(ICore coreFile)
	{
		this.coreFile = coreFile;
		source = null;
		
		if (coreFile instanceof ILibraryDependentCore) {
			//Figure out and pass-back the executable PATH.
			//It may not be able to figure it out from the core file header
			passBackExecutablePath((ILibraryDependentCore)coreFile);
		}
	}
	
	public J9DDRImage(URI source, ICore coreFile, ImageInputStream meta)
	{
		this.coreFile = coreFile;
		this.source = source;
		this.meta = meta;
		
		if (coreFile instanceof ILibraryDependentCore) {
			//Figure out and pass-back the executable PATH.
			//It may not be able to figure it out from the core file header
			passBackExecutablePath((ILibraryDependentCore)coreFile);
		}
	}

	public ICore getCore() {
		return coreFile;
	}
	
	public URI getSource() {
		return source;
	}

	private void passBackExecutablePath(ILibraryDependentCore core)
	{
		Iterator<J9DDRImageAddressSpace> addressSpacesIt = getAddressSpaces();
		
		while (addressSpacesIt.hasNext()) {
			J9DDRImageAddressSpace thisAS = addressSpacesIt.next();
			
			J9DDRImageProcess proc = thisAS.getCurrentProcess();
			
			String executablePath = proc.getExecutablePath();
			if (executablePath != null) {
				core.executablePathHint(executablePath);
			}
		}
	}

	private void checkJ9RASMachineData()
	{
		if (! machineDataSet) {
			j9MachineData = J9RASImageDataFactory.getMachineData(coreFile);
			machineDataSet = true;
		}
	}
	
	public Iterator<J9DDRImageAddressSpace> getAddressSpaces() {
		Collection<? extends IAddressSpace> addressSpaces = coreFile.getAddressSpaces();
		
		List<J9DDRImageAddressSpace> dtfjList = new LinkedList<J9DDRImageAddressSpace>();
		
		for (IAddressSpace thisAs : addressSpaces) {
			dtfjList.add(new J9DDRImageAddressSpace(thisAs));
		}
		
		return dtfjList.iterator();
	}

	/**
	 * Return the dump creation time. See JTC-JAT 92709, we now obtain this from a field in the J9RAS 
	 * structure that is set by the JVM dump agent when the dump is triggered. If that is not available,
	 * we revert to obtaining the creation time via the core readers from a timestamp that the OS puts
	 * in the dump. This is supported in system dumps on Windows and zOS, but not Linux or AIX.
	 * 
	 * @return long - dump creation time (milliseconds since 1970)
	 */
	public long getCreationTime() throws DataUnavailable {
		checkJ9RASMachineData();
		
		// First try getting the time from the dump time field in the J9RAS structure
		if (j9MachineData != null) {
			try {
				return j9MachineData.dumpTimeMillis();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				// Fall-through to get the creation time from the core dump itself
			} catch (com.ibm.j9ddr.DataUnavailableException e) {
				// Fall-through to get the creation time from the core dump itself
			}
		}
		
		// Revert to obtaining the creation time via the core readers, from the OS timestamp in the dump
		Properties prop = coreFile.getProperties();
		
		String result = prop.getProperty(ICore.CORE_CREATE_TIME_PROPERTY);
		
		if (result != null) {
			return Long.parseLong(result);
		} else {
			throw new DataUnavailable();
		}
	}

	public String getHostName() throws DataUnavailable, CorruptDataException {
		checkJ9RASMachineData();
		
		if (j9MachineData != null) {
			try {
				return j9MachineData.hostName();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				throw new DataUnavailable();
			} catch (com.ibm.j9ddr.DataUnavailableException e) {
				throw new DataUnavailable();
			}
		} else {
			throw new DataUnavailable();
		}
	}

	public Iterator<?> getIPAddresses() throws DataUnavailable {
		checkJ9RASMachineData();
		
		if (j9MachineData != null) {
			try {
				return j9MachineData.ipaddresses();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				throw new DataUnavailable();
			} catch (com.ibm.j9ddr.DataUnavailableException e) {
				throw new DataUnavailable();
			}
		} else {
			throw new DataUnavailable();
		}
	}

	public long getInstalledMemory() throws DataUnavailable {
		checkJ9RASMachineData();
		
		if (j9MachineData != null) {
			try {
				return j9MachineData.memoryBytes();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				//TODO think about handling this better
				throw new DataUnavailable();
			}
		} else {
			throw new DataUnavailable();
		}
	}

	public int getProcessorCount() throws DataUnavailable {
		checkJ9RASMachineData();
		
		if (j9MachineData != null) {
			try {
				return j9MachineData.cpus();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				//TODO think about handling this better
				throw new DataUnavailable();
			}
		} else {
			throw new DataUnavailable();
		}
	}

	public String getProcessorSubType() throws DataUnavailable, CorruptDataException {
		Properties prop = coreFile.getProperties();
		
		String result = prop.getProperty(ICore.PROCESSOR_SUBTYPE_PROPERTY);
		
		if (result != null) {
			return result;
		} else {
			throw new DataUnavailable();
		}
	}

	public String getProcessorType() throws DataUnavailable,
			CorruptDataException {
		checkJ9RASMachineData();
		
		if (j9MachineData != null) {
			try {
				return j9MachineData.osArch();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				throw new DTFJCorruptDataException(j9MachineData.getProcess(),e);
			}
		} else {
			Properties prop = coreFile.getProperties();
			
			String result = prop.getProperty(ICore.PROCESSOR_TYPE_PROPERTY);
			
			if (result != null) {
				return result;
			} else {
				throw new DataUnavailable();
			}
		}
	}

	public String getSystemSubType() throws DataUnavailable,
			CorruptDataException {
		checkJ9RASMachineData();
		
		if (j9MachineData != null) {
			try {
				return j9MachineData.osVersion();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				//Ignore  - fall back on core readers
			}
		}
		
		Properties prop = coreFile.getProperties();
		
		String result = prop.getProperty(ICore.SYSTEM_SUBTYPE_PROPERTY);
		
		if (result != null) {
			return result;
		} else {
			throw new DataUnavailable();
		}
	}

	public String getSystemType() throws DataUnavailable, CorruptDataException {
		checkJ9RASMachineData();
		
		if (j9MachineData != null) {
			try {
				return j9MachineData.osName();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				//Ignore  - fall back on core readers
			}
		}
		
		Properties prop = coreFile.getProperties();
		
		String result = prop.getProperty(ICore.SYSTEM_TYPE_PROPERTY);
		
		if (result != null) {
			return result;
		} else {
			throw new DataUnavailable();
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.Image#close()
	 */
	public void close() {
		closed = true;			//indicate that we've attempted to close the image even if it ultimately fails
		try {
			coreFile.close();
			if(meta != null) {
				meta.close();
			}
		} catch (IOException e) {
			//ignore errors as unlikely the caller can fix this
		}
		//if the Image Source has been set, then see if an extracted file needs to be deleted
		if((imageSource != null) && (imageSource.getExtractedTo() != null)) {
			imageSource.getExtractedTo().delete();		//attempt to delete the file
			if(imageSource.hasMetaData() && (imageSource.getMetadata().getExtractedTo() != null)) {
				imageSource.getMetadata().getExtractedTo().delete();
			}
		}
	}

	/**
	 * Return OS properties (as obtained by the JVM, no support in the core readers)
	 * 
	 * @return Properties (may be empty)
	 */
	public Properties getProperties() {
		checkJ9RASMachineData();
		
		if (j9MachineData != null) {
			try {
				return j9MachineData.systemInfo();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				// ignore (not supported in the DTFJ Image.getProperties() API)
			} catch (com.ibm.j9ddr.DataUnavailableException e) {
				// ignore (not supported in the DTFJ Image.getProperties() API)
			}
		}
		return new Properties();
	}

	@Override
	public boolean equals(Object o) {
		if(o == null) {
			return false;
		}
		if(!(o instanceof J9DDRImage)) {
			return false;
		}
		J9DDRImage compareto = (J9DDRImage) o;
		return source.equals(compareto.source);
	}

	@Override
	public int hashCode() {
		return super.hashCode();
	}
	
	public ManagedImageSource getImageSource() {
		return imageSource;
	}

	public void setImageSource(ManagedImageSource source) {
		imageSource = source;
	}
	
	public boolean isClosed() {
		return closed;
	}
	
	/**
	 * Return the value of the system nanotime (high resolution timer) at dump creation time. See JTC-JAT
	 * 92709, we obtain this from a field in the J9RAS structure that is set by the JVM dump agent when the
	 * dump is triggered.
	 * 
	 * @return long - system nanotime
	 */
	public long getCreationTimeNanos() throws DataUnavailable, CorruptDataException {
		checkJ9RASMachineData();
		
		if (j9MachineData != null) {
			try {
				return j9MachineData.dumpTimeNanos();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				throw new DTFJCorruptDataException(j9MachineData.getProcess(),e);
			} catch (com.ibm.j9ddr.DataUnavailableException e) {
				// fall through
			}
		}
		throw new DataUnavailable();
	}
}
