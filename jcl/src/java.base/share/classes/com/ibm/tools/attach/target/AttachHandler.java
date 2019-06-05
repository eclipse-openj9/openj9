/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target;

import static com.ibm.tools.attach.target.IPC.LOGGING_DISABLED;
import static com.ibm.tools.attach.target.IPC.LOGGING_ENABLED;
import static com.ibm.tools.attach.target.IPC.loggingStatus;

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
import java.io.PrintStream;
import java.util.Properties;
import java.util.Vector;

import com.ibm.oti.vm.VM;

/**
 * This class handles incoming attachment requests from other VMs. Must be
 * public because it is used by java.lang.System
 * 
 */
public class AttachHandler extends Thread {

	/*[PR Jazz 3007] remove duplicate definition of SYNC_FILE_PERMISSIONS */
	private static final String VMID_PROPERTY = "com.ibm.tools.attach.id"; //$NON-NLS-1$
	private static final String DISPLAYNAME_PROPERTY = "com.ibm.tools.attach.displayName"; //$NON-NLS-1$
	/**
	 * set this property to "yes" to enable Attach API logging
	 */
	static final String LOGGING_ENABLE_PROPERTY = "com.ibm.tools.attach.logging"; //$NON-NLS-1$
	static final String LOG_NAME_PROPERTY = "com.ibm.tools.attach.log.name"; //$NON-NLS-1$
	static final String VMID_VALID_PATTERN = "\\p{Alpha}\\w*"; /* Alphabetic followed by alphanumeric or underscore */ //$NON-NLS-1$

	/**
	 * Time delay before we give up trying to terminate the wait loop.
	 */
	static final long shutdownTimeoutMs = Integer.getInteger("com.ibm.tools.attach.shutdown_timeout", 10000).longValue(); //$NON-NLS-1$
	private enum AttachStateValues {
		ATTACH_UNINITIALIZED, ATTACH_TERMINATED, ATTACH_STARTING, ATTACH_INITIALIZED
	}

	/**
	 * This is a singleton: there is only one handler per VM
	 */
	static AttachHandler mainHandler = new AttachHandler();
	static volatile Thread currentAttachThread = mainHandler; /* Join on this when shutting down */
	private Vector<Attachment> attachments = new Vector<>();
	static String vmId = ""; /* ID of the currently running VM *///$NON-NLS-1$
	/**
	 * Human-friendly name for VM
	 */
	private String displayName;
	private static AttachStateValues attachState = AttachStateValues.ATTACH_UNINITIALIZED;
	private static boolean waitingForSemaphore = false;

	static final class AttachStateSync {
		/**
		 * Empty class for synchronization objects.
		 */
	}

	static AttachStateSync stateSync = new AttachStateSync();

	static int notificationCount;

	static final class syncObject {
		/**
		 * Empty class for synchronization objects.
		 */
	}

	private static boolean doCancelNotify;
	
	/**
	 * used to force an attacher to ignore its own notifications
	 */
	private final syncObject ignoreNotification = new syncObject();

	private static final syncObject accessorMutex = new syncObject();
	private static String nameProperty;
	private static String pidProperty;
	private static int numberOfTargets;
	/**
	 * As of Java 9, a VM cannot attach to itself unless explicitly enabled.
	 * Grab the setting before the application has a chance to change it, 
	 * but parse it lazily because we rarely need the value.
	 */
	public final static String allowAttachSelf = 
			VM.getVMLangAccess().internalGetProperties().getProperty("jdk.attach.allowAttachSelf", //$NON-NLS-1$
/*[IF Sidecar19-SE]*/
					"false" //$NON-NLS-1$
/*[ELSE]
					"true" //$NON-NLS-1$
/*[ENDIF]*/
					);  
	
	/* only the attach handler thread uses syncFileLock */
	/* [PR Jazz 30075] Make syncFileLock an instance variable since it is accessed only by the attachHandler singleton. */
	FileLock syncFileLock;

	/**
	 * Keep the constructor private
	 */
	private AttachHandler() {
		setDaemon(true);
		setName("Attach API initializer"); //$NON-NLS-1$
	}

	/**
	 * start the attach handler, create files , semaphore, etc.
	 * 
	 * @note This is on the VM startup critical path.
	 */
	static void initializeAttachAPI() {
		/* Disable attach on z/OS if the process is using the default OMVS segment. */
		/*[PR Jazz 37922 default UID check takes priority over the system property ]*/
		if (IPC.isUsingDefaultUid()) {
			setAttachState(AttachStateValues.ATTACH_TERMINATED);
			return;
		}
		/*
		 *  set default behaviour:
		 *  Java 6 R24 and later: disabled by default on z/OS, enabled on all other platforms
		 *  Java 5: disabled by default on all platforms 
		 */
		/*[PR Jazz 59196 LIR: Disable attach API by default on z/OS (31972)]*/
		boolean enableAttach = true;
		String osName = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("os.name"); //$NON-NLS-1$
		if (null != osName) {
			if (osName.equalsIgnoreCase("z/OS")) { //$NON-NLS-1$
				enableAttach = false;
			} else if (osName.startsWith("Windows")) { //$NON-NLS-1$
				IPC.isWindows = true;
			}
		}
		/* the system property overrides the default */
		String enableAttachProp = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("com.ibm.tools.attach.enable");  //$NON-NLS-1$
		if (null != enableAttachProp) {
			if (enableAttachProp.equalsIgnoreCase("no")) { //$NON-NLS-1$
				enableAttach = false;
			} else if (enableAttachProp.equalsIgnoreCase("yes")) { //$NON-NLS-1$
				enableAttach = true;
			}
		}
		if (enableAttach) {
			/* CMVC 161383.  Ensure the shutdown hook is in place before the thread starts */
			java.lang.Runtime.getRuntime().addShutdownHook(new teardownHook());
			mainHandler.start();
		} else { /*[PR Jazz 31596 add missing state transition]*/
			setAttachState(AttachStateValues.ATTACH_TERMINATED);
		}
	}

	/*
	 * VMID must be a string of word characters starting with an alphabetic to
	 * distinguish it from PIDs and make life easier for the file system.
	 */
	private static String validateVmId(String id) {
		if (null != id) {
			if (!id.matches(VMID_VALID_PATTERN)) /* valid user-specified VM ID */ {
				id = null;
			}
		}
		return id;
	}

	/**
	 * Creates the file artifacts for this VM
	 * @param newDisplayName Attach API display name for this VM
	 * @return false if the attach API was terminated before all file objects were created
	 * @throws IOException
	 */
	private boolean createFiles(String newDisplayName) throws IOException {
		try {
			/*[PR Jazz 31593] stress failures caused by long wait times for lock file ]*/
			if (CommonDirectory.tryObtainMasterLock()) {
				/* 
				 * Non-blocking lock attempt succeeded. 
				 * clean up garbage files from crashed processes or other failures.
				 * This operation is optional: if there is contention for the lock, 
				 * the first process in will do the cleanup.
				 */
				CommonDirectory.deleteStaleDirectories(pidProperty);
			} else {
				/* The following code needs to hold the lock, so go for a blocking lock. */
				CommonDirectory.obtainMasterLock();
			}
			if (LOGGING_DISABLED != loggingStatus) {
				IPC.logMessage("AttachHandler obtained master lock"); //$NON-NLS-1$
			}
			CommonDirectory.createNotificationFile();
			/*[PR Jazz 31593 create the file artifacts after we have cleaned the directory]*/
			if (isAttachApiTerminated()) { 
				return false; /* abort if we decided to shut down */
			}
			String myId = TargetDirectory.createMyDirectory(pidProperty, false);
			/*[PR RTC 80844 problem in initialization, or we are shutting down ]*/
			if (null == myId) { 
				return false;
			}
			setVmId(myId); /* may need to tweak the ID */
			setDisplayName(newDisplayName);
			CommonDirectory.openSemaphore();
			CommonDirectory.obtainAttachLock();
			Advertisement.createAdvertisementFile(getVmId(), newDisplayName);
		} finally {
			CommonDirectory.releaseAttachLock();
			CommonDirectory.releaseMasterLock();
		}
		return true;
	}

	@Override
	/**
	 * wait for attach requests
	 */
	public void run() {
		/* Set  the current thread as a System Thread */
		com.ibm.oti.vm.VM.markCurrentThreadAsSystem();

		synchronized (stateSync) { /* CMVC 161729 : shutdown hook may already be running */
			if (AttachStateValues.ATTACH_UNINITIALIZED == getAttachState()) {
				setAttachState(AttachStateValues.ATTACH_STARTING);
			} else {
				return;
			}
		}
		try {
			if (!initialize()) {
				IPC.logMessage("ERROR: failed to initialize"); //$NON-NLS-1$
				return;
			}
			WaitLoop waiter = new WaitLoop();
			synchronized (stateSync) { /* CMVC 161135. force stop if the VM is shutting down. */
				if (isAttachApiTerminated()) {
					return;
				} else {
					setAttachState(AttachStateValues.ATTACH_INITIALIZED);
					currentAttachThread = waiter;
				}
			}
			waiter.start();
		} catch (OutOfMemoryError e) {
			/* avoid anything which might allocate more memory, but indicate that the attach API is not viable */
			setAttachState(AttachStateValues.ATTACH_TERMINATED);
			return;
		} catch (IOException e) {
			setAttachState(AttachStateValues.ATTACH_TERMINATED);
			return;
		}	
	}

	private boolean initialize() throws IOException {
		String loggingProperty = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty(LOGGING_ENABLE_PROPERTY);
		String logName = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty(LOG_NAME_PROPERTY);
		if ((null != logName) && !logName.equals("")) { //$NON-NLS-1$
			logName = logName + '_';
		} else {
			logName = ""; //$NON-NLS-1$
		}
		
		nameProperty = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty(DISPLAYNAME_PROPERTY);
		pidProperty = validateVmId(com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty(VMID_PROPERTY));
		if ((null == pidProperty) || (0 == pidProperty.length())) {
			/* CMVC 161414 - PIDs and UIDs are long */
			long pid = IPC.getProcessId();
			pidProperty = Long.toString(pid);
		}
		if (null == nameProperty) {
			nameProperty = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("sun.java.command"); //$NON-NLS-1$
		}
		synchronized (IPC.accessorMutex) {
			if ((null == IPC.logStream) && (null != loggingProperty)
					&& loggingProperty.equalsIgnoreCase("yes")) { //$NON-NLS-1$
				File logFile = new File(logName + pidProperty + ".log"); //$NON-NLS-1$
				IPC.logStream = new PrintStream(logFile);
				IPC.setDefaultVmId(pidProperty);
				IPC.printMessageWithHeader("AttachHandler initialized", IPC.logStream); //$NON-NLS-1$
				loggingStatus = LOGGING_ENABLED;
			} else {
				loggingStatus = LOGGING_DISABLED;			
			}			
		}
		
		
		synchronized (stateSync) {
			if (isAttachApiTerminated()) {
				IPC.logMessage("cancel initialize before prepareCommonDirectory"); //$NON-NLS-1$
				return false;
			} else {
				/*[PR Jazz 30075] method renamed */
				CommonDirectory.prepareCommonDirectory();
			}
		}

		try {
			if (!createFiles(nameProperty)) {
				return false; /* attach API was terminated */
			}
		} catch (IOException e) {
			IPC.logMessage("AttachHandler IOException while creating files: ", e.getMessage()); //$NON-NLS-1$
			terminate(false);
			return false; /* CMVC 161135. Force the run() method to stop. */
		}
		
		File syncFileObjectTemp = TargetDirectory.getSyncFileObject();
		if (isAttachApiTerminated() || (null == syncFileObjectTemp)) {
			IPC.logMessage("cancel initialize before creating syncFileLock"); //$NON-NLS-1$
			return false;
		}		
		/* the syncFileObject was created by createFiles() (above) */
		syncFileLock = new FileLock(syncFileObjectTemp.getAbsolutePath(), TargetDirectory.SYNC_FILE_PERMISSIONS); 
		/* give other users read permission to the sync file */

		return true;
	}


	/**
	 * Try to read the reply. If it exists, connect to the attacher. This may be called from tryAttachTarget() in the case
	 * of a VM attaching to itself.
	 * @return an Attachment object if the reply file exists; null if it does not.
	 * @throws IOException if there is a problem reading the reply file.
	 */
	public Attachment connectToAttacher() throws IOException {
		String targetDirectoryPath = TargetDirectory.getTargetDirectoryPath(AttachHandler.getVmId());
		IPC.checkOwnerAccessOnly(targetDirectoryPath);
		Reply attacherReply = Reply.readReply(targetDirectoryPath);
		Attachment at = null;
		if (null != attacherReply) {
			int portNumber = attacherReply.getPortNumber();

			IPC.logMessage(notificationCount+" connectToAttacher reply on port ", portNumber); //$NON-NLS-1$
			if (portNumber >= 0) {
				at = new Attachment(mainHandler, attacherReply);
				addAttachment(at);
				at.start();
			}
		} else if (LOGGING_DISABLED != loggingStatus) {
			IPC.logMessage("connectToAttacher ", notificationCount, " waitForNotification no reply file"); //$NON-NLS-1$ //$NON-NLS-2$
		}
		return at;
	}

	/**
	 * delete advertising file, delete all attachments, wake up the attach handler thread if necessary
	 * @param wakeHandler true if the attach handler thread may be waiting on the semaphore.
	 * @return true if the caller should destroy the semaphore
	 */
	protected boolean terminate(boolean wakeHandler) {
		
		if (LOGGING_DISABLED != loggingStatus) {
		IPC.logMessage("AttachHandler terminate: Attach API is being shut down"); //$NON-NLS-1$
		}
		
		synchronized (stateSync) {
			/*[PR CMVC 161729 : shutdown hook may run before the initializer]*/
			AttachStateValues myAttachState = getAttachState();
			setAttachState(AttachStateValues.ATTACH_TERMINATED);
			switch (myAttachState) {
			case ATTACH_UNINITIALIZED: return false; /* never even started */
			case ATTACH_STARTING: break; /*/* may be anywhere in the initialization code.  Do the full shutdown */
			case ATTACH_INITIALIZED: break; /* go through the full shutdown */
			case ATTACH_TERMINATED: return false; /* already started terminating */
			default:
				IPC.logMessage("Unrecognized synchronization state "+stateSync.toString()); //$NON-NLS-1$
				break;
			}
		}
		currentAttachThread.interrupt(); /* do this after we change the attachState */
		if (wakeHandler) {
			if (LOGGING_DISABLED != loggingStatus) {
				IPC.logMessage("AttachHandler terminate removing contents of directory : ", TargetDirectory.getTargetDirectoryPath(getVmId())); //$NON-NLS-1$
			}
			TargetDirectory.deleteMyFiles(); /*[PR Jazz 58094] Leave the directory to keep the census accurate but prevent attachments */
		} else {
			if (LOGGING_DISABLED != loggingStatus) {
				IPC.logMessage("AttachHandler terminate removing directory : ", TargetDirectory.getTargetDirectoryPath(getVmId())); //$NON-NLS-1$
			}
			TargetDirectory.deleteMyDirectory(false);
		}
		for (Attachment a : attachments) {
			if (null != a) {
				a.teardown();
			}
		}
		FileLock.shutDown();
		
		return terminateWaitLoop(wakeHandler, 0);
	}

	/**
	 * Shut down the wait loop thread and determine if we still require the semaphore.
	 * @param wakeHandler true if the wait loop is waiting for a notification
	 * @param retryNumber indicates how many times we have tried to shut down the wait loop
	 * @return true if there are no other VMs using the semaphore
	 */
	/*[PR Jazz 50781: CMVC 201366: OTT:Attach API wait loop still alive after shutdown hooks ]*/
	static boolean terminateWaitLoop(boolean wakeHandler, int retryNumber) {
		boolean gotLock = false;
		boolean destroySemaphore = false;
		/*[PR CMVC 187777 : non-clean shutdown in life cycle tests]*/
		/*
		 * If multiple VMs shut down simultaneously, there is contention for the lock file.
		 * If someone else has the lock file, it is likely but not guaranteed 
		 * that they will post to the semaphore and wake up this VM.
		 * 
		 * Try to get the lock file, but give up after a timeout. If this VM is 
		 * notified and terminates in the meantime, don't bother notifying. 
		 * 
		 * Note that the master lock is not held while waiting to shut down.
		 */
		long lockDeadline = System.nanoTime() + shutdownTimeoutMs*1000000/10; /* let the file lock use only a fraction of the timeout budget */
		try {
			gotLock = CommonDirectory.tryObtainMasterLock();
			while (!gotLock && isWaitingForSemaphore()) {
				Thread.sleep(10);
				gotLock = CommonDirectory.tryObtainMasterLock();
				if (System.nanoTime() > lockDeadline) {
					break;
				}
			}
		} catch (InterruptedException e) {
			IPC.logMessage("InterruptedException while waiting to shut down", e); //$NON-NLS-1$
		}
		if (!isWaitingForSemaphore()) {
			wakeHandler = false; /* The wait loop is already shutting down */
			if (LOGGING_DISABLED != loggingStatus) {
				IPC.logMessage("VM already notified for termination, abandoning master lock"); //$NON-NLS-1$
			}
			if (gotLock) {
				/* 
				 * We grabbed the lock to notify the wait loop.  
				 * Release it because the wait loop does not need to be notified. 
				 */ 
				CommonDirectory.releaseMasterLock();
				gotLock = false;
			}
		}

		if (gotLock) {	
			try {
				if (LOGGING_DISABLED != loggingStatus) {
					IPC.logMessage("AttachHandler terminate obtained master lock"); //$NON-NLS-1$
				}
				/*
				 * If one or more directories is deleted, the target count is wrong and the wait loop may
				 * not be notified.  If the terminate fails, try adding extra counts to the semaphore.
				 */
				int numTargets = CommonDirectory.countTargetDirectories() + retryNumber;
				setNumberOfTargets(numTargets);
				if (numTargets <= 1) {
					/* 
					 * if this VM is the only one running, don't need to lock sync files.
					 * numTargets may be 0 if my directory was deleted accidentally.
					 */
					setDoCancelNotify(false); /* cancelNotify re-opens the semaphore */
					/* 
					 * CMVC 162086: destroy does not always interrupt a semaphore wait. 
					 * Therefore, notify myself 
					 */
					if (wakeHandler) {
						CommonDirectory.notifyVm(1, true); 
					}
					destroySemaphore = true;
				} else if (wakeHandler) {
					/*
					 * Defect 158515. Post to the semaphore to wake up the
					 * attach handler thread. The order that waiting processes
					 * wake up is non-deterministic, so post once for each
					 * process. The attach handler clears any unused posts once
					 * it wakes up.
					 */
					setDoCancelNotify(true); /* get the handler thread to close the semaphore */
					CommonDirectory.notifyVm(numTargets, true); 
					/* CMVC 162086. add an extra notification since the count won't include this VM: the advertisement directory is already deleted */
				}
			} finally {
				CommonDirectory.releaseMasterLock();
				if (LOGGING_DISABLED != loggingStatus) {
					IPC.logMessage("AttachHandler terminate released master lock"); //$NON-NLS-1$
				}
			}
		} else { 
			/*
			 * Either the handler has already terminated, or we cannot get the lock. In the latter case, clean up as best we can 
			 */
			IPC.logMessage("AttachHandler tryObtainMasterLock failed"); //$NON-NLS-1$
		}
		return destroySemaphore;
	}

	/**
	 * wind up the attach API on termination
	 */
	static final class teardownHook extends Thread {

		@Override
		public void run() {
			/*[PR CMVC 188652]  Suppress OOM messages from attach API*/
			try {
				Thread.currentThread().setName("Attach API teardown"); //$NON-NLS-1$
				/* Set  the current thread as a System Thread */
				com.ibm.oti.vm.VM.markCurrentThreadAsSystem();
				if (LOGGING_DISABLED != loggingStatus) {
					IPC.logMessage("shutting down attach API"); //$NON-NLS-1$
				}
				if (null == mainHandler) {
					return; /* the constructor failed */
				}
				long shutdownDeadlineNs = System.nanoTime() + shutdownTimeoutMs*1000000;
				boolean destroySemaphore = mainHandler.terminate(true);
				try {
					/*[PR CMVC 172177] Ensure the initializer thread has terminated */
					/*
					 * timeout in milliseconds.
					 * The initializer does not block, but under heavy load could take a long time to terminate.
					 */
					mainHandler.join(shutdownTimeoutMs/2); /* don't let the join use all the time */
					/*[PR CMVC 164479] AIX sometimes hangs in File.exists.  See if a ridiculously long timeout makes a difference*/
					/* 
					 * currentAttachThread is the same as mainHandler before the wait loop launches.
					 * In that case, the join is redundant but harmless. 
					 */
					int timeout = 100;
					int retryNumber = 0;
					while (System.nanoTime() < shutdownDeadlineNs) {
						currentAttachThread.join(timeout); /* timeout in milliseconds */
						if (currentAttachThread.getState() != Thread.State.TERMINATED) {
							IPC.logMessage("Timeout waiting for wait loop termination.  Retry #"+retryNumber); //$NON-NLS-1$
							timeout *= 2;
							AttachHandler.terminateWaitLoop(true, retryNumber);
							++retryNumber;
						} else {
							break;
						}
					}
				} catch (InterruptedException e) {
					IPC.logMessage("teardown join with attach handler interrupted"); //$NON-NLS-1$
				}
				if (currentAttachThread.getState() != Thread.State.TERMINATED) {
					IPC.logMessage("Attach API not terminated"); //$NON-NLS-1$
				}
				TargetDirectory.deleteMyDirectory(true); /*[PR Jazz 58094] terminate() cleared out the directory */
				/*[PR CMVC 161992] wait until the attach handler thread has finished before closing the semaphore*/
				if (destroySemaphore) {
					if (CommonDirectory.tryObtainMasterLock()) {
						/* if this fails, then another process became active after the VMs were counted */
						CommonDirectory.destroySemaphore();
						if (LOGGING_DISABLED != loggingStatus) {
							IPC.logMessage("AttachHandler destroyed semaphore"); //$NON-NLS-1$
						}
						CommonDirectory.releaseMasterLock();
					} else {
						if (LOGGING_DISABLED != loggingStatus) {
							IPC.logMessage("could not obtain lock, semaphore not destroyed"); //$NON-NLS-1$
						}
						CommonDirectory.closeSemaphore();
					}
				} else {
					CommonDirectory.closeSemaphore();
					if (LOGGING_DISABLED != loggingStatus) {
						IPC.logMessage("AttachHandler closed semaphore"); //$NON-NLS-1$ 
					}
				}
				if (null != IPC.logStream) {
					/* 
					 * Defer closing the file as long as possible.
					 * logStream is a PrintStream, which never throws IOException, so any further writes will be silently ignored.
					 */
					IPC.logStream.close();
				}
			} catch (OutOfMemoryError e) {
				/* 
				 * note that this may leave the wait loop running or fail to clean up file system artifacts.
				 * These are recoverable conditions, though.
				 */
				IPC.tracepoint(IPC.TRACEPOINT_STATUS_OOM_DURING_TERMINATE, e.getMessage());
				return;
			}
		} 
	}

	/**
	 * Checks if the attach API is initialized.  If not, it waits until it changes to the initialized or terminated state.
	 * This is called by an application thread. attachState is initialized to ATTACH_UNINITIALIZED during class initialization.
	 * 
	 * This is provided for the benefit of applications which use attach API to load JVMTI agents 
	 * into their own JVMs.
	 * 
	 * @return true if the attach API was successfully initialized, false if the API is stopped, encountered an error, or timed out.
	 */
	public static boolean waitForAttachApiInitialization() {
		boolean status;
		synchronized (stateSync) { /* prevent race condition with initialization and teardown code */
			AttachStateValues currentState = getAttachState();
			if (AttachStateValues.ATTACH_INITIALIZED == currentState) {
				status = true;
			} else if (AttachStateValues.ATTACH_TERMINATED == currentState) {
				status = false; /* initialization failed or was not enabled */
			} else {
				/* 
				 * normal state sequence: UNINITIALIZED->STARTING->INITIALIZED->TERMINATED. 
				 * May skip from UNINITIALIZED or STARTING directly to TERMINATED. 
				 */
				int waitCycles = 2; /* at most 2 state transitions are required to get to INITIALIZED. */
				status = false; /* default value */
				while (waitCycles > 0) { /*[PR CMVC 162715] code was being fooled by UNINITIALIZED->STARTING transitions */
					--waitCycles;
					try {
						/* assignments to stateSync cause a notifyAll on stateSync */
						stateSync.wait(100000); /* timeout value */
						currentState = getAttachState();
						switch (currentState) {
						case ATTACH_STARTING:
							break;
						case ATTACH_INITIALIZED :
							status = true;
							waitCycles = 0; /* break out of the loop */
							break;
						case ATTACH_TERMINATED:
							waitCycles = 0; /* break out of the loop */
							break;
						default: /* this shouldn't happen */
							waitCycles = 0; /* break out of the loop */
							break;
						}
					} catch (InterruptedException e) {
						break;
					}
				}
			}
		}
		return status;
	}

	/* ########################### Accessors ############################ */

	static void setNumberOfTargets(int numberOfTargets) {
		synchronized (accessorMutex) {
			AttachHandler.numberOfTargets = numberOfTargets;
		}
	}

	static int getNumberOfTargets() {
		synchronized (accessorMutex) {
			return AttachHandler.numberOfTargets;
		}
	}

	void addAttachment(Attachment at) {
		synchronized (accessorMutex) {
			attachments.add(at);
		}
	}

	/**
	 * the attachment calls this to remove itself from the list
	 * @param attachment Attachment object to remove
	 */
	void removeAttachment(Attachment attachment) {
		synchronized (accessorMutex) {
			int pos = attachments.indexOf(attachment);

			if (pos > 0) {
				attachments.remove(pos);
			}
		}
	}

	/**
	 * @return properties for agents
	 */
	static Properties getAgentProperties() {
		String agentPropertyNames[] = {
				IPC.LOCAL_CONNECTOR_ADDRESS,
				"sun.jvm.args", //$NON-NLS-1$
				"sun.jvm.flags", //$NON-NLS-1$
				"sun.java.command"			 //$NON-NLS-1$
		};
		Attachment.saveLocalConnectorAddress();
		//If we don't have a local connector address, just continue and get the others
		Properties agentProperties = new Properties();
		for (String pName: agentPropertyNames) {
			String pValue = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty(pName);
			if (null != pValue) {
				agentProperties.put(pName, pValue);
			}
		}
		return agentProperties;
	}

	/**
	 * This is provided for the benefit of applications which use attach API to load JVMTI agents 
	 * into their own JVMs.
	 * 
	 * @return True if the Attach API is set up and the VM can attach to other
	 *         VMs and receive attach requests.
	 */
	public static boolean isAttachApiInitialized() {
		return (AttachStateValues.ATTACH_INITIALIZED == getAttachState());
	}

	/*
	 * @return true if the attach API has not started launching (i.e. disabled)
	 * or is shutting down
	 * 
	 */
	public static boolean isAttachApiTerminated() {
		return (AttachStateValues.ATTACH_TERMINATED == getAttachState());
	}

	/**
	 * Retrieves the state in a thread-safe fashion
	 */
	private static AttachStateValues getAttachState() {
		synchronized (stateSync) {
			return attachState;
		}
	}

	/**
	 * @return flag indicating if the wait loop is waiting on the semaphore.
	 */
	static boolean isWaitingForSemaphore() {
		synchronized (stateSync) {
			return waitingForSemaphore;
		}
	}

	/**
	 * Set the waitingForSemaphore true if the attach API is not being shut down and return true,
	 * otherwise set it false and return false. This is thread safe and coordinated with the overall
	 * attach API state.
	 * 
	 * @return true is the attach API is not being shut down
	 */
	static boolean startWaitingForSemaphore() {
		synchronized (stateSync) {
			if (attachState != AttachStateValues.ATTACH_TERMINATED) {
				AttachHandler.waitingForSemaphore = true;
			} else {
				AttachHandler.waitingForSemaphore = false;
			}
			return waitingForSemaphore;
		}
	}

	/**
	 * clear the waiting for semaphore flag in a thread-safe fashion.
	 */
	static void endWaitingForSemaphore() {
		synchronized (stateSync) {
			AttachHandler.waitingForSemaphore = false;
		}
	}

	/**
	 * Updates the state in a thread-save fashion and alerts threads which may be waiting for a state change.
	 * @param attachState new state
	 */
	private static void setAttachState(AttachStateValues attachState) {
		synchronized (stateSync) {
			AttachHandler.attachState = attachState;
			stateSync.notifyAll();
		}
	}

	/**
	 * @return displayName human-friendly name for VM
	 */
	String getDisplayName() {
		synchronized (accessorMutex) { /* synchronized to force update of variables across threads */
			return displayName;
		}
	}

	/**
	 * @param displayName human-friendly name for VM
	 */
	void setDisplayName(String displayName) {
		synchronized (accessorMutex) { /* synchronized to force update of variables across threads */
			this.displayName = displayName;
		}
	}

	/**
	 * @return ignoreNotification sync object
	 */
	public Object getIgnoreNotification() {
		return ignoreNotification;
	}

	/**
	 * This field is static final, so no synchronization required.
	 * 
	 * @return the main AttachHandler created by the factory
	 */
	public static AttachHandler getMainHandler() {
		return mainHandler;
	}

	/**
	 * This is provided for the benefit of applications which use attach API to load JVMTI agents 
	 * into their own JVMs.
	 * 
	 * @return ID of this VM
	 */
	public static String getVmId() {
		synchronized (accessorMutex) {
			return vmId;
		}
	}

	private static void setVmId(String id) {
		synchronized (accessorMutex) { /* synchronized to force update of variables across threads */
			vmId = id;
		}
	}

	/**
	 * @return OS process ID
	 */
	/* CMVC 161414 - PIDs and UIDs are long */
	public static long getProcessId() {
		return IPC.getProcessId();
	}

	static boolean getDoCancelNotify() {
		synchronized (accessorMutex) { /* synchronized to force update of variables across threads */
			return doCancelNotify;
		}
	}

	static void setDoCancelNotify(boolean doCancel) {
		synchronized (accessorMutex) { /* synchronized to force update of variables across threads */
			AttachHandler.doCancelNotify = doCancel;
		}
	}
}
