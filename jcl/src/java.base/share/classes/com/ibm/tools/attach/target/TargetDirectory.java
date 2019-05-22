/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target;
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
import java.io.IOException;
import static com.ibm.tools.attach.target.IPC.LOGGING_DISABLED;
import static com.ibm.tools.attach.target.IPC.loggingStatus;

/**
 * This class represents the advertisement directory representing a potential attach API target VM.
 */
public final class TargetDirectory {

	private static final int VARIANT_LIMIT = 100;
	/* 
	 * allow owner or group to create, read directories, but only owner can delete.
	 * Need to allow other processes to lock the sync file. 
	 */
	static final int ADVERTISEMENT_FILE_PERMISSIONS = 0600;
	static final String ATTACH_NOTIFICATION_SYNC_FILENAME = "attachNotificationSync"; //$NON-NLS-1$
	/**
	 * All users must have write access in order to get an exclusive (i.e write) lock on the file.
	 */
	public static final int SYNC_FILE_PERMISSIONS = 0666; 
	static final int TARGET_DIRECTORY_PERMISSIONS = 01711;

	private volatile static  File targetDirectoryFileObject;
	private volatile static  File syncFileObject;
	private volatile static  File advertisementFileObject;
	
	/**
	 * Create the directory and files specific to this VM.
	 * @param myVmId proposed ID of this VM
	 * @param preserveId if false, change the VMID if there is a conflict
	 * @return actual VM ID, or null in case of error
	 * @throws IOException if files or directory cannot be created
	 */
	static String createMyDirectory(String myVmId, boolean preserveId) throws IOException {
		String newId = myVmId;
		File tgtDir = new File(getTargetDirectoryPath(newId));
		if (tgtDir.exists()) {
			/*
			 * Don't know what's in the directory, so don't re-use it.
			 * Clean it and potential other conflicts out.
			 */
			IPC.logMessage("target directory file conflict: ", tgtDir.getAbsolutePath()); //$NON-NLS-1$
			if (preserveId) {
				deleteMyDirectory(false);
			}
			CommonDirectory.deleteStaleDirectories(null);
		}
		int count = 0;

		String advertFilename = Advertisement.getFilename();
		if (!preserveId) {
			while (tgtDir.exists() && (count < VARIANT_LIMIT)) {
				newId = myVmId+'_'+count;
				IPC.logMessage("try VMID ", newId); //$NON-NLS-1$
				tgtDir = new File(getTargetDirectoryPath(newId));
				++count;
			}
		}
		if (!tgtDir.exists() && (count <= VARIANT_LIMIT)) {
			String targetDirectoryPath = tgtDir.getAbsolutePath();
			/*
			 * This fails if the file cannot be owned by the current user.
			 * The actual permissions my not be the same as requested due to umask.
			 */
			IPC.mkdirWithPermissions(targetDirectoryPath, TARGET_DIRECTORY_PERMISSIONS);
			IPC.checkOwnerAccessOnly(targetDirectoryPath);
		} else {
			/* The directory exists but is unusable. */
			IPC.logMessage("Attach API target directory already exists for VMID ", myVmId); //$NON-NLS-1$
			/*[MSG "K0547", "Attach API target directory already exists for VMID {0}"]*/			  
			throw new IOException(com.ibm.oti.util.Msg.getString("K0547", myVmId));//$NON-NLS-1$
		}
		File replyFile = new File(tgtDir, Reply.REPLY_FILENAME);
		if (replyFile.exists()) {
			/* The directory should be empty at this point. */
			final String absolutePath = replyFile.getAbsolutePath();
			IPC.logMessage("Illegal file in target directory: ", absolutePath); //$NON-NLS-1$
			/*[MSG "K0804", "Illegal file {0} found in target directory"]*/
			throw new IOException(com.ibm.oti.util.Msg.getString("K0804", absolutePath));//$NON-NLS-1$
		}

		targetDirectoryFileObject = tgtDir;
		syncFileObject = createSyncFileObject(newId);
		if (null == syncFileObject) {
			newId = null;
			IPC.logMessage("createSyncFileObject failed"); //$NON-NLS-1$
		} else {
			createMySyncFile();
			/* Advertisement creates the actual file */
			advertisementFileObject = new File(targetDirectoryFileObject, advertFilename);
			if (advertisementFileObject.exists() && !advertisementFileObject.delete()) {
				final String absolutePath = advertisementFileObject.getAbsolutePath();
				IPC.logMessage("Illegal file found in target directory:", absolutePath); //$NON-NLS-1$
				/*[MSG "K0804", "Illegal file {0} found in target directory"]*/
				throw new IOException(com.ibm.oti.util.Msg.getString("K0804", //$NON-NLS-1$
						absolutePath));
			}
		}
		return newId;
	}
	
	/**
	 * Remove this VM's target files 
	 * @return true if the files were successfully deleted
	 */
	static boolean deleteMyFiles() {
		if (LOGGING_DISABLED != loggingStatus) {
			IPC.logMessage("deleting my files"); //$NON-NLS-1$
		}

		if ((null != advertisementFileObject) && advertisementFileObject.delete()) {
			advertisementFileObject = null;		
		} else {
			return false;
		}
		if ((null != syncFileObject) && syncFileObject.delete()) {
			syncFileObject = null;
		} else {
			return false;
		}
		return true;
	}

	/**
	 * Remove this VM's target directory and the files within it
	 * @param directoryEmpty set to true if the directory is supposedly empty
	 */
	static void deleteMyDirectory(boolean directoryEmpty) {
		if (LOGGING_DISABLED != loggingStatus) {
			IPC.logMessage("deleting my directory "); //$NON-NLS-1$
		}		
		/* try the fast path for deleting files and directories. try the heavyweight path if something fails */
		if ((!directoryEmpty && !deleteMyFiles()) || (null == targetDirectoryFileObject) || !targetDirectoryFileObject.delete()) {
			deleteTargetDirectory(AttachHandler.getVmId());	
		}
	}

	public static void deleteTargetDirectory(String vmId) {
		if ((null != vmId) && (0 != vmId.length())) { /* skip if the vmid was never set - we didn't create the directory */
			File tgtDir = new File(getTargetDirectoryPath(vmId));
			File[] vmFiles = tgtDir.listFiles();
			if (LOGGING_DISABLED != loggingStatus) {
				IPC.logMessage("deleting target directory ", tgtDir.getAbsolutePath()); //$NON-NLS-1$
			}
			if (null != vmFiles) {
				for (File f: vmFiles) {
					if (!f.delete()) {
						IPC.logMessage("error deleting directory ", f.getAbsolutePath()); //$NON-NLS-1$
					} else if (LOGGING_DISABLED != loggingStatus) {
						IPC.logMessage("deleted file ", f.getAbsolutePath()); //$NON-NLS-1$
					}
				}
			}
			if (!tgtDir.delete()) {
				IPC.logMessage("error deleting directory ", tgtDir.getAbsolutePath()); //$NON-NLS-1$
			} else if (LOGGING_DISABLED != loggingStatus) {
				IPC.logMessage("deleted directory ", tgtDir.getAbsolutePath()); //$NON-NLS-1$
			}
		}
	}
	
	/**
	 * Recreate this VM's target directory if it was accidentally deleted.
	 * @param myVmId ID of this VM
	 * @return false if the directory could not be created
	 */
	static boolean ensureMyAdvertisementExists(String myVmId) {
		if (!AttachHandler.isAttachApiInitialized()) {
			/* 
			 * either initialization is in progress and sync file will be created, 
			 * or initialization failed or was terminated so we cannot create the file 
			 */
			IPC.logMessage("ensureTargetDirectoryExists: attach API not initialized"); //$NON-NLS-1$	
			return false;
		}
		if (!advertisementFileObject.exists()) { /* either the file or the enclosing directory is missing */
			IPC.logMessage("ensureTargetDirectoryExists: advertisement file missing"); //$NON-NLS-1$
			try {
				/*[PR RTC 80844 we may be shutting down or having problems with the file system]*/
				if (null== TargetDirectory.createMyDirectory(myVmId, true)) {  /* re-use the VMID */
					IPC.logMessage("ensureTargetDirectoryExists: error creating target directory"); //$NON-NLS-1$
					return false;
				};
				Advertisement.createAdvertisementFile(myVmId, AttachHandler.getMainHandler().getDisplayName());
			} catch (IOException e) {
				IPC.logMessage("ensureTargetDirectoryExists: IOException creating advertisement file"); //$NON-NLS-1$
			 	return false;
			}
		}
		return true;
	}
	
	/**
	 * create a lock file for this VM
	 * @throws IOException if the file cannot be created
	 */
	static void createMySyncFile() throws IOException {
		/*[PR RTC 80844 we may be shutting down]*/
		if (!AttachHandler.isAttachApiTerminated()) { /* don't create the file if we don't need it */
			File syncFileCopy = syncFileObject; /* guard against race conditions */
			if  (null == syncFileCopy) {
				IPC.logMessage("createMySyncFile aborted due to null syncFileObject"); //$NON-NLS-1$
			} else if (!syncFileCopy.exists()) {
				if (!syncFileCopy.createNewFile()) {
					IPC.logMessage(syncFileCopy.getName(), " already exists"); //$NON-NLS-1$
				}
				/* always do the chmod in case the file got changed accidentally */
				IPC.chmod(syncFileCopy.getAbsolutePath(), TargetDirectory.SYNC_FILE_PERMISSIONS);
				/* AttachHandler.terminate() will delete this file on shutdown */ 
			}
		}
	}
	
	/**
	 * Create the File object for this VM's sync file
	 * @param targetVmId ID of the VM
	 * @return File object to the target's sync file
	 */
	public static File createSyncFileObject(String targetVmId) {
		String tdp = getTargetDirectoryPath(targetVmId);
		if (null == tdp) {
			return null;
		}
		File syncFile=new File(tdp,ATTACH_NOTIFICATION_SYNC_FILENAME);
		return syncFile;
	}
	
	/**
	 * Get the path to the target directory for a given VMID
	 * @param vmId machine-friendly name of the VM
	 * @return directory of the target's notification directory, or null in case of error
	 */
	public static String getTargetDirectoryPath(String vmId) {
		File cd = CommonDirectory.getCommonDirFileObject();
		if ((null == vmId) || (null == cd)) {
			/* either commonDir or the VMID have not been properly initialized */
			return null;
		}
		return (new File(cd, vmId)).getPath();
	}

	/**
	 * Get the File object for this VM's target directory.
	 * @return File object 
	 */
	static File getTargetDirectoryFileObject() {
		return targetDirectoryFileObject;
	}

	/**
	 * @return File object for this VM's sync file
	 */
	static File getSyncFileObject() {
		return syncFileObject;
	}
	
	/**
	 * Get the File object for this VM's advertisement file.
	 * @return File object 
	 */
	static File getAdvertisementFileObject() {
		return advertisementFileObject;
	}
}
