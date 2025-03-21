/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

/*
 * Copyright IBM Corp. and others 1998
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

import java.net.URL;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

import com.ibm.oti.util.Msg;

/**
 * Implementation of SharedClassURLHelper.
 * <p>
 * @see SharedClassURLHelper
 * @see SharedClassHelperFactory
 * @see SharedClassAbstractHelper
 */
final class SharedClassURLHelperImpl extends SharedClassAbstractHelper implements SharedClassURLHelper {
	/* Not public - should only be created by factory */
	private boolean minimizeUpdateChecks;

	/* Used to keep track of new jar files */
	private Set<String> jarFileNameCache = null;

	/*[PR VMDESIGN 1080] set to "true" in builds in which VMDESIGN 1080 is finished. */
	// This field is examined using the CDS adaptor
	public static final boolean MINIMIZE_ENABLED = true;

	/*[IF JAVA_SPEC_VERSION >= 24]*/
	SharedClassURLHelperImpl(ClassLoader loader, int id) {
		initialize(loader, id);
		initializeShareableClassloader(loader);
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 24 */
	SharedClassURLHelperImpl(ClassLoader loader, int id, boolean canFind, boolean canStore) {
		initialize(loader, id, canFind, canStore);
		initializeShareableClassloader(loader);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 24 */

	private static native void init();

	/* The passed-in URL path contains a jar file */
	private boolean newJarFileCheck(URL convertedJarPath) {
		String protocol = convertedJarPath.getProtocol();
		String jarFile = convertedJarPath.getFile();
		boolean isJarType = false;

		if (protocol.equalsIgnoreCase("jar")) { //$NON-NLS-1$
			isJarType = true;
		} else if (protocol.equalsIgnoreCase("file")) { //$NON-NLS-1$
			if (jarFile.endsWith(".jar") || jarFile.endsWith(".zip") //$NON-NLS-1$ //$NON-NLS-2$
			|| jarFile.contains("!/") || jarFile.contains("!\\") //$NON-NLS-1$ //$NON-NLS-2$
			) {
				isJarType = true;
			}
		}

		if (null == jarFileNameCache) {
			jarFileNameCache = ConcurrentHashMap.newKeySet();
		}

		if (isJarType) {
			return jarFileNameCache.add(jarFile);
		}

		return false;
	}

	private native boolean findSharedClassImpl3(int loaderId, String partition, String className, ClassLoader loader, URL url,
			boolean doFind, boolean doStore, byte[] romClassCookie, boolean newJarFile, boolean minUpdateChecks);

	private native boolean storeSharedClassImpl3(int idloaderId, String partition, ClassLoader loader, URL url, Class<?> clazz, boolean newJarFile, boolean minUpdateChecks, byte[] flags);

	static {
		init();
	}

	@Override
	public boolean setMinimizeUpdateChecks() {
		minimizeUpdateChecks = true;
		return true;
	}

	@Override
	public byte[] findSharedClass(URL path, String className) {
		return findSharedClass(null, path, className);
	}

	@Override
	public byte[] findSharedClass(String partition, URL path, String className) {
		ClassLoader loader = getClassLoader();
		if (loader == null) {
			/*[MSG "K059f", "ClassLoader has been garbage collected. Returning null."]*/
			printVerboseInfo(Msg.getString("K059f")); //$NON-NLS-1$
			return null;
		}
		/*[IF JAVA_SPEC_VERSION < 24]*/
		if (!canFind) {
			return null;
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		if (path==null) {
			/*[MSG "K05b3", "Cannot call findSharedClass with null URL. Returning null."]*/
			printVerboseError(Msg.getString("K05b3")); //$NON-NLS-1$
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
			synchronized(this) {
				doFind = theFilter.acceptFind(className);
				/* Don't invoke the store filter if the cache is full */
				if (nativeFlags[CACHE_FULL_FLAG] == 0) {
					doStore = theFilter.acceptStore(className);
				} else {
					doStore = true;
				}
			}
		} else {
			doFind = true;
			doStore = true;
		}
		URL convertedPath = convertJarURL(path);
		/* Too expensive to touch every URL to see if it exists. Leave this check to the cache. */
		if (!validateURL(convertedPath, false)) {
			return null;
		}

		byte[] romClassCookie = new byte[ROMCLASS_COOKIE_SIZE];
		boolean newJarFile = minimizeUpdateChecks ? false : newJarFileCheck(convertedPath);
		boolean found = findSharedClassImpl3(this.id, partition, className, loader, convertedPath, doFind, doStore, romClassCookie, newJarFile, minimizeUpdateChecks);
		if (!found) {
			return null;
		}
		return romClassCookie;
	}

	@Override
	public boolean storeSharedClass(URL path, Class<?> clazz) {
		return storeSharedClass(null, path, clazz);
	}

	@Override
	public boolean storeSharedClass(String partition, URL path, Class<?> clazz) {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		if (!canStore) {
			return false;
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		if (path==null) {
			/*[MSG "K05b4", "Cannot call storeSharedClass with null URL. Returning false."]*/
			printVerboseError(Msg.getString("K05b4")); //$NON-NLS-1$
			return false;
		}
		if (clazz==null) {
			/*[MSG "K05a3", "Cannot call storeSharedClass with null Class. Returning false."]*/
			printVerboseError(Msg.getString("K05a3")); //$NON-NLS-1$
			return false;
		}
		URL convertedPath = convertJarURL(path);
		/* Too expensive to touch every URL to see if it exists. Leave this check to the cache. */
		if (!validateURL(convertedPath, false)) {
			return false;
		}

		ClassLoader actualLoader = getClassLoader();
		if (!validateClassLoader(actualLoader, clazz)) {
			/* Attempt to call storeSharedClass with class defined by a different classloader */
			return false;
		}
		boolean newJarFile = minimizeUpdateChecks ? false : newJarFileCheck(convertedPath);
		return storeSharedClassImpl3(this.id, partition, actualLoader, convertedPath, clazz, newJarFile, minimizeUpdateChecks, nativeFlags);
	}

	@Override
	String getHelperType() {
		return "SharedClassURLHelper"; //$NON-NLS-1$
	}
}
