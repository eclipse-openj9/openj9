/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.javacore;

import java.io.IOException;
import java.net.InetAddress;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.Vector;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.javacore.parser.j9.JavaCoreResourceReleaser;
import com.ibm.dtfj.utils.ManagedImage;
import com.ibm.dtfj.utils.file.ManagedImageSource;

public class JCImage implements JCReleasingImage, ManagedImage {

	private Vector fAddressSpaces = new Vector();
	private String fosType = null;
	private String fosSubType = null;
	private String fcpuType = null;
	private String fcpuSubType = null;
	private int fcpuCount;
	private boolean fcpuCountSet = false;
	private long fBytesMem;
	private boolean fBytesMemSet = false;
	private long fCreationTime;
	private boolean fCreationTimeSet = false;
	private long fCreationTimeNanos;
	private boolean fCreationTimeNanosSet = false;
	private String fHostName = null;
	private List fIPAddresses = new Vector();
	private List _closables = new Vector();
	private URI source = null;
	private ManagedImageSource imageSource = null;
	
	public JCImage() {

	}
	
	public void setSource(URI source) {
		this.source = source;
	}

	public URI getSource() {
		try {
			if(source == null) {
				return new URI("jc:" + fHostName + ":" + fCreationTime);
			} else {
				return source;
			}
		} catch (URISyntaxException e) {
			return null;
		}
	}



	/**
	 * 
	 * @param imageAddressSpace
	 */
	public void addAddressSpace(ImageAddressSpace imageAddressSpace) {
		if (imageAddressSpace != null) {
			fAddressSpaces.add(imageAddressSpace);
		}
	}
	
	/**
	 * 
	 */
	public Iterator getAddressSpaces() {
		return fAddressSpaces.iterator();
	}

	/**
	 * 
	 */
	public long getCreationTime() throws DataUnavailable {
		if (!fCreationTimeSet) {
			throw new DataUnavailable("Creation time not available");
		}
		return fCreationTime;
	}

	/**
	 * 
	 */
	public String getHostName() throws DataUnavailable, CorruptDataException {
		if (fHostName == null) {
			throw new DataUnavailable("Host name not available");
		}
		return fHostName;
	}

	/**
	 * 
	 */
	public Iterator getIPAddresses() throws DataUnavailable {
		return fIPAddresses.iterator();
	}

	/**
	 * 
	 */
	public long getInstalledMemory() throws DataUnavailable {
		if (!fBytesMemSet) {
			throw new DataUnavailable("Installed memory not available");
		}
		return fBytesMem;
	}

	/**
	 * 
	 */
	public int getProcessorCount() throws DataUnavailable {
		if (!fcpuCountSet) {
			throw new DataUnavailable("Processor count not available");
		}
		return fcpuCount;
	}

	/**
	 * 
	 */
	public String getProcessorSubType() throws DataUnavailable, CorruptDataException {
		if (fcpuSubType == null) {
			throw new DataUnavailable("Processor subtype not available");
		}
		return fcpuSubType;
	}

	/**
	 * 
	 */
	public String getProcessorType() throws DataUnavailable, CorruptDataException {
		if (fcpuType == null) {
			throw new DataUnavailable("Processor type not available");
		}
		return fcpuType;
	}

	/**
	 * 
	 */
	public String getSystemSubType() throws DataUnavailable, CorruptDataException {
		if (fosSubType == null) {
			throw new DataUnavailable("System subtype not available");
		}
		return fosSubType;
	}

	/**
	 * 
	 */
	public String getSystemType() throws DataUnavailable, CorruptDataException {
		if (fosType == null) {
			throw new DataUnavailable("System type not available");
		}
		return fosType;
	}

	

	/*
	 * The following are not in DTFJ
	 */
	
	/**
	 * 
	 */
	public void setAddressSpaces(Vector addressSpaces) {
		fAddressSpaces = addressSpaces;
	}

	/**
	 * 
	 * @param bytesMem
	 */
	public void setBytesMem(long bytesMem) {
		fBytesMem = bytesMem;
		fBytesMemSet = true;
	}

	/**
	 * 
	 * @param cpuCount
	 */
	public void setcpuCount(int cpuCount) {
		fcpuCount = cpuCount;
		fcpuCountSet = true; 
	}

	/**
	 * 
	 * @param cpuSubType
	 */
	public void setcpuSubType(String cpuSubType) {
		fcpuSubType = cpuSubType;
	}

	/**
	 * 
	 * @param cpuType
	 */
	public void setcpuType(String cpuType) {
		fcpuType = cpuType;
	}

	/**
	 * 
	 * @param creationTime
	 */
	public void setCreationTime(long creationTime) {
		fCreationTime = creationTime;
		fCreationTimeSet = true;
	}
	
	/**
	 * 
	 * @param creationTimeNanos
	 */
	public void setCreationTimeNanos(long nanoTime) {
		fCreationTimeNanos = nanoTime;
		fCreationTimeNanosSet = true;
	}
	
	/**
	 * 
	 * @param hostName
	 */
	public void setHostName(String hostName) {
		fHostName = hostName;
	}

	/**
	 * 
	 * @param addresses
	 */
	public void addIPAddresses(InetAddress address) {
		fIPAddresses.add(address);
	}

	/**
	 * 
	 * @param osSubType
	 */
	public void setOSSubType(String osSubType) {
		fosSubType = osSubType;
	}

	/**
	 * 
	 * @param osType
	 */
	public void setOSType(String osType) {
		fosType = osType;
	}

	public void close() {
		// This is just for show; JCImageFactory closes the input stream during construction.

		Iterator it = _closables .iterator();
		while(it.hasNext()) {
			Object o = it.next();
			if (o instanceof JavaCoreResourceReleaser) {
				try {
					((JavaCoreResourceReleaser)o).releaseResources();
				} catch (IOException e) {
					Logger.getLogger(com.ibm.dtfj.image.ImageFactory.DTFJ_LOGGER_NAME).log(Level.WARNING , e.getMessage(), e);
				}
			}
		}
		//if the Image Source has been set, then see if an extracted file needs to be deleted
		if((imageSource != null) && (imageSource.getExtractedTo() != null)) {
			imageSource.getExtractedTo().delete();		//attempt to delete the file
		}
	}

	public void addReleasable(JavaCoreResourceReleaser o) {
		_closables.add(o);
	}

	public Properties getProperties() {
		return new Properties();		//not supported for this reader
	}
	
	public ManagedImageSource getImageSource() {
		return imageSource;
	}

	public void setImageSource(ManagedImageSource source) {
		imageSource = source;
		
	}

	public long getCreationTimeNanos() throws DataUnavailable, CorruptDataException {
		if (!fCreationTimeNanosSet) {
			throw new DataUnavailable("Creation time not available");
		}
		return fCreationTimeNanos;
	}
}
