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
package com.ibm.dtfj.javacore.builder.javacore;

import java.net.InetAddress;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.javacore.JCImage;
import com.ibm.dtfj.java.javacore.JCInvalidArgumentsException;
import com.ibm.dtfj.javacore.builder.BuilderFailureException;
import com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder;
import com.ibm.dtfj.javacore.builder.IImageBuilder;

public class ImageBuilder extends AbstractBuilderComponent implements IImageBuilder {

	private JCImage fImage;
	private BuilderContainer fBuilderContainer;


	
	public ImageBuilder(String id) {
		super(id);
		fImage = new JCImage();
		fBuilderContainer = getBuilderContainer();
	}

	public IImageAddressSpaceBuilder getAddressSpaceBuilder(String builderID) {
		return (IImageAddressSpaceBuilder) fBuilderContainer.findComponent(builderID);
	}
	
	
	/**
	 * 
	 * @param id
	 * 
	 */
	public IImageAddressSpaceBuilder generateAddressSpaceBuilder(String id) throws BuilderFailureException {
		IImageAddressSpaceBuilder addressSpaceBuilder = null;
		if (getAddressSpaceBuilder(id) == null) {
			try {
				addressSpaceBuilder = new ImageAddressSpaceBuilder(fImage, id);
				if (addressSpaceBuilder instanceof AbstractBuilderComponent) {
					fBuilderContainer.add((AbstractBuilderComponent)addressSpaceBuilder);
				}
				else {
					throw new BuilderFailureException(addressSpaceBuilder.toString() + " must also be an Abstract Builder Component");
				}
			} catch (JCInvalidArgumentsException e) {
				throw new BuilderFailureException(e);
			}
		}
		return addressSpaceBuilder;
	}
	
	
	/**
	 * 
	 */
	public IImageAddressSpaceBuilder getCurrentAddressSpaceBuilder() {
		return (IImageAddressSpaceBuilder) fBuilderContainer.getLastAdded();
	}
	
	
	/**
	 * 
	 */
	public Image getImage() {
		return fImage;
	}
	
	/** 
	 * Set OS type, equivalent to os.name property
	 * @param osType
	 */
	public void setOSType(String osType) {
	    fImage.setOSType(osType);
	}

	/**
	 * Set OS sub-type, equivalent to os.version property 
	 * @param osSubType
	 */
	public void setOSSubType(String osSubType) {
		fImage.setOSSubType(osSubType);
	}

	/**
	 * Set CPU type - equivalent to os.arch property
	 * @param cpuType
	 */
	public void setcpuType(String cpuType) {
		fImage.setcpuType(cpuType);
	}

	/**
	 * Set CPU sub-type 
	 * @param cpuSubType
	 */
	public void setcpuSubType(String cpuSubType) {
		fImage.setcpuSubType(cpuSubType);
	}

	/**
	 * Set CPU count   
	 * @param cpuCount
	 */
	public void setcpuCount(int cpuCount) {
		fImage.setcpuCount(cpuCount);
	}	

	/**
	 * Set dump creation time
	 * @param time
	 */
	public void setCreationTime(long creationTime) {
		fImage.setCreationTime(creationTime);
	}

	/**
	 * Set dump creation nanotime
	 * @param nanoTime
	 */
	public void setCreationTimeNanos(long nanoTime) {
		fImage.setCreationTimeNanos(nanoTime);
	}
	
	public void addHostAddr(InetAddress addr) {
		fImage.addIPAddresses(addr);
	}

	public void setHostName(String hostName) {
		fImage.setHostName(hostName);
	}

}
