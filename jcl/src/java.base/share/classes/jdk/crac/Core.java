/*[INCLUDE-IF CRAC_SUPPORT]*/
/*
 * Copyright IBM Corp. and others 2024
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
package jdk.crac;

import java.nio.file.Paths;

import openj9.internal.criu.InternalCRIUSupport;
import openj9.internal.criu.JVMCheckpointException;
import openj9.internal.criu.JVMRestoreException;

/**
 * The CRIU Support API.
 */
public class Core {
	private static CRIUSupportContext<?> globalContext;

	/**
	 * Get a global context.
	 *
	 * @return a Context
	 */
	public static Context<Resource> getGlobalContext() {
		if ((globalContext == null) && InternalCRIUSupport.isCRaCSupportEnabled()) {
			try {
				globalContext = new CRIUSupportContext<>();
			} catch (IllegalArgumentException e) {
				System.err.println("Invalid checkpoint directory supplied: " + InternalCRIUSupport.getCRaCCheckpointToDir()); //$NON-NLS-1$
				throw e;
			}
		}
		return (Context<Resource>)globalContext;
	}

	/**
	 * Invoke InternalCRIUSupport with default settings.
	 *
	 * @throws CheckpointException if this failed with a CheckpointException
	 * @throws RestoreException    if this failed with a RestoreException
	 */
	public static void checkpointRestore() throws CheckpointException, RestoreException {
		if (InternalCRIUSupport.isCRaCSupportEnabled()) {
			try {
				((CRIUSupportContext<?>)getGlobalContext()).checkpointJVM();
			} catch (Throwable t) {
				throw new CheckpointException(t);
			}
		} else {
			throw new UnsupportedOperationException("CRaC support is not enabled"); //$NON-NLS-1$
		}
	}
}

class CRIUSupportContext<R extends Resource> extends Context<R> {
	// InternalCRIUSupport.getCRaCCheckpointToDir() is not null if
	// InternalCRIUSupport.isCRaCSupportEnabled() returns true before creating CRIUSupportContext<>().
	private final InternalCRIUSupport internalCRIUSupport = new InternalCRIUSupport(
			Paths.get(InternalCRIUSupport.getCRaCCheckpointToDir()))
			.setLeaveRunning(false)
			.setShellJob(true)
			.setTCPEstablished(true)
			.setFileLocks(true);

	@Override
	public void register(R resource) throws Exception {
		if (Context.debug) {
			System.out.print("Register: " + resource); //$NON-NLS-1$
		}

		try {
			internalCRIUSupport.registerPreCheckpointHook(() -> {
				try {
					resource.beforeCheckpoint(this);
				} catch (Throwable t) {
					throw new JVMCheckpointException(t.getMessage(), 0, t.getCause());
				}
			}, InternalCRIUSupport.HookMode.CONCURRENT_MODE, InternalCRIUSupport.LOWEST_USER_HOOK_PRIORITY);
			internalCRIUSupport.registerPostRestoreHook(() -> {
				try {
					resource.afterRestore(this);
				} catch (Throwable t) {
					throw new JVMRestoreException(t.getMessage(), 0, t.getCause());
				}
			}, InternalCRIUSupport.HookMode.CONCURRENT_MODE, InternalCRIUSupport.LOWEST_USER_HOOK_PRIORITY);
		} catch (JVMCheckpointException jce) {
			throw new CheckpointException(jce);
		} catch (JVMRestoreException jre) {
			throw new RestoreException(jre);
		}
	}

	@Override
	public void beforeCheckpoint(Context<? extends Resource> resource) throws CheckpointException {
		throw new UnsupportedOperationException("CRaC CRIUSupportContext.beforeCheckpoint() is not supported"); //$NON-NLS-1$
	}

	@Override
	public void afterRestore(Context<? extends Resource> resource) throws RestoreException {
		throw new UnsupportedOperationException("CRaC CRIUSupportContext.afterRestore() is not supported"); //$NON-NLS-1$
	}

	public void checkpointJVM() {
		try {
			internalCRIUSupport.checkpointJVM();
		} catch (Exception e) {
			e.printStackTrace();
			throw e;
		}

	}
}
