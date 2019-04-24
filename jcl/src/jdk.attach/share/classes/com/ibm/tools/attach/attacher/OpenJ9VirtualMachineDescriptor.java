/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.attacher;
/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import com.ibm.tools.attach.target.Advertisement;
import com.ibm.tools.attach.target.IPC;
import com.ibm.tools.attach.target.TargetDirectory;
import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.spi.AttachProvider;

/**
 * JavaSE-specific methods
 *
 */
final class OpenJ9VirtualMachineDescriptor extends VirtualMachineDescriptor {

	@Override
	/**
	 * @note this compares two remote VMs, not the objects
	 */
	public boolean equals(Object comparand) {
		return super.equals(comparand);
	}

	@Override
	/**
	 * 	@note this obtains the hashcode of the remote VM, not of this object
	 */
	public int hashCode() {
		return super.hashCode();
	}

	private final String replyFile;
	private final String attachSyncFileValue;
	private final long processId;
	private final long uid;
	private final boolean globalSemaphore;

	/**
	 * @param provider AttachProvider associated with this VM
	 * @param id VM identifier (for machine use)
	 * @param displayName VM name for human use
	 */
	OpenJ9VirtualMachineDescriptor(AttachProvider provider, String id,
			String displayName) {
		super(provider, id, displayName);
		attachSyncFileValue = null;		
		replyFile = null;
		processId = 0;
		uid = 0;
		globalSemaphore = true;
	}

	/**
	 * @param provider AttachProvider associated with this VM
	 * @param id VM identifier (for machine use)
	 */
	OpenJ9VirtualMachineDescriptor(AttachProvider provider, String id) {
		super(provider, id);
		attachSyncFileValue = null;		
		replyFile = null;
		processId = 0;
		uid = 0;
		globalSemaphore = true;
	}

	/**
	 * Create an object for a target VM (other than the caller)
	 * @param provider AttachProvider which handles the target VM
	 * @param advert Advertisement file
	 */
	OpenJ9VirtualMachineDescriptor(AttachProvider provider,
			Advertisement advert) {
		super(provider, advert.getVmId(), advert.getDisplayName());
		replyFile = advert.getReplyFile(); 
		attachSyncFileValue = advert.getNotificationSync();
		processId = advert.getProcessId();
		uid = advert.getUid();
		globalSemaphore = advert.isGlobalSemaphore();

	}

	/**
	 * 
	 * @return Operating system process ID for the target VM.
	 */
	long getProcessId() {
		return processId;
	}

	/**
	 * 
	 * @return true if the target uses the global semaphore (Windows only)
	 */
	public boolean isGlobalSemaphore() {
		return globalSemaphore;
	}

	/**
	 * Create a new VirtualMachineDescriptor with data from a file
	 * @param provider AttachProvider which creates this.
	 * @param advertFile file containing text of the advertisement
	 * @return VirtualMachineDescriptor with information from advertisement file.
	 */
	static OpenJ9VirtualMachineDescriptor fromAdvertisement(AttachProvider provider, File advertFile) {
		Advertisement advert = null;
		try (FileInputStream propStream = new FileInputStream(advertFile)) {
			advert = Advertisement.readAdvertisementFile(propStream); /* throws IOException if file cannot be read */
		} catch (IOException e) {
			IPC.logMessage("could not read advertisement file ", advertFile.getAbsolutePath()); //$NON-NLS-1$
			return null;
		}
		OpenJ9VirtualMachineDescriptor vmd = new OpenJ9VirtualMachineDescriptor(provider, advert);
		return vmd;
	}

	/**
	 * @return replyFile path to reply file
	 */
	String getReplyFile() {
		return replyFile;
	}

	/**
	 * Figure out the path to the target's sync file, inferring it when the advertisement file cannot be read.
	 * @return file path
	 */
	String getAttachSyncFileValue() {
		String asFile = attachSyncFileValue;
		if (null == asFile) {
			/* don't have permissions to the advertisement file.  Infer the filename from the VMID */
			asFile = TargetDirectory.createSyncFileObject(id()).getAbsolutePath();
		}
		return asFile;
	}

	long getUid() {
		return uid;
	}
	

}
