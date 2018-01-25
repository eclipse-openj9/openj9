/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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

import java.util.*;
import com.ibm.oti.shared.SharedClassesNamedPermission.SharedPermissions;

/**
 * SharedClassUtilities provides APIs to get information about all shared class caches in a directory and
 * to destroy a particular shared class cache.   
 * <p>
 * 
 * @see SharedClassCacheInfo 
 *
 */
public class SharedClassUtilities {
	
	/**
	 * Value to be passed to <code>flags</code> parameter of the {@link SharedClassUtilities#getSharedCacheInfo} method.
	 */
	static final public int NO_FLAGS = 0;
	/**
	 * Uses the platform dependent default value as the cache type.
	 */
	static final public int PERSISTENCE_DEFAULT = 0;
	/**
	 * Specifies a persistent cache.
	 */
	static final public int PERSISTENT = 1;
	/**
	 * Specifies a non-persistent cache.
	 */
	static final public int NONPERSISTENT = 2;
	/**
	 * Specifies a cache snapshot. 
	 */
	static final public int SNAPSHOT = 5;	/* Set it to 5 as J9PORT_SHR_CACHE_TYPE_SNAPSHOT is defined to be 5 in shchelp.h */
	/**
	 * Returned by {@link SharedClassUtilities#destroySharedCache} to indicate either no cache exists
	 * or the method has successfully destroyed caches of all generations.
	 */
	static final public int DESTROYED_ALL_CACHE = 0;
	/**
	 * Returned by {@link SharedClassUtilities#destroySharedCache} to indicate that the method failed to destroy any cache.
	 */
	static final public int DESTROYED_NONE = -1;
	/**
	 * Returned by {@link SharedClassUtilities#destroySharedCache} to indicate that the method has failed to destroy the 
	 * current generation cache.
	 */
	static final public int DESTROY_FAILED_CURRENT_GEN_CACHE = -2;
	/**
	 * Returned by {@link SharedClassUtilities#destroySharedCache} to indicate that the method has failed to destroy one
	 * or more older generation caches, and either a current generation cache does not exist or is successfully destroyed.
	 */
	static final public int DESTROY_FAILED_OLDER_GEN_CACHE = -3;
	/**
	 * Returned by native methods to indicate shared class utilities support is disabled.
	 */
	static final private int SHARED_CLASSES_UTILITIES_DISABLED = -255;
	
	private static native void init(); 
	private static native int getSharedCacheInfoImpl(String cacheDir, int flags, boolean useCommandLineValues, List<SharedClassCacheInfo> arrayList);
	private static native int destroySharedCacheImpl(String cacheDir, int cacheType, String cacheName, boolean useCommandLineValues);
	
	static {
		init();
	}
	
	/**
	 * Iterates through all shared class caches present in the given directory and returns their information in
	 * a {@link List}.
	 * <p>
	 * If <code>useCommandLineValues</code> is <code>true</code> then use the command line value as the directory to search in.
	 * If the command line value is not available, use the platform dependent default value.
	 * If <code>useCommandLineValues</code> is <code>false</code>, then use <code>cacheDir</code> as the directory to search in.
	 * <code>cacheDir</code> can be <code>null</code>. In such a case, use the platform dependent default value.
	 * 
	 * @param		cacheDir Absolute path of the directory to look for the shared class caches
	 * @param		flags Reserved for future use. Always pass {@link SharedClassUtilities#NO_FLAGS}
	 * @param		useCommandLineValues Use command line values instead of using parameter values
	 * 
	 * @return		List of {@link SharedClassCacheInfo} corresponding to shared class caches which are present
	 * in the specified directory, <code>null</code> on failure.
	 * 
	 * @throws		IllegalStateException
	 * 					If shared classes is disabled for this JVM (that is -Xshareclasses:none is present).
	 * @throws		IllegalArgumentException
	 * 					If <code>flags</code> is not a valid value.
	 * @throws		SecurityException
	 * 					If a security manager is enabled and the calling thread does not
	 * 					have SharedClassesNamedPermission("getSharedCacheInfo")
	 */
	static public List<SharedClassCacheInfo> getSharedCacheInfo(String cacheDir, int flags, boolean useCommandLineValues) {
		SecurityManager sm = System.getSecurityManager();
		if (sm != null) {
			sm.checkPermission(SharedPermissions.getSharedCacheInfo);
		}
		
		int retVal;
		/*[MSG "K0553", "parameter {0} has invalid value"]*/
		if (flags != NO_FLAGS) {
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0553", "\"flags\""));	//$NON-NLS-1$ //$NON-NLS-2$
		}
		ArrayList<SharedClassCacheInfo> arrayList = new ArrayList<>();
		retVal = getSharedCacheInfoImpl(cacheDir, flags, useCommandLineValues, arrayList);
		if (-1 == retVal) {
			return null;
		} else if(SHARED_CLASSES_UTILITIES_DISABLED == retVal) {
			/*[MSG "K0557", "Shared Classes support is disabled with -Xshareclasses:none"]*/
			throw new IllegalStateException(com.ibm.oti.util.Msg.getString("K0557")); //$NON-NLS-1$
		}
		return arrayList;
	}
	
	/**
	 * Destroys a named shared class cache of a given type in a given directory.
	 * <p>
	 * If <code>useCommandLineValues</code> is <code>true</code>, then use the command line value to get the shared class cache name,
	 * its type and its directory. If any of these is not available, then use the default value.
	 * If <code>useCommandLineValues</code> is <code>false</code>, then use <code>cacheDir</code>, <code>persistence</code>, and
	 * <code>cacheName</code> to identify the cache to be destroyed. To accept the default value for <code>cacheDir</code>
	 * or <code>cacheName</code>, specify the parameter with a <code>null</code> value.
	 * <p>
	 * The return value of this method depends on the status of existing current and older generation caches.
	 * <ul>
	 * <li>If it fails to destroy any existing cache with the given name, it returns
	 * {@link SharedClassUtilities#DESTROYED_NONE}.
	 * <li>If no cache exists or it is able to destroy all existing caches of all generations, it returns
	 * {@link SharedClassUtilities#DESTROYED_ALL_CACHE}.<br>
	 * <li>If it fails to destroy an existing current generation cache, irrespective of the state of older generation
	 * caches, it returns {@link SharedClassUtilities#DESTROY_FAILED_CURRENT_GEN_CACHE}.<br>
	 * <li>If it fails to destroy one or more older generation caches, and either a current generation cache does not
	 * exist or is successfully destroyed, it returns {@link SharedClassUtilities#DESTROY_FAILED_OLDER_GEN_CACHE}.<br>
	 * 	</ul>
	 * 
	 * @param		cacheDir Absolute path of the directory where the shared class cache is present
	 * @param		cacheType Type of the cache. The type has one of the following values:
	 * <ul>
	 * <li>{@link SharedClassUtilities#PERSISTENCE_DEFAULT}
	 * <li>{@link SharedClassUtilities#PERSISTENT}
	 * <li>{@link SharedClassUtilities#NONPERSISTENT}
	 * <li>{@link SharedClassUtilities#SNAPSHOT}
	 * </ul>
	 * @param		cacheName Name of the cache to be deleted
	 * @param		useCommandLineValues Use command line values instead of using parameter values
	 * 
	 * @return		Returns one of the following values:
	 * 				<ul>
	 * 				<li>{@link SharedClassUtilities#DESTROYED_ALL_CACHE}
	 * 				<li>{@link SharedClassUtilities#DESTROYED_NONE}
	 * 				<li>{@link SharedClassUtilities#DESTROY_FAILED_CURRENT_GEN_CACHE}
	 * 				<li>{@link SharedClassUtilities#DESTROY_FAILED_OLDER_GEN_CACHE}
	 * 				</ul>
	 * 
	 * @throws		IllegalStateException
	 * 					If shared classes is disabled for this JVM (that is -Xshareclasses:none is present).
	 * @throws		IllegalArgumentException
	 * 					If <code>cacheType<code> is not a valid value.
	 * @throws		SecurityException
	 * 					If a security manager is enabled and the calling thread does not
	 * 					have SharedClassesNamedPermission("destroySharedCache")
	 */
	static public int destroySharedCache(String cacheDir, int cacheType, String cacheName, boolean useCommandLineValues) {
		SecurityManager sm = System.getSecurityManager();
		if (sm != null) {
			sm.checkPermission(SharedPermissions.destroySharedCache);
		}

		int retVal = -1;
		
		/*
		 * check if cacheType has a valid value only if we are not using command line values.
		 */
		/*[MSG "K0553", "parameter {0} has invalid value"]*/
		if ((false == useCommandLineValues)
			&& (PERSISTENCE_DEFAULT != cacheType)
			&& (PERSISTENT != cacheType)
			&& (NONPERSISTENT != cacheType)
			&& (SNAPSHOT != cacheType)
		) {
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0553", "\"cacheType\""));	//$NON-NLS-1$ //$NON-NLS-2$
		}
		
		retVal = destroySharedCacheImpl(cacheDir, cacheType, cacheName, useCommandLineValues);
		if (SHARED_CLASSES_UTILITIES_DISABLED == retVal) {
			/*[MSG "K0557", "Shared Classes support is disabled with -Xshareclasses:none"]*/
			throw new IllegalStateException(com.ibm.oti.util.Msg.getString("K0557")); //$NON-NLS-1$
		}
		
		return retVal;
	}
}

