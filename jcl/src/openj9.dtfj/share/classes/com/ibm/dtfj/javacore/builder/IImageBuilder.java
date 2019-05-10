/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
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
package com.ibm.dtfj.javacore.builder;

import java.net.InetAddress;

import com.ibm.dtfj.image.Image;


/**
 * Image building factory for com.ibm.dtfj.image.Image
 * <br><br>
 * Support for multiple address spaces:
 * If a javacore contains multiple address spaces, each
 * with its own set of processes and runtimes, it is assumed
 * that some sort of unique ID in the javacore (could be start address
 * of address space) is used to distinguish each address space, and that
 * the proper addressSpaceBuilder can be selected throughout the parsing
 * process by parsing a tag in the javacore that contains this id.
 *
 */
public interface IImageBuilder {
	/**
	 * 
	 * @param builderID unique id to lookup an image address space factory
	 * @return image address space factory if found, or null
	 */
	public IImageAddressSpaceBuilder getAddressSpaceBuilder(String builderID);
	
	/**
	 * At least one image address space factory must exist for each image builder factory.
	 * In multiple image address space scenarios, the last image address space factory generated
	 * may be considered the current one.
	 * @return current image address space factory. Must not be null.
	 */
	public IImageAddressSpaceBuilder getCurrentAddressSpaceBuilder();
	
	/**
	 * Get com.ibm.dtfj.image.Image being build by this image factory
	 * @return valid Image. Must not be null.
	 */
	public Image getImage();
	
	/**
	 * Generates a valid image address space factory and associates it with this image factory.
	 * Must return a valid image address space factory or throw exception if an error occurred generating
	 * the image address space factory.
	 * <br>
	 * At least one image address space factory must be created with a unique id for each image factory.
	 * <br>
	 * @param id unique id
	 * @return generated image address space factory
	 * @throws BuilderFailureException if image address space factory could not be generated
	 */
	public IImageAddressSpaceBuilder generateAddressSpaceBuilder(String id) throws BuilderFailureException;
	
	/** 
	 * Set OS type, equivalent to os.name property 
	 * @param osType
	 */
	public void setOSType(String osType);

	/**
	 * Set OS sub-type, equivalent to os.version property 
	 * @param osSubType
	 */
	public void setOSSubType(String osSubType);

	/**
	 * Set CPU type - equivalent to os.arch property
	 * @param cpuType
	 */
	public void setcpuType(String cpuType);

	/**
	 * Set CPU sub-type 
	 * @param cpuSubType
	 */
	public void setcpuSubType(String cpuSubType);

	/**
	 * Set CPU count 
	 * @param cpuCount
	 */
	public void setcpuCount(int cpuCount);

	/**
	 * Set the time the dump was created
	 * @param creationTime the time
	 */
	public void setCreationTime(long creationTime);
	
	/**
	 * Set the nanotime the dump was created
	 * @param nanoTime the time
	 */
	public void setCreationTimeNanos(long nanoTime);
	
	/**
	 * Set host name
	 * @param hostName
	 */
	public void setHostName(String hostName);
	
	/**
	 * Add a host address
	 * @param addr The IP address to add
	 */
	public void addHostAddr(InetAddress addr);
}
