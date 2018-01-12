/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

import com.ibm.oti.util.Msg;
/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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

/**
 * Implementation of SharedClassTokenHelper.
 * <p>
 * @see SharedClassTokenHelper
 * @see SharedClassHelperFactory
 * @see SharedClassAbstractHelper
 */
final class SharedClassTokenHelperImpl extends SharedClassAbstractHelper implements SharedClassTokenHelper {
	/* Not public - should only be created by factory */
	SharedClassTokenHelperImpl(ClassLoader loader, int id, boolean canFind, boolean canStore) {
		initialize(loader, id, canFind, canStore);
		initializeShareableClassloader(loader);
	}

	private native boolean findSharedClassImpl2(int loaderId, String className, ClassLoader loader, String token, 
			boolean doFind, boolean doStore, byte[] romClassCookie);

	private native boolean storeSharedClassImpl2(int loaderId, ClassLoader loader, String token, Class<?> clazz, byte[] flags);

	@Override
	public synchronized byte[] findSharedClass(String token, String className) {
		ClassLoader loader = getClassLoader();
		if (loader == null) {
			/*[MSG "K059f", "ClassLoader has been garbage collected. Returning null."]*/
			printVerboseInfo(Msg.getString("K059f")); //$NON-NLS-1$
			return null;
		}
		if (!canFind) {
			return null;
		}
		if (token==null) {
			/*[MSG "K05a0", "Cannot call findSharedClass with null token. Returning null."]*/
			printVerboseError(Msg.getString("K05a0")); //$NON-NLS-1$
			return null;
		}
		if (className==null) {
			/*[MSG "K05a1", "Cannot call findSharedClass with null class name. Returning null."]*/
			printVerboseError(Msg.getString("K05a1")); //$NON-NLS-1$
			return null;
		}
		SharedClassFilter theFilter = getSharingFilter();
		boolean doFind, doStore;
		if (theFilter!=null) {
			doFind = theFilter.acceptFind(className);
			/* Don't invoke the store filter if the cache is full */
			if (nativeFlags[CACHE_FULL_FLAG] == 0) {
				doStore = theFilter.acceptStore(className);
			} else {
				doStore = true;
			}
		} else {
			doFind = true;
			doStore = true;
		}
		byte[] romClassCookie = new byte[ROMCLASS_COOKIE_SIZE];
		boolean found = findSharedClassImpl2(this.id, className, loader, token, doFind, doStore, romClassCookie);
		if (!found) {
			return null;
		}
		return romClassCookie;
	}

	@Override
	public synchronized boolean storeSharedClass(String token, Class<?> clazz) {
		if (!canStore) {
			return false;
		}
		if (token==null) {
			/*[MSG "K05a2", "Cannot call storeSharedClass with null token. Returning false."]*/
			printVerboseError(Msg.getString("K05a2")); //$NON-NLS-1$
			return false;
		}
		if (clazz==null) {
			/*[MSG "K05a3", "Cannot call storeSharedClass with null Class. Returning false."]*/
			printVerboseError(Msg.getString("K05a3")); //$NON-NLS-1$
			return false;
		}
		ClassLoader actualLoader = getClassLoader();
		if (!validateClassLoader(actualLoader, clazz)) {
			/* Attempt to call storeSharedClass with class defined by a different classloader */
			return false;
		}
		return storeSharedClassImpl2(this.id, actualLoader, token, clazz, nativeFlags);
	}
	
	@Override
	String getHelperType() {
		return "SharedClassTokenHelper"; //$NON-NLS-1$
	}
}
