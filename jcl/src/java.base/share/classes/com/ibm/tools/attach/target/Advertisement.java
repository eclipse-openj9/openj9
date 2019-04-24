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
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Properties;
import static com.ibm.tools.attach.target.IPC.loggingStatus;
import static com.ibm.tools.attach.target.IPC.LOGGING_DISABLED;

public final class Advertisement {
	private static final String KEY_ATTACH_NOTIFICATION_SYNC = "attachNotificationSync"; //$NON-NLS-1$
	private static final String KEY_REPLY_FILE = "replyFile"; //$NON-NLS-1$
	private static final String KEY_NOTIFIER = "notifier"; //$NON-NLS-1$
	private static final String KEY_DISPLAY_NAME = "displayName"; //$NON-NLS-1$
	private static final String KEY_VM_ID = "vmId"; //$NON-NLS-1$
	private static final String KEY_USER_UID = "userUid"; //$NON-NLS-1$
	private static final String KEY_USER_ID = "userId"; //$NON-NLS-1$
	private static final String KEY_VERSION = "version"; //$NON-NLS-1$
	private static final String KEY_PROCESS_ID = "processId"; //$NON-NLS-1$
	private static final String ADVERT_FILENAME = "attachInfo"; //$NON-NLS-1$
	private static final String GLOBAL_SEMAPHORE = "globalSemaphore"; //$NON-NLS-1$
	private Properties props;
	private final long pid, uid;

	/**
	 * Create these objects via readAdvertisementFile().
	 * @param advertStream
	 * @throws IOException
	 */
	private Advertisement(InputStream advertStream) throws IOException {
		props = new Properties();
		try {
			props.load(advertStream);
		} catch (IllegalArgumentException e) {
			/*[MSG "K0556", "Error parsing virtual machine advertisement file"]*/
			throw (new IOException(com.ibm.oti.util.Msg.getString("K0556"), e)); //$NON-NLS-1$
		}
		long id;
		try {
			id = Long.parseLong(props.getProperty(KEY_PROCESS_ID));
		} catch (NumberFormatException e) {
			id = 0;
		}
		pid = id;
		try {
			id = Long.parseLong(props.getProperty(KEY_USER_UID));
		} catch (NumberFormatException e) {
			id = 0;
		}
		if ((null == props.getProperty(KEY_VM_ID)) || (null == props.getProperty(KEY_DISPLAY_NAME))) {
			/* CMVC 160738 : handle malformed advertisement files */
			/*[MSG "K0554", "Virtual machine ID or display name is null"]*/
			throw new IOException(com.ibm.oti.util.Msg.getString("K0554")); //$NON-NLS-1$
		}
		uid = id;
	}

	/**
	 * Factory method to read the data from an advertisement file
	 * @param advertStream InputStream to an open file
	 * @return Advertisement object containing the data from the file.
	 * @throws IOException if file cannot be read
	 */
	public static Advertisement readAdvertisementFile(InputStream advertStream) throws IOException {
		Advertisement advert = null;
		try {
			advert = new Advertisement(advertStream);
		} catch (IOException e) {
			IPC.logMessage("IOException opening advertisement ", e.getMessage());  //$NON-NLS-1$
			throw e;
		}
		return advert;
	}
	/**
	 * This must run on the same thread as the code that created the file objects.
	 * @param vmId machine-friendly name of the VM
	 * @param displayName human-friendly name of the VM. If null or zero-length, vmId is used
	 * @return Create the information to discover and notify this VM
	 */
	private static StringBuilder createAdvertContent(String vmId, String displayName) {
		StringBuilder contentBuffer = new StringBuilder(512); 
		addKeyAsciiValue(contentBuffer, KEY_VERSION, "0.1"); //$NON-NLS-1$
		addKeyValue(contentBuffer, KEY_USER_ID, com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("user.name")); //$NON-NLS-1$
		/* CMVC 161414 - PIDs and UIDs are long */
		addKeyAsciiValue(contentBuffer, KEY_USER_UID ,Long.toString(IPC.getUid()));
		addKeyAsciiValue(contentBuffer, KEY_PROCESS_ID, Long.toString(IPC.getProcessId()));
		addKeyValue(contentBuffer, KEY_VM_ID, vmId);
		addKeyValue(contentBuffer, KEY_DISPLAY_NAME, (((null == displayName) || (displayName.length() == 0))? vmId: displayName));
		addKeyValue(contentBuffer, KEY_NOTIFIER, CommonDirectory.MASTER_NOTIFIER);
		addKeyValue(contentBuffer, GLOBAL_SEMAPHORE, Boolean.TRUE.toString());
		File tmpTargetDirectoryFileObject = TargetDirectory.getTargetDirectoryFileObject();
		File tmpSyncFileObject = TargetDirectory.getSyncFileObject();
		
		if (null != tmpTargetDirectoryFileObject && null != tmpSyncFileObject)	{
			addKeyValue(contentBuffer, KEY_REPLY_FILE, (new File(tmpTargetDirectoryFileObject, Reply.REPLY_FILENAME)).getPath());
			addKeyValue(contentBuffer, KEY_ATTACH_NOTIFICATION_SYNC, tmpSyncFileObject.getAbsolutePath());
			
			return contentBuffer;
		} else {
			return null;
		}
	}

	/**
	 * Add a key-value pair to a string buffer in Properties format.
	 * Cannot use Properties because it causes several additional classes to be loaded (defect 158165)
	 * @param contentBuffer  where to put the string
	 * @param newKey key to identify the data
	 * @param newValue value of the data
	 */
	private static void addKeyValue(StringBuilder contentBuffer, String newKey, String newValue) {
		contentBuffer.append(newKey);
		contentBuffer.append('=');
		encodeString(contentBuffer, newValue);
		contentBuffer.append('\n');
	}

	/**
	 * Add a key-value pair to a string buffer in Properties format.
	 * Use when newValue does not contain control characters
	 * @param contentBuffer  where to put the string
	 * @param newKey key to identify the data
	 * @param newValue value of the data
	 */
	private static void addKeyAsciiValue(StringBuilder contentBuffer, String newKey, String newValue) {
		contentBuffer.append(newKey);
		contentBuffer.append('=');
		contentBuffer.append(newValue);
		contentBuffer.append('\n');
	}
	/**
	 * Write out file with control characters escaped
	 * @param buffer output buffer with escaped characters
	 * @param originalString original data
	 */
	private static void encodeString(StringBuilder buffer, String originalString) {
		for (int i=0; i < originalString.length(); i++) {
			char ch = originalString.charAt(i);
			switch (ch) {
			case '\t':
				buffer.append("\\t"); //$NON-NLS-1$
				break;
			case '\n':
				buffer.append("\\n"); //$NON-NLS-1$
				break;
			case '\f':
				buffer.append("\\f"); //$NON-NLS-1$
				break;
			case '\r':
				buffer.append("\\r"); //$NON-NLS-1$
				break;
			default:
				if ("\\#!=:".indexOf(ch) >= 0 || (ch == ' ')) //$NON-NLS-1$
					buffer.append('\\');
				if (ch >= ' ' && ch <= '~') {
					buffer.append(ch);
				} else {
					String hex = Integer.toHexString(ch);
					buffer.append("\\u"); //$NON-NLS-1$
					for (int j=0; j < 4 - hex.length(); j++) buffer.append("0"); //$NON-NLS-1$
					buffer.append(hex);
				}
			}
		}
	}

	/**
	 * Write an advertisement file. Adds escapes where necessary.
	 * This is called during initialization or after successful initialization only.
	 * @param vmId ID of this VM
	 * @param displayName display name of this VM
	 * @throws IOException if cannot open the advertisement file or cannot delete an existing one
	 */
	static void createAdvertisementFile(String vmId, String displayName) throws IOException {

		if (AttachHandler.isAttachApiTerminated()) {
			/* don't create the file if we don't need it */
			return;
		}
		File advertFile = TargetDirectory.getAdvertisementFileObject();
		/* AttachHandler.terminate() will delete this file on shutdown */ 
		IPC.createNewFileWithPermissions(advertFile, TargetDirectory.ADVERTISEMENT_FILE_PERMISSIONS);
		/* we have a brand new, empty file with correct ownership and permissions */
		try (FileOutputStream advertOutputStream = new FileOutputStream(advertFile);){
			StringBuilder advertContent = createAdvertContent(vmId, displayName);
			if (null == advertContent) {
				IPC.logMessage("createAdvertisementFile failed to create advertisement file : file object is null"); //$NON-NLS-1$
				return;
			}
			
			advertOutputStream.write(advertContent.toString().getBytes("ISO8859_1")); //$NON-NLS-1$
			if (LOGGING_DISABLED != loggingStatus) {
				IPC.logMessage("createAdvertisementFile ", advertFile.getAbsolutePath()); //$NON-NLS-1$
			}
		}
	}
	
	/**
	 * @return name of advertisement file, not including directory path
	 */
	public static String getFilename() {
		return Advertisement.ADVERT_FILENAME;
	}
	
	/**
	 * @return Target virtual machine's display name
	 */
	public String getDisplayName() {
		return props.getProperty(KEY_DISPLAY_NAME);
	}
	
	/**
	 * 
	 * @return Operating system process ID of the target VM
	 */
	public long getProcessId() {
		
		return pid;
	}

	/**
	 * 
	 * @return machine-friendly identifier of the target VM
	 */
	public String getVmId() {
		return props.getProperty(KEY_VM_ID);
	}

	/**
	 * 
	 * @return identifier of the semaphore used to notify the target
	 */
	String getNotifier() {
		return props.getProperty(KEY_NOTIFIER);
	}

	/**
	 * attacher creates a file using this path when it is attaching to the target.
	 * @return file path
	 */
	public String getReplyFile() {
		return props.getProperty(KEY_REPLY_FILE);
	}

	/**
	 * 
	 * @return user identifier (numeric)
	 */
	public long getUid() {
		return uid;
	}

	/**
	 * 
	 * @return true if the target uses the global semaphore (Windows only)
	 */
	public boolean isGlobalSemaphore() {
		return Boolean.parseBoolean(props.getProperty(GLOBAL_SEMAPHORE));
	}

	/**
	 * attach locks the sync file to prevent a target from consuming >1 semaphore increments
	 * @return file path
	 */
	public String getNotificationSync() {
		return props.getProperty(KEY_ATTACH_NOTIFICATION_SYNC);
	}


}
