/*[INCLUDE-IF JAVA_SPEC_VERSION >= 9]*/
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
package com.ibm.gpu.internal;

import java.security.AccessController;
import java.security.PrivilegedAction;

import com.ibm.gpu.CUDAManager;
import com.ibm.gpu.spi.GPUAssist;

/**
 * Provides CUDA-based GPU sort assist depending on the values
 * of the following system properties:
 * <ul>
 *   <li>com.ibm.gpu.enforce</li>
 *   <li>com.ibm.gpu.enable</li>
 *   <li>com.ibm.gpu.disable</li>
 * </ul>
 * See CUDAManager for more details.
 */
public final class CudaGPUAssistProvider implements GPUAssist.Provider {

	/**
	 * The default constructor, as required to be a service provider.
	 */
	public CudaGPUAssistProvider() {
		super();
	}

	@Override
	/*[IF JAVA_SPEC_VERSION >= 17]*/
	@SuppressWarnings("removal")
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	public GPUAssist getGPUAssist() {
		PrivilegedAction<CUDAManager> getInstance = () -> CUDAManager.instance();
		CUDAManager manager = AccessController.doPrivileged(getInstance);

		if (manager.isSortEnabledOnGPU() || manager.isSortEnforcedOnGPU()) {
			if (manager.getDeviceCount() > 0) {
				return new CudaGPUAssist(manager);
			}
		}

		return null;
	}

}
