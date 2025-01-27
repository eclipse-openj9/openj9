/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2009
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
package com.ibm.tools.attach.attacher;

import java.io.File;
import java.io.IOException;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import java.util.ArrayList;
import java.util.List;

import openj9.internal.tools.attach.target.Advertisement;
import openj9.internal.tools.attach.target.AttachHandler;
import openj9.internal.tools.attach.target.CommonDirectory;
import openj9.internal.tools.attach.target.IPC;
import openj9.internal.tools.attach.target.TargetDirectory;
import com.sun.tools.attach.AttachNotSupportedException;
/*[IF JAVA_SPEC_VERSION < 24]*/
import com.sun.tools.attach.AttachPermission;
/*[ENDIF] JAVA_SPEC_VERSION < 24 */
import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.spi.AttachProvider;

/**
 * Concrete subclass of the class that lists the available target VMs
 *
 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
@SuppressWarnings("removal")
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
public class OpenJ9AttachProvider extends AttachProvider {

	/**
	 * Creates an IPC object
	 */
	public OpenJ9AttachProvider() {
		super();
	}

	@Override
	public OpenJ9VirtualMachine attachVirtualMachine(String id)
			throws AttachNotSupportedException, IOException {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		checkAttachSecurity();
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		try {
			OpenJ9VirtualMachine vm = new OpenJ9VirtualMachine(this, id);
			IPC.logMessage("Attach target id: " + id); //$NON-NLS-1$
			vm.attachTarget();
			return vm;
		} catch (NullPointerException e) {
		/* constructor throws an NPE if the ID or name is not set.
		Turn this into an exception the  application is expecting */
			/*[MSG "K0554", "Virtual machine ID or display name is null"]*/
			AttachNotSupportedException exc = new AttachNotSupportedException(com.ibm.oti.util.Msg.getString("K0554")); //$NON-NLS-1$
			exc.initCause(e);
			throw exc;
		}
	}

	@Override
	public OpenJ9VirtualMachine attachVirtualMachine (
			VirtualMachineDescriptor descriptor)
			throws AttachNotSupportedException, IOException {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		checkAttachSecurity();
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		if (!(descriptor.provider() instanceof OpenJ9AttachProvider)) {
			/*[MSG "K0543", "Virtual provider does not match"]*/
			throw new AttachNotSupportedException(com.ibm.oti.util.Msg.getString("K0543")); //$NON-NLS-1$
		}

		String id = descriptor.id();
		OpenJ9VirtualMachine vm = new OpenJ9VirtualMachine(this, id);
		IPC.logMessage("Attach target descriptor.id(): " + id); //$NON-NLS-1$
		vm.attachTarget();
		return vm;
	}

	@Override
	public List<VirtualMachineDescriptor> listVirtualMachines() {
		List<VirtualMachineDescriptor> ret = null;
		PrivilegedExceptionAction<List<VirtualMachineDescriptor>> action = () -> listVirtualMachinesImp();
		try {
			ret = AccessController.doPrivileged(action);
		} catch (PrivilegedActionException e) {
			Throwable cause = e.getCause();
			if (cause instanceof RuntimeException) {
				throw (RuntimeException) cause;
			} else if (cause instanceof Error) {
				throw (Error) cause;
			} else {
				throw new RuntimeException(cause);
			}
		}
		return ret;
	}

	private List<VirtualMachineDescriptor> listVirtualMachinesImp() {
		AttachHandler.waitForAttachApiInitialization(); /* ignore result: we can list targets if API is disabled */
		/* Figure out where the IPC metadata lives and validate */
		File commonDir = CommonDirectory.getCommonDirFileObject();
		ArrayList<VirtualMachineDescriptor> descriptors = new ArrayList<>();
		if (null == commonDir) {
			IPC.logMessage("listVirtualMachines() error getting common directory"); //$NON-NLS-1$
			return null; /* indicate an error */
		} else if (!commonDir.exists()) {
			IPC.logMessage("listVirtualMachines() common directory is absent, expected " + commonDir.getAbsolutePath()); //$NON-NLS-1$
			return descriptors; /*[PR 103332 - common dir will not exist if attach API is disabled */
		} else if (!commonDir.isDirectory()) { /* Cleanup. handle case where couldn't open common dir. */
			IPC.logMessage("listVirtualMachines() common directory is mis-configured for " + commonDir.getAbsolutePath()); //$NON-NLS-1$
			return null; /* Configuration error */
		}

		try {
			/*[PR 164751 avoid scanning the directory when an attach API is launching ]*/
			CommonDirectory.obtainControllerLock("OpenJ9AttachProvider.listVirtualMachinesImp"); //$NON-NLS-1$
		} catch (IOException e) { /*[PR 164751 avoid scanning the directory when an attach API is launching ]*/
			/*
			 * IOException is thrown if we already have the lock. The only other cases where we lock this file are during startup and shutdown.
			 * The attach API startup is complete, thanks to waitForAttachApiInitialization() and threads using this method terminate before shutdown.
			 */
			IPC.logMessage("listVirtualMachines() IOError on controller lock : ", e.toString()); //$NON-NLS-1$
			return descriptors; /* An error has occurred. Since the attach API is not working correctly, be conservative and don't list and targets */
		}
		try {
			File[] vmDirs = commonDir.listFiles();
			if (vmDirs == null) {
				/* an IOException on listFiles will cause vmDirs to be null */
				return descriptors;
			}

			long myUid = IPC.getUid();
			/* Iterate through the files in the directory */
			for (File f : vmDirs) {

				/* skip files */
				if (!f.isDirectory() || !CommonDirectory.isFileOwnedByUid(f, myUid)) {
					continue;
				}

				boolean staleDirectory = true;
				File advertisement = new File(f, Advertisement.getFilename());
				long uid = 0;
				if (advertisement.exists()) {
					OpenJ9VirtualMachineDescriptor descriptor = OpenJ9VirtualMachineDescriptor.fromAdvertisement(this, advertisement);
					if (null != descriptor) {
						long pid = descriptor.getProcessId();
						uid = descriptor.getUid();
						if ((0 == pid) || IPC.processExists(pid)) {
							descriptors.add(descriptor);
							staleDirectory = false;
						}
					}
					/*[PR Jazz 30110 advertisement is from an older version or is corrupt.  get the owner via file stat ]*/
					if ((myUid != 0) && (0 == uid)) {
						/*
						 * If this process's UID is 0, then it is root and should ignore file ownership and clean up everyone's files.
						 * If getFileOwner fails, the uid will appear to be -1, and non-root users will ignore it.
						 * CommonDirectory.deleteStaleDirectories() will handle the case of a target directory which does not have an advertisement directory.
						 */
						uid = CommonDirectory.getFileOwner(advertisement.getAbsolutePath());
					}
				}

				/*[PR Jazz 22292 do not delete files the process does not own, unless the process is running as root ]*/
				if (staleDirectory && ((myUid == 0) || (uid == myUid))) {
					IPC.logMessage("listVirtualMachines() removing stale directory : ", f.getName()); //$NON-NLS-1$
					TargetDirectory.deleteTargetDirectory(f.getName());
				}
			}
		} finally {
			/* guarantee that we unlock the file */
			CommonDirectory.releaseControllerLock("OpenJ9AttachProvider.listVirtualMachinesImp"); //$NON-NLS-1$
		}
		return descriptors;
	}

	/**
	 * @param id
	 *            VM ID of target
	 * @return descriptor of target
	 */
	VirtualMachineDescriptor getDescriptor(String id) {
		List<VirtualMachineDescriptor> vmds = listVirtualMachines();
		if (null != vmds) {
			for (VirtualMachineDescriptor vmd : vmds) {
				if (vmd.id().equalsIgnoreCase(id)) {
					return vmd;
				}
			}
		}
		return null;
	}

	@Override
	public String name() {

		return "IBM"; //$NON-NLS-1$
	}

	@Override
	public String type() {
		return "Java SE"; //$NON-NLS-1$
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	private static void checkAttachSecurity() {
		@SuppressWarnings("removal")
		final SecurityManager securityManager = System.getSecurityManager();
		if (securityManager != null) {
			securityManager.checkPermission(Permissions.ATTACH_VM);
		}
	}
	static class Permissions {

		/**
		 * Ability to attach to another Java virtual machine and load agents into
		 * that VM.
		 */
		final static AttachPermission ATTACH_VM = new AttachPermission(
				"attachVirtualMachine", null); //$NON-NLS-1$;
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */
}
