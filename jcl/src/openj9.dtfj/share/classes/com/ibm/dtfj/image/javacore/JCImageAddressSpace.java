/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2020 IBM Corp. and others
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

import java.nio.ByteOrder;
import java.util.Iterator;
import java.util.Properties;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.javacore.JCInvalidArgumentsException;
import com.ibm.dtfj.javacore.builder.IBuilderData;

public class JCImageAddressSpace implements ImageAddressSpace {
	
	private Vector fProcesses;
	private Vector fImageSections;
	private JCImage fImage;

	
	public JCImageAddressSpace(JCImage image) {
		if (image == null) {
			throw new IllegalArgumentException("Must pass a valid image");
		}
		fImage = image;
		fProcesses = new Vector();
		fImageSections = new Vector();
		image.addAddressSpace(this);
	}

	@Override
	public ByteOrder getByteOrder() {
		return ByteOrder.BIG_ENDIAN; // FIXME
	}

	/**
	 * At the moment, just the last process to be added
	 */
	public ImageProcess getCurrentProcess() {
		ImageProcess currentProcess = null;
		int size = fProcesses.size();
		if (size > 0) {
			currentProcess = (ImageProcess) fProcesses.get(size-1);
		}
		return currentProcess;
	}

	/**
	 * 
	 */
	public Iterator getImageSections() {
		return fImageSections.iterator();
	}

	/**
	 * 
	 */
	public ImagePointer getPointer(long address) {
		try {
			return new JCImagePointer(this, address);
		} catch (JCInvalidArgumentsException e) {
			e.printStackTrace();
			return null;
		}
	}

	/**
	 * 
	 */
	public Iterator getProcesses() {
		return fProcesses.iterator();
	}
	
	/**
	 * Not in DTFJ
	 * @param imageProcess
	 */
	public void addImageProcess(ImageProcess imageProcess) {
		if (imageProcess != null) {
			fProcesses.add(imageProcess);
		}
	}
	
	/**
	 * Not in DTFJ
	 * 
	 */
	public JCImage getImage() {
		return fImage;
	}
	
	/**
	 * NOT in DTFJ. For building purposes only. Do not use to check if an address value is found in a given address space.
	 * It only checks that the address is not set to a default "unavailable" value.
	 * @param id
	 * 
	 */
	public boolean isValidAddressID(long id) {
		return id != (long)IBuilderData.NOT_AVAILABLE;
	}
	
	/**
	 * Not in DTFJ
	 * @param imageSection The new image section to add to the list
	 */
	public void addImageSection(ImageSection imageSection) {
		fImageSections.add(imageSection);
	}

	public String getID() throws DataUnavailable, CorruptDataException {
		return "0";
	}

	public Properties getProperties() {
		return new Properties();		//not supported for this reader
	}
}
