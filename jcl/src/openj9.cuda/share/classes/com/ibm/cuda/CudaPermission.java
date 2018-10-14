/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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
package com.ibm.cuda;

import java.security.BasicPermission;
import java.security.Permission;

import com.ibm.cuda.CudaDevice.CacheConfig;
import com.ibm.cuda.CudaDevice.Limit;
import com.ibm.cuda.CudaDevice.SharedMemConfig;

/**
 * This class defines CUDA permissions as described in the table below.
 *
 * <table border=1 cellpadding=5>
 * <caption>CUDA Permissions</caption>
 * <tr>
 * <th align='left'>Permission Name</th>
 * <th align='left'>Allowed Action</th>
 * </tr>
 * <tr>
 * <td>configureDeviceCache</td>
 * <td>Configuring the cache of a device.
 *     See {@link CudaDevice#setCacheConfig(CacheConfig)}.</td>
 * </tr>
 * <tr>
 * <td>configureDeviceSharedMemory</td>
 * <td>Configuring the shared memory of a device.
 *     See {@link CudaDevice#setSharedMemConfig(SharedMemConfig)}.</td>
 * </tr>
 * <tr>
 * <td>loadModule</td>
 * <td>Loading a GPU code module onto a device.
 *     See {@link CudaModule}.</td>
 * </tr>
 * <tr>
 * <td>peerAccess.disable</td> <td>Disabling peer access from one device to another.
 *     See {@link CudaDevice#disablePeerAccess(CudaDevice)}.</td>
 * </tr>
 * <tr>
 * <td>peerAccess.enable</td>
 * <td>Enabling peer access from one device to another.
 *     See {@link CudaDevice#enablePeerAccess(CudaDevice)}.</td>
 * </tr>
 * <tr>
 * <td>setDeviceLimit</td>
 * <td>Setting a device limit.
 *     See {@link CudaDevice#setLimit(Limit, long)}.</td>
 * </tr>
 * </table>
 */
public final class CudaPermission extends BasicPermission {

	private static final long serialVersionUID = 5769765985242116236L;

	static final Permission DisablePeerAccess = new CudaPermission("peerAccess.disable"); //$NON-NLS-1$

	static final Permission EnablePeerAccess = new CudaPermission("peerAccess.enable"); //$NON-NLS-1$

	static final Permission LoadModule = new CudaPermission("loadModule"); //$NON-NLS-1$

	static final Permission SetCacheConfig = new CudaPermission("configureDeviceCache"); //$NON-NLS-1$

	static final Permission SetLimit = new CudaPermission("setDeviceLimit"); //$NON-NLS-1$

	static final Permission SetSharedMemConfig = new CudaPermission("configureDeviceSharedMemory"); //$NON-NLS-1$

	/**
	 * Create a representation of the named permissions.
	 *
	 * @param name
	 *          name of the permission
	 */
	public CudaPermission(String name) {
		super(name);
	}

	/**
	 * Create a representation of the named permissions.
	 *
	 * @param name
	 *          name of the permission
	 * @param actions
	 *          not used, must be null or an empty string
	 * @throws IllegalArgumentException
	 *          if actions is not null or an empty string
	 */
	public CudaPermission(String name, String actions) {
		super(name, actions);

		// ensure that actions is null or an empty string
		if (!((null == actions) || actions.isEmpty())) {
			throw new IllegalArgumentException();
		}
	}
}
