/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.java.diagnostics.utils;

import java.net.URI;
import java.util.Collections;
import java.util.Iterator;
import java.util.Properties;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.Image;

/**
 * Simple Java bean which contains the data from the Image interface.
 * This allows commands to access this information without being given
 * a handle to the Image object itself
 * 
 * @author adam
 *
 */
public class DTFJImageBean implements Image {
	private final Image image;
	
	public DTFJImageBean(Image image) {
		this.image = image;
	}

	public long getCreationTime() throws DataUnavailable {
		return image.getCreationTime();
	}

	public String getHostName() throws DataUnavailable, CorruptDataException {
		return image.getHostName();
	}

	public Iterator<?> getIPAddresses() throws DataUnavailable {
		return image.getIPAddresses();
	}

	public long getInstalledMemory() throws DataUnavailable {
		return image.getInstalledMemory();
	}

	public int getProcessorCount() throws DataUnavailable {
		return image.getProcessorCount();
	}

	public String getProcessorSubType() throws DataUnavailable,	CorruptDataException {
		return image.getProcessorSubType();
	}

	public String getProcessorType() throws DataUnavailable, CorruptDataException {
		return image.getProcessorType();
	}

	public String getSystemSubType() throws DataUnavailable, CorruptDataException {
		return image.getSystemSubType();
	}

	public String getSystemType() throws DataUnavailable, CorruptDataException {
		return image.getSystemType();
	}
	
	public Properties getProperties() {
		return image.getProperties();
	}

	public URI getSource() {
		return image.getSource();
	}

	public void close() {
		image.close();
	}

	@Override
	public boolean equals(Object o) {
		if(o == null) {
			return false;
		}
		if(!(o instanceof DTFJImageBean)) {
			return false;
		}
		DTFJImageBean bean = (DTFJImageBean) o;
		return image.equals(bean.image);
	}

	@Override
	public int hashCode() {
		return image.hashCode();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.Image#getAddressSpaces()
	 * 
	 * This call is not supported so that plugins are not able to 'escape'
	 * from their context and into other address spaces / processes.
	 */
	public Iterator<?> getAddressSpaces() {
		return Collections.emptyList().iterator();
	}

	public long getCreationTimeNanos() throws DataUnavailable, CorruptDataException {
		return image.getCreationTimeNanos();
	}
}
