/*[INCLUDE-IF CRIU_SUPPORT]*/
/*
 * Copyright IBM Corp. and others 2023
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
package openj9.internal.criu;

import openj9.internal.criu.InternalCRIUSupport;
import openj9.internal.criu.security.CRIUConfigurator;
import openj9.internal.criu.InternalCRIUSupport.HookMode;

/**
 * Handles the security providers.
 * The CRIUSECProvider is a security provider that is used as follows when CRIU
 * is enabled. During the checkpoint phase, all security providers are removed
 * from the system properties (which are read from security.java) and CRIUSEC is
 * added to the system properties. The pre-checkpoint hook clears the digests,
 * to ensure that no state is saved during checkpoint that would be restored
 * during the restore phase. During the resore phase, CRIUSEC is removed from
 * the provider list and the other security providers are added back to the
 * system properties. A new provider list is created from the system properties.
 */
public final class SecurityProviders {

	private SecurityProviders() {}

	/**
	 * Resets all cryptographic state within the CRIUSEC provider that
	 * was accumulated during checkpoint.
	 */
	public static void registerResetCRIUState() {
		J9InternalCheckpointHookAPI.registerPreCheckpointHook(
				InternalCRIUSupport.HookMode.SINGLE_THREAD_MODE, InternalCRIUSupport.RESET_CRIUSEC_PRIORITY,
				"Reset the digests", //$NON-NLS-1$
				() -> openj9.internal.criu.CRIUSECProvider.resetCRIUSEC()
		);
	}

	/**
	 * Adds the security providers during restore.
	 */
	public static void registerRestoreSecurityProviders() {
		J9InternalCheckpointHookAPI.registerPostRestoreHook(
				InternalCRIUSupport.HookMode.SINGLE_THREAD_MODE, InternalCRIUSupport.RESTORE_SECURITY_PROVIDERS_PRIORITY,
				"Restore the security providers", //$NON-NLS-1$
				() -> {
					if (!InternalCRIUSupport.isCheckpointAllowed()) {
						CRIUConfigurator.setCRIURestoreMode();
					}
				});
	}
}
