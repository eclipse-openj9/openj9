/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target;
/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

import java.io.IOException;
import static com.ibm.tools.attach.target.IPC.loggingStatus;
import static com.ibm.tools.attach.target.IPC.LOGGING_DISABLED;

final class WaitLoop extends Thread {

	WaitLoop() {
		setDaemon(true);
		setName("Attach API wait loop"); //$NON-NLS-1$
		/*[PR Jazz 33224 go to max priority to prevent starvation]*/
		/*
		 * Give the handler thread increased priority so it responds promptly to
		 * attach requests. the handler sleeps now so increasing the priority
		 * will not affect other threads.
		 */
		setPriority(MAX_PRIORITY);
	}
	/**
	 * @return Attachment object, or null if this VM is not being attached.
	 * @throws IOException if cannot open semaphore
	 */
	private Attachment waitForNotification(boolean retry) throws IOException {
		Object myIN = AttachHandler.mainHandler.getIgnoreNotification();

		if (LOGGING_DISABLED != loggingStatus) {
			IPC.logMessage("iteration ", AttachHandler.notificationCount," waitForNotification ignoreNotification entering"); //$NON-NLS-1$ //$NON-NLS-2$
		}

		/*
		 * If this VM is attaching, do not decrement the semaphore more than once.
		 * We cannot use the file locking trick that the potential target VMs use since this VM holds the file locks.
		 */
		synchronized (myIN) {
			if (LOGGING_DISABLED != loggingStatus) {
				IPC.logMessage("iteration ", AttachHandler.notificationCount, " waitForNotification ignoreNotification entered"); //$NON-NLS-1$ //$NON-NLS-2$
			}
		}
		if (LOGGING_DISABLED != loggingStatus) {
			IPC.logMessage("iteration ", AttachHandler.notificationCount, " waitForNotification starting wait"); //$NON-NLS-1$ //$NON-NLS-2$
		}
		
		int status = CommonDirectory.SEMAPHORE_OKAY;
		if (AttachHandler.startWaitingForSemaphore()) { /* check if we are shutting down */
			status = CommonDirectory.waitSemaphore(AttachHandler.vmId);
			AttachHandler.endWaitingForSemaphore();
		}
		if (LOGGING_DISABLED != loggingStatus) {
			IPC.logMessage("iteration ", AttachHandler.notificationCount, " waitForNotification ended wait"); //$NON-NLS-1$ //$NON-NLS-2$
		}

		if (AttachHandler.isAttachApiTerminated()) {
			/*
			 * Now that I have woken up, eat the remaining posts to the semaphore 
			 * to avoid waking other processes.
			 */
			if (AttachHandler.getDoCancelNotify()) {
				if (LOGGING_DISABLED != loggingStatus) {
					IPC.logMessage("iteration ", AttachHandler.notificationCount, " waitForNotification cancelNotify"); //$NON-NLS-1$ //$NON-NLS-2$
				}
				CommonDirectory.cancelNotify(AttachHandler.getNumberOfTargets(), true);
			}
			return null;
		} 

		if (status != CommonDirectory.SEMAPHORE_OKAY) {
			if (retry) {
				IPC.logMessage("iteration ", AttachHandler.notificationCount, " waitForNotification reopen semaphore"); //$NON-NLS-1$ //$NON-NLS-2$
				synchronized (AttachHandler.stateSync) {
					if (!AttachHandler.isAttachApiTerminated()) {
						try {
							CommonDirectory.obtainMasterLock(); /*[PR 164751 avoid scanning the directory when an attach API is launching ]*/
							status = CommonDirectory.reopenSemaphore(); 
							CommonDirectory.releaseMasterLock();
						} catch (IOException e) { 
							IPC.logMessage("waitForNotification: IOError on master lock : ", e.toString()); //$NON-NLS-1$
						}
					}
				}
				
				/*[PR Jazz 41720 - Recreate notification directory if it is deleted. ]*/
				if ((CommonDirectory.SEMAPHORE_OKAY == status) && TargetDirectory.ensureMyAdvertisementExists(AttachHandler.getVmId())) {
					if (CommonDirectory.tryObtainMasterLock()) { /*[PR 199483] post to the semaphore to test it */
						IPC.logMessage("semaphore recovery: send test post"); //$NON-NLS-1$
						int numTargets = CommonDirectory.countTargetDirectories();
						AttachHandler.setNumberOfTargets(numTargets);
						CommonDirectory.notifyVm(numTargets, true); 
						CommonDirectory.releaseMasterLock();
					}
					return waitForNotification(false);
				}
			} /* have either failed to wait twice or the semaphore won't reopen, or we couldn't create the advertisement file */
			AttachHandler.mainHandler.terminate(false);
			return null;
		}

		Attachment at = checkReplyAndCreateAttachment();
		return at;
	}

	private static Attachment checkReplyAndCreateAttachment() throws IOException {
		Attachment at = AttachHandler.mainHandler.connectToAttacher();
		/*[PR Jazz 41720 - Recreate notification directory if it is deleted. ]*/
		if (!TargetDirectory.ensureMyAdvertisementExists(AttachHandler.getVmId())) {
			/* cannot create the target directory,so shut down the attach API */
			AttachHandler.mainHandler.terminate(false); /* no need to notify myself */
		}
		if (LOGGING_DISABLED != loggingStatus) {
			IPC.logMessage("checkReplyAndCreateAttachment iteration "+ AttachHandler.notificationCount+" waitForNotification obtainLock"); //$NON-NLS-1$ //$NON-NLS-2$
		}
		if (!AttachHandler.mainHandler.syncFileLock.lockFile(true)) { /* the sync file is missing. */
			TargetDirectory.createMySyncFile();
			/* don't bother locking this since the attacher will not have locked it. */
		} else {
			if (LOGGING_DISABLED != loggingStatus) {
				IPC.logMessage("iteration ", AttachHandler.notificationCount," checkReplyAndCreateAttachment releaseLock"); //$NON-NLS-1$ //$NON-NLS-2$
			}
			AttachHandler.mainHandler.syncFileLock.unlockFile();
		}
		try {
			/*[PR Jazz 33224 Throttle the loop in to prevent the loop from occupying the semaphore ]*/
			Thread.sleep(1000);
		} catch (InterruptedException e) { /* the attach handler thread is interrupted on shutdown */
			return at;
		}
		return at;
	}

	@Override
	public void run() {
		/* Set  the current thread as a System Thread */
		com.ibm.oti.vm.VM.markCurrentThreadAsSystem();

		while (!AttachHandler.isAttachApiTerminated()) {
			try {
				waitForNotification(true);
			} catch (IOException e) {
				IPC.logMessage("exception in waitForNotification ", e.toString()); //$NON-NLS-1$
				/*[PR CMVC 188652] Suppress OOM output from attach API */
			} catch (OutOfMemoryError e) { 
				IPC.tracepoint(IPC.TRACEPOINT_STATUS_OOM_DURING_WAIT, e.getMessage());
				try {
					Thread.sleep(1000);
				} catch (InterruptedException e1) {
					continue; /* if we were interrupted by the shutdown code, the while() condition will break the loop */
				}
			}
			++AttachHandler.notificationCount;
		}
		AttachHandler.mainHandler.syncFileLock = null;	
	}
}