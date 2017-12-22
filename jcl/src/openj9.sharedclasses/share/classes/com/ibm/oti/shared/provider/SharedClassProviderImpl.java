/*[INCLUDE-IF Sidecar19-SE & SharedClasses]*/
/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

package com.ibm.oti.shared.provider;

import java.net.URL;
import java.security.BasicPermission;
import java.util.function.IntConsumer;
import com.ibm.sharedclasses.spi.SharedClassProvider;
import com.ibm.oti.shared.CannotSetClasspathException;
import com.ibm.oti.shared.HelperAlreadyDefinedException;
import com.ibm.oti.shared.Shared;
import com.ibm.oti.shared.SharedClassHelperFactory;
import com.ibm.oti.shared.SharedClassPermission;
import com.ibm.oti.shared.SharedClassStatistics;
import com.ibm.oti.shared.SharedClassURLClasspathHelper;
import com.ibm.oti.shared.SharedClassURLHelper;

/**
 * Implementation of SharedClassProvider.
 */
public class SharedClassProviderImpl implements SharedClassProvider {
	static class IndexHolderImpl implements SharedClassURLClasspathHelper.IndexHolder {
		public int index;
		@Override
		public void setIndex(int value) {
			index = value;
		}
	}

	private SharedClassURLClasspathHelper sharedClassURLClasspathHelper;
	private SharedClassURLHelper sharedClassURLHelper;

	@Override
	public boolean isSharedClassEnabled() {
		return Shared.isSharingEnabled();
	}
	@Override
	public SharedClassProvider initializeProvider(ClassLoader loader, URL[] classpath) {
		return initializeProvider(loader, classpath, false, false);
	}
	@Override
	public SharedClassProvider initializeProvider(ClassLoader loader, URL[] classpath, boolean urlHelper, boolean tokenHelper) {
		if (!isSharedClassEnabled()) {
			return null;
		}
		SharedClassHelperFactory sharedClassHelperFactory = Shared.getSharedClassHelperFactory();
		if (null == sharedClassHelperFactory) {
			return null;
		}

		try {
			if (null != classpath) {
				sharedClassURLClasspathHelper = sharedClassHelperFactory.getURLClasspathHelper(loader, classpath);
				if (null == sharedClassURLClasspathHelper) {
					return null;
				}
			}
			if (urlHelper) {
				sharedClassURLHelper = sharedClassHelperFactory.getURLHelper(loader);
				if (null == sharedClassURLHelper) {
					return null;
				}
			}
 		} catch (HelperAlreadyDefinedException ex) {
			return null;
		}
		return this;
	}
	@Override
	public byte[] findSharedClassURL(URL path, String className){
		if (null == sharedClassURLHelper) {
			return null;
		}
		return sharedClassURLHelper.findSharedClass(path, className);
	}
	@Override
	public boolean storeSharedClassURL(URL path, Class<?> clazz){
		if (null == sharedClassURLHelper) {
			return false;
		}
		return sharedClassURLHelper.storeSharedClass(path, clazz);
	}
	@Override
	public byte[] findSharedClassURLClasspath(String className, IntConsumer indexConsumer) {
		if (null == sharedClassURLClasspathHelper) {
			return null;
		}
		IndexHolderImpl myHolder = null;

		if (null != indexConsumer) {
			myHolder = new IndexHolderImpl();
		}
		byte[] result = sharedClassURLClasspathHelper.findSharedClass(className, myHolder);

		if ((null != result)
			&& (null != indexConsumer)
			&& (null != myHolder)
		) {
			indexConsumer.accept(myHolder.index);
		}
		return result;
	}
	@Override
	public boolean storeSharedClassURLClasspath(Class<?> clazz, int foundAtIndex) {
		if (null == sharedClassURLClasspathHelper) {
			return false;
		}
		return sharedClassURLClasspathHelper.storeSharedClass(clazz, foundAtIndex);
	}
	@Override
	public boolean setURLClasspath(URL[] newClasspath) {
		if (null == sharedClassURLClasspathHelper) {
			return false;
		}
		try {
			sharedClassURLClasspathHelper.setClasspath(newClasspath);
			return true;
		} catch (CannotSetClasspathException ex) {
			return false;
		}
	}
	@Override
	public long getCacheSize() {
		if (isSharedClassEnabled()) {
			return SharedClassStatistics.maxSizeBytes();
		} else {
			return 0;
		}
	}
	@Override
	public long getSoftmxBytes() {
		if (isSharedClassEnabled()) {
			return SharedClassStatistics.softmxBytes();
		} else {
			return -1;
		}
	}
	@Override
	public long getMinAotBytes() {
		if (isSharedClassEnabled()) {
			return SharedClassStatistics.minAotBytes();
		} else {
			return -1;
		}
	}
	@Override
	public long getMaxAotBytes() {
		if (isSharedClassEnabled()) {
			return SharedClassStatistics.maxAotBytes();
		} else {
			return -1;
		}
	}
	@Override
	public long getMinJitDataBytes() {
		if (isSharedClassEnabled()) {
			return SharedClassStatistics.minJitDataBytes();
		} else {
			return -1;
		}
	}
	@Override
	public long getMaxJitDataBytes() {
		if (isSharedClassEnabled()) {
			return SharedClassStatistics.maxJitDataBytes();
		} else {
			return -1;
		}
	}
	@Override
	public long getFreeSpace() {
		if (isSharedClassEnabled()) {
			return SharedClassStatistics.freeSpaceBytes();
		} else {
			return 0;
		}
	}
	@Override
	public BasicPermission createPermission(String classLoaderClassName, String actions) {
		if (!isSharedClassEnabled()) {
			return null;
		}
		return new SharedClassPermission(classLoaderClassName, actions);
	}
}
