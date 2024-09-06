/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2017
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package openj9.internal.tools.attach.target;

import java.io.IOException;
import static openj9.internal.tools.attach.target.IPC.loggingStatus;
import static openj9.internal.tools.attach.target.IPC.LOGGING_DISABLED;

final class WaitLoop extends Thread {

	WaitLoop() {
		super("Attach API wait loop"); //$NON-NLS-1$
		setDaemon(true);
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
							/*[PR 164751 avoid scanning the directory when an attach API is launching ]*/
							CommonDirectory.obtainControllerLock("WaitLoop.waitForNotification(" + retry + ")_1"); //$NON-NLS-1$ //$NON-NLS-2$
							status = CommonDirectory.reopenSemaphore();
							CommonDirectory.releaseControllerLock("WaitLoop.waitForNotification(" + retry + ")_2"); //$NON-NLS-1$ //$NON-NLS-2$
						} catch (IOException e) {
							IPC.logMessage("waitForNotification: IOError on controller lock : ", e.toString()); //$NON-NLS-1$
						}
					}
				}

				/*[PR Jazz 41720 - Recreate notification directory if it is deleted. ]*/
				if ((CommonDirectory.SEMAPHORE_OKAY == status) && TargetDirectory.ensureMyAdvertisementExists(AttachHandler.getVmId())) {
					/*[PR 199483] post to the semaphore to test it */
					if (CommonDirectory.tryObtainControllerLock("WaitLoop.waitForNotification(" + retry + ")_3")) { //$NON-NLS-1$ //$NON-NLS-2$
						IPC.logMessage("semaphore recovery: send test post"); //$NON-NLS-1$
						int numTargets = CommonDirectory.countTargetDirectories();
						AttachHandler.setNumberOfTargets(numTargets);
						CommonDirectory.notifyVm(numTargets, true, "WaitLoop.waitForNotification"); //$NON-NLS-1$
						CommonDirectory.releaseControllerLock("WaitLoop.waitForNotification(" + retry + ")_4"); //$NON-NLS-1$ //$NON-NLS-2$
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
		/* the sync file is missing, use FileLockWatchdogTask for non-CommonControlFile */
		if (!AttachHandler.mainHandler.syncFileLock.lockFile(true, "WaitLoop.checkReplyAndCreateAttachment", true)) { //$NON-NLS-1$
			TargetDirectory.createMySyncFile();
			/* don't bother locking this since the attacher will not have locked it. */
		} else {
			if (LOGGING_DISABLED != loggingStatus) {
				IPC.logMessage("iteration ", AttachHandler.notificationCount," checkReplyAndCreateAttachment releaseLock"); //$NON-NLS-1$ //$NON-NLS-2$
			}
			AttachHandler.mainHandler.syncFileLock.unlockFile("WaitLoop.checkReplyAndCreateAttachment"); //$NON-NLS-1$
		}
		try {
			/*[PR Jazz 33224 Throttle the loop in to prevent the loop from occupying the semaphore ]*/
			IPC.logMessage("WaitLoop.checkReplyAndCreateAttachment before sleep"); //$NON-NLS-1$
			Thread.sleep(300);
		} catch (InterruptedException e) { /* the attach handler thread is interrupted on shutdown */
			IPC.logMessage("WaitLoop.checkReplyAndCreateAttachment Interrupted"); //$NON-NLS-1$
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
				IPC.logMessage("WaitLoop.waitForNotification exception: AttachHandler.notificationCount = " + AttachHandler.notificationCount, e.toString()); //$NON-NLS-1$
				/*[PR CMVC 188652] Suppress OOM output from attach API */
			} catch (OutOfMemoryError e) {
				IPC.tracepoint(IPC.TRACEPOINT_STATUS_OOM_DURING_WAIT, e.getMessage());
				try {
					IPC.logMessage("WaitLoop.waitForNotification OutOfMemoryError before sleep"); //$NON-NLS-1$
					Thread.sleep(1000);
				} catch (InterruptedException e1) {
					IPC.logMessage("WaitLoop.waitForNotification OutOfMemoryError Interrupted"); //$NON-NLS-1$
					continue; /* if we were interrupted by the shutdown code, the while() condition will break the loop */
				}
			}
			++AttachHandler.notificationCount;
		}
		AttachHandler.mainHandler.syncFileLock = null;
	}
}
