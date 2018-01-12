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
package com.ibm.dtfj.image.j9;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.Vector;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.corereaders.ResourceReleaser;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.utils.ManagedImage;
import com.ibm.dtfj.utils.file.ManagedImageSource;

/**
 * @author jmdisher
 *
 */
public class Image implements ReleasingImage, ManagedImage
{
	private String _osType;
	private String _osSubType;
	private String _cpuType;
	private String _cpuSubType;
	private int _cpuCount;
	private long _bytesMem;
	private long _creationTime;
	private Vector _addressSpaces = new Vector();
	private String _hostname;
	private List _ipAddresses = new Vector();
	private List _closables = new Vector();
	private URI source = null;
	private ManagedImageSource imageSource = null;
	
	public Image(String osType, String osSubType, String cpuType, String cpuSubType, int cpuCount, long bytesMem, long creationTime)
	{
		_osType = osType;
		_osSubType = osSubType;
		_cpuType = cpuType;
		_cpuSubType = cpuSubType;
		_cpuCount = cpuCount;
		_bytesMem = bytesMem;
		_creationTime = creationTime;
	}

	public Iterator getAddressSpaces()
	{
		return _addressSpaces.iterator();
	}

	public String getProcessorType() throws DataUnavailable, CorruptDataException
	{
		//Jazz 4961 : chamlain : NumberFormatException opening corrupt dump
		if (null == _cpuType) {
			// corrupt cores are missing this information
			throw new DataUnavailable();
		} else {
			return _cpuType;
		}
	}

	public String getProcessorSubType() throws DataUnavailable, CorruptDataException
	{
		if (null == _cpuSubType) {
			//cores from some platforms and, on Linux, from some versions of GlibC are missing this information (in the case of Linux, this is a missing AUX vector, in the core file)
			throw new DataUnavailable();
		} else {
			return _cpuSubType;
		}
	}

	public int getProcessorCount() throws DataUnavailable
	{
		if (0 == _cpuCount)
		{
			throw new DataUnavailable("Processor count could not be found");
		}
		else
		{
			return _cpuCount;
		}
	}

	public String getSystemType() throws DataUnavailable, CorruptDataException
	{
		//Jazz 4961 : chamlain : NumberFormatException opening corrupt dump
		if (null == _osType) {
			// corrupt cores are missing this information
			throw new DataUnavailable();
		} else {
			return _osType;
		}
	}

	public String getSystemSubType() throws DataUnavailable, CorruptDataException
	{
		if (null == _osSubType)
		{
			throw new DataUnavailable("Operating System did not identify its sub-type");
		}
		else
		{
			return _osSubType;
		}
	}

	public long getInstalledMemory() throws DataUnavailable
	{
		if (0 == _bytesMem)
		{
			throw new DataUnavailable("Installed memory size not found");
		}
		return _bytesMem;
	}

	public long getCreationTime() throws DataUnavailable
	{
		if (0 == _creationTime)
		{
			throw new DataUnavailable("Creation time not specified in platform core file");
		}
		return _creationTime;
	}
	
	public void addAddressSpace(ImageAddressSpace addressSpace)
	{
		_addressSpaces.add(addressSpace);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.Image#getHostName()
	 */
	public String getHostName() throws DataUnavailable
	{
		if (null != _hostname) {
			return _hostname;
		} else {
			throw new DataUnavailable();
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.Image#getIPAddresses()
	 */
	public Iterator getIPAddresses() throws DataUnavailable
	{
		if (_ipAddresses.size() > 0) {
			return _ipAddresses.iterator();
		} else {
			throw new DataUnavailable();
		}
	}
	
	public void setHostName(String hostname)
	{
		//make sure that we aren't hiding any errors
		if (null == _hostname) {
			if (hostname.length() > 0 ) {
				_hostname = hostname;
			} else {
				throw new UnsupportedOperationException("hostname cannot be an empty string");
			}
		} else {
			//this is being set twice which is an error
			throw new UnsupportedOperationException("hostname cannot be set more than once");
		}
	}
	
	public void addIPAddress(Object newAddress)
	{
		//this could be CorruptData or InetAddress so we have to allow objects in
		_ipAddresses.add(newAddress);
	}

	public void addReleasable(ResourceReleaser o) {
		_closables.add(o);
	}
	
	public void close() {
		Iterator it = _closables.iterator();
		while(it.hasNext()) {
			Object o = it.next();
			if (o instanceof ResourceReleaser) {
				try {
					((ResourceReleaser)o).releaseResources();
				} catch (IOException e) {
					Logger.getLogger(com.ibm.dtfj.image.ImageFactory.DTFJ_LOGGER_NAME).log(Level.INFO, e.getMessage());
				}
			}
		}
		//if the Image Source has been set, then see if an extracted file needs to be deleted
		if((imageSource != null) && (imageSource.getExtractedTo() != null)) {
			imageSource.getExtractedTo().delete();		//attempt to delete the file
			if((imageSource.getMetadata() != null) && (imageSource.getMetadata().getExtractedTo() != null)) {
				imageSource.getMetadata().getExtractedTo().delete();		//attempt to delete the meta-data file
			}
		}
	}
	
	/*
	 * Do NOT rely on this finalizer to clean up. Always call the close() method when done with this Image.
	 * (non-Javadoc)
	 * @see java.lang.Object#finalize()
	 */
	protected void finalize() throws Throwable {
		super.finalize();
		close();
	}

	//currently returns no OS specific properties
	public Properties getProperties() {
		return new Properties();
	}
	
	public void SetSource(URI source) {
		this.source = source;		//allow the factory to set the source without changing the legacy public methods
	}
	
	public URI getSource() {
		try {
			if(source == null) {
				return new URI("jx:" + _hostname + ":" + _creationTime);
			} else {
				return source;
			}
		} catch (URISyntaxException e) {
			return null;
		}
	}
	
	public ManagedImageSource getImageSource() {
		return imageSource;
	}

	public void setImageSource(ManagedImageSource source) {
		imageSource = source;
	}

	public long getCreationTimeNanos() throws DataUnavailable, CorruptDataException {
		// Not supported in legacy DTFJ (pre-DDR)
		throw new DataUnavailable();
	}
}
