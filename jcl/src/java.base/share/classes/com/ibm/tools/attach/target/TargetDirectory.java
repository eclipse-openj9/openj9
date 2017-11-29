/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target;
/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

import java.io.File;
import java.io.IOException;

/**
 * This class represents the advertisement directory representing a potentisal attach API target VM.
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
	 * Create the directory and files specific to this VM
	 * @param myVmId proposed ID of this VM
	 * @return actual VM ID, or null in case of error
	 * @throws IOException if files or directory cannot be created
	 */
	static String createMyDirectory(String myVmId, boolean recycle) throws IOException {
		String newId = myVmId;
		File tgtDir = new File(getTargetDirectoryPath(newId));
		int count = 0;
		boolean reuseDirectory = false;
		/*[PR 104653 - allow variation on process IDs as VM ids, re-use empty target directories */
		if (!recycle) {
			while (tgtDir.exists() && (count < VARIANT_LIMIT)) {
				File oldAdvert = new File(tgtDir, Advertisement.getFilename());
				if (tgtDir.isDirectory() && tgtDir.canRead() && tgtDir.canWrite() && !oldAdvert.exists()) {
					IPC.logMessage(tgtDir.getAbsolutePath()+" exists and is usable"); //$NON-NLS-1$
					/* this is a stale directory with no advertisement. Make sure it is also executable */
					tgtDir.setExecutable(true);
					reuseDirectory = true;
					break;
				}
				newId = myVmId+'_'+count;
				tgtDir = new File(getTargetDirectoryPath(newId));
				++count;
			}
		}
		if (!tgtDir.exists() && (count < VARIANT_LIMIT)) {
			IPC.mkdirWithPermissions(tgtDir.getAbsolutePath(), TARGET_DIRECTORY_PERMISSIONS);
		} else if (!reuseDirectory){ /* directory exists but is unusable */
			/*[MSG "K0547", "Attach API target directory already exists for VMID {0}"]*/			  
			throw new IOException(com.ibm.oti.util.Msg.getString("K0547", myVmId));//$NON-NLS-1$
		}
		targetDirectoryFileObject = tgtDir;
		syncFileObject = createSyncFileObject(newId);
		/*[PR RTC 80844 we may be shutting down or having problems with the file system]*/
		if (null == syncFileObject) {
			newId = null;
			IPC.logMessage("createSyncFileObject failed"); //$NON-NLS-1$
		} else {
			createMySyncFile();

			/* Advertisement creates the actual file */
			advertisementFileObject = new File(targetDirectoryFileObject, Advertisement.getFilename());
		}
		return newId;
	}
	
	/**
	 * Remove this VM's target files 
	 * @return true if the files were successfully deleted
	 */
	static boolean deleteMyFiles() {
		if (IPC.loggingEnabled ) {
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
		if (IPC.loggingEnabled ) {
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
			if (IPC.loggingEnabled ) {
				IPC.logMessage("deleting target directory ", tgtDir.getAbsolutePath()); //$NON-NLS-1$
			}
			if (null != vmFiles) {
				for (File f: vmFiles) {
					if (!f.delete()) {
						IPC.logMessage("error deleting directory ", f.getAbsolutePath()); //$NON-NLS-1$
					} else if (IPC.loggingEnabled ) {
						IPC.logMessage("deleted file ", f.getAbsolutePath()); //$NON-NLS-1$
					}
				}
			}
			if (!tgtDir.delete()) {
				IPC.logMessage("error deleting directory ", tgtDir.getAbsolutePath()); //$NON-NLS-1$
			} else if (IPC.loggingEnabled ) {
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
			 * either initialization is in progrees and sync file will be created, 
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
