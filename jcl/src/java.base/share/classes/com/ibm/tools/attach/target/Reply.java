/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target;
/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.RandomAccessFile;

public final class Reply {
	/**
	 * Manages the file created by an attaching process
	 *
	 */
	private  String key;
	private  Integer portNumber;
	private final File replyFile;
	static final long ROOT_UID = 0;
	static final int REPLY_PERMISSIONS = 0600;
	static final String REPLY_FILENAME = "replyInfo"; //$NON-NLS-1$
	final long targetUid;

	/**
	 * @param thePort port on which to contact attacher
	 * @param theKey Security key to validate transaction
	 * @param targetDirectory Path to target's advertisement directory
	 * @param theUid user identity of the target process. The reply file will be chowned to this UID if this process is running as root.
	 * @note this is called by the attacher, since it knows the port and key.
	 */
	public Reply(Integer thePort, String theKey, String targetDirectory, long theUid) {
		portNumber = thePort;
		key = theKey;
		targetUid = theUid;
		replyFile = new File(targetDirectory, REPLY_FILENAME);
	}

	/**
	 * @param notificationDirectory Path to target's advertisement directory
	 * @note this is called by the target, since does not know the port and key.
	 */
	Reply(String notificationDirectory) {
		targetUid = 0;
		replyFile = new File(notificationDirectory, REPLY_FILENAME);	
	}

	/**
	 * Write data to the reply file.  Must be matched with an eraseReply()
	 * keyText is a security key to validate transaction
	 * portNumberText is text to be written to file. Virtual machine ID, name, semaphore name etc.
	 * @throws IOException if file are not accessible or cannot be created with correct permissions
	 */
	public void writeReply() throws IOException {
		/* replyFile has been created by the constructor */
		final String replyFileAbsolutePath = replyFile.getAbsolutePath();
		IPC.logMessage("writing reply file port=", portNumber.intValue(), " file path=", replyFileAbsolutePath);  //$NON-NLS-1$//$NON-NLS-2$
		IPC.createNewFileWithPermissions(replyFile, TargetDirectory.ADVERTISEMENT_FILE_PERMISSIONS);
		/* we have a brand new, empty file with correct ownership and permissions */
		try (RandomAccessFile replyChannelRAF = new RandomAccessFile(replyFile, "rw")) { //$NON-NLS-1$
			replyChannelRAF.writeBytes(key);
			replyChannelRAF.writeByte('\n');
			replyChannelRAF.writeBytes(portNumber.toString());
			replyChannelRAF.writeByte('\n');
			replyChannelRAF.close();

			long myUid = IPC.getUid();
			if ((ROOT_UID == myUid) && (ROOT_UID != targetUid)) {
				IPC.chownFileToTargetUid(replyFileAbsolutePath, targetUid);
			}
		} catch (FileNotFoundException e) {
			/*[MSG "K0552", "File not found exception thrown in writeReply for file {0}"]*/
			throw new IOException(com.ibm.oti.util.Msg.getString("K0552", replyFileAbsolutePath), e); //$NON-NLS-1$
		}
	}

	/**
	 * @throws IOException if reply file is ill-formed
	 * @return null if the file does not exist, otherwise returns Reply object with port number and key
	 */
	static Reply readReply(String path) throws IOException {
		Reply rply = new Reply(path);
		String replyFileAbsolutePath = rply.replyFile.getAbsolutePath();
		if (rply.fileDoesNotExist()) {
			return null;
		}
		IPC.checkOwnerAccessOnly(replyFileAbsolutePath);
		try (BufferedReader replyStream = new BufferedReader(new FileReader(rply.replyFile));) {
			rply.key = replyStream.readLine();
			String line = replyStream.readLine();
			replyStream.close();
			try {
				rply.portNumber = Integer.valueOf(line);
			} catch (NumberFormatException e) {
				rply.portNumber = Integer.valueOf(-1);
				throw new IOException(e.getMessage());
			}
		} catch (FileNotFoundException e) {
			/*[MSG "K0548", "Cannot read reply file {0}"]*/
			throw new IOException(com.ibm.oti.util.Msg.getString("K0548", replyFileAbsolutePath), e); //$NON-NLS-1$
		}
		return rply;
	}
	
	private boolean fileDoesNotExist() {
		return !replyFile.exists();
	}

	/**
	 * @return  port on which to contact attacher
	 */
	synchronized int getPortNumber() {
		return portNumber.intValue();
	}
	/**
	 * @return  key to validate transaction
	 */
	synchronized String getKey() {
		return key;
	}	
	/**
	 * Delete the reply file
	 */
	public void deleteReply() {
		if (null != replyFile) {
			if (!replyFile.delete()) {
				IPC.logMessage("eraseReply could not delete ", replyFile.getName()); //$NON-NLS-1$
			} else {
				IPC.logMessage("eraseReply deleted ", replyFile.getName()); //$NON-NLS-1$
			}
		}
	}

}
