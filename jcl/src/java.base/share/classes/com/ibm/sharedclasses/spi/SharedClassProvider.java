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
package com.ibm.sharedclasses.spi;

import java.net.URL;
import java.security.BasicPermission;
import java.util.function.IntConsumer;

/**
 * <p>SharedClassProvider defines functions used by a class Loader to store/load data into/from the shared cache.</p>
 */
public interface SharedClassProvider {

	/**
	 * <p>Initializes a SharedClassProvider.</p>
	 *
	 * @param loader ClassLoader
	 *				The ClassLoader that uses SharedClassProvider.
	 * @param classpath URL[]
	 *				The current URL classpath array of the ClassLoader.
	 *
	 * @return SharedClassProvider
	 * 				A SharedClassProvider if it is successfully initialized, or null otherwise.
	 */
	public SharedClassProvider initializeProvider(ClassLoader loader, URL[] classpath);
	/**
	 * <p>Initializes a SharedClassProvider.</p>
	 *
	 * @param loader ClassLoader
	 *				The ClassLoader that uses SharedClassProvider.
	 * @param classpath URL[]
	 *				The current URL classpath array of the ClassLoader.
	 * @param urlHelper
	 * 				True if a SharedClassURLHelper needs to be created for this class loader, false otherwise.
	 * @param tokenHelper
	 *				True if a SharedClassTokenHelper needs to be created for this class loader, false otherwise.
	 *
	 * @return SharedClassProvider
	 * 				A SharedClassProvider if it is successfully initialized, or null otherwise.
	 */
	public SharedClassProvider initializeProvider(ClassLoader loader, URL[] classpath, boolean urlHelper, boolean tokenHelper);

	/**
	 * <p>Checks whether shared classes are enabled for this JVM.</p>
	 * 
	 * @return boolean
	 * 				True if shared classes are enabled (using -Xshareclasses on the command-line), false otherwise.
	 */
	public boolean isSharedClassEnabled();

	/**
	 * <p>Finds a class in the shared cache by using a specific URL and class name.
	 * A class will be returned if it was stored against the same URL.</p>
	 * <p>Null is returned if the class cannot be found or if it is stale (see <q>Dynamic cache updates</q>).</p>
	 * <p>To obtain an instance of the class, the byte[] returned must be passed to defineClass by the caller ClassLoader.</p>
	 *
	 * @param 		path URL.
	 * 					A URL path. Cannot be null.
	 *
	 * @param 		className String.
	 * 					The name of the class to be found
	 *
	 * @return		byte[].
	 * 					A byte array describing the class found, or null.
	 */
	public byte[] findSharedClassURL(URL path, String className);
	
	/**
	 * <p>Stores a class in the shared cache by using the URL location it was loaded from.
	 * The class that is being stored must have been defined by the caller ClassLoader and must exist in the URL location specified.
	 * Returns <code>true</code> if the class is stored successfully or <code>false</code> otherwise.</p>
	 * <p>Will return <code>false</code> if the class that is being stored was not defined by the caller ClassLoader.</p>
	 * <p>Also returns <code>false</code> if the URL is not a file URL or if the resource it refers to does not exist.</p>
	 *
	 * @param 		path URL.
	 * 					The URL path that the class was loaded from. Cannot be null.
	 *
	 * @param 		clazz Class.
	 * 					The class to store in the shared cache
	 *
	 * @return		boolean.
	 * 					True if the class was stored successfully, false otherwise.
	 */
	public boolean storeSharedClassURL(URL path, Class<?> clazz);

	/**
	 * <p>Finds a class in the shared cache by using the class name given (implicitly using the caller's classpath).</p>
	 * 
	 * <p>See <q>Using classpaths</q> for rules on when a class will be found.<br>
	 * Null is returned if the class cannot be found, if it is stale (see <q>Dynamic cache updates</q>) 
	 * or if it is found for an unconfirmed entry (see <q>Using classpaths</q>).</p>
	 * 
	 * @param 		className String.
	 * 					The name of the class to be found
	 * 
	 * @param 		indexConsumer IntConsumer.
	 *					The consumer used to receive the classpath index if desired.
	 *
	 * @return		byte[].
	 * 					A byte array describing the class found, or null.
	 */
	public byte[] findSharedClassURLClasspath(String className, IntConsumer indexConsumer);

	/**
	 * <p>Stores a class in the shared cache by using the caller's URL classpath.</p>
	 * <p>The class being stored must have been defined by the caller ClassLoader and must exist in the URL location specified.</p>
	 * <p>Returns <code>true</code> if the class is stored successfully or <code>false</code> otherwise.</p>
	 * <p>Will return <code>false</code> if the class being stored was not defined by the caller ClassLoader.</p>
	 * <p>Also returns <code>false</code> if the URL at foundAtIndex is not a file URL or if the resource it refers to does not exist. </p>
	 *
	 * @param 		clazz Class.
	 * 					The class to store in the shared cache
	 *
	 * @param 		foundAtIndex int.
	 * 					The index in the caller's classpath where the class was loaded from (first entry is 0).
	 *
	 * @return		boolean.
	 * 					True if the class was stored successfully, false otherwise.
	 */
	public boolean storeSharedClassURLClasspath(Class<?> clazz, int foundAtIndex);

	/**
	 * <p>Updates the URLClasspath helper's classpath with a new classpath.</p>
	 * 
	 * <p>This function is useful for ClassLoaders that compute their classpath lazily. The initial classpath
	 * is passed to the constructor optimistically, but if the classloader discovers a change while reading
	 * an entry, it can update the classpath by using this function.</p>
	 * <p><b>Note:</b> It is essential that the helper's classpath is kept up-to-date with the classloader.</p>
	 * 
	 * <p>The classpath that is passed to this function must be exactly the same as the original
	 * classpath up to and including the right-most entry that classes have been loaded from (the right-most <q>confirmed</q> entry).</p>
	 * 
	 * <p>After the classpath has been updated, any indexes passed to storeSharedClassURLClasspath and returned from
	 * findSharedClassURLClasspath correspond to the new classpath.</p>
	 * 
	 * @param 		newClasspath
	 * 					The new URL classpath array
	 * @return		boolean.
	 * 					True if the classpath has been set, false otherwise.
	 */
	public boolean setURLClasspath(URL[] newClasspath);
	
	/**
	 * <p>Returns the size of the cache that the JVM is currently connected to.</p>
	 *
	 * @return		long.
	 * 					The total size in bytes of the shared cache.
	 */
	public long getCacheSize();
	
	/**
	 * <p>Returns the soft limit in bytes for the available space of the cache that the JVM is currently connected to.</p>
	 * 
	 * @return		long.
	 * 					The soft max size or cache size in bytes if it is not set.
	 */
	public long getSoftmxBytes();
	
	/**
	 * <p>Returns the minimum space reserved for AOT data of the cache that the JVM is currently connected to.</p>
	 * 
	 * @return 		long.
	 *					The minimum shared classes cache space reserved for AOT data in bytes or -1 if it is not set.
	 */
	public long getMinAotBytes();
	
	/**
	 * <p>Returns the maximum space allowed for AOT data of the cache that the JVM is currently connected to.</p>
	 * 
	 * @return 		long.
	 * 					The maximum shared classes cache space allowed for AOT data in bytes or -1 if it is not set.
	 */
	public long getMaxAotBytes();
	
	/**
	 * <p>Returns the minimum space reserved for JIT data of the cache that the JVM is currently connected to.</p>
	 * 
	 * @return 		long.
	 * 					The minimum shared classes cache space reserved for JIT data in bytes or -1 if it is not set.
	 */
	public long getMinJitDataBytes();
	
	/**
	 * <p>Returns the maximum space allowed for JIT data of the cache that the JVM is currently connected to.</p>
	 * 
	 * @return 		long.
	 * 					The maximum shared classes cache space allowed for JIT data in bytes  or -1 if it is not set.
	 */
	public long getMaxJitDataBytes();
	
	/**
	 * <p>Returns the free space in bytes of the cache that the JVM is currently connected to.</p>
	 *
	 * @return 		long.
	 * 					The free space of the shared classes cache.
	 */
	public long getFreeSpace();
	
	/**
	 * <p>Constructs a new instance of SharedClassPermission which is a sub-class of BasicPermission.</p>
	 * 
	 * @param		classLoaderClassName java.lang.String.
	 *					The class name of the class loader requiring the permission.
	 * @param		actions java.lang.String.
	 *					The actions which are applicable to it.
	 * @return		BasicPermission.
	 * 					A new instance of SharedClassPermission which is a sub-class of BasicPermission, or null if shared classes are not enabled.
	 */
	public BasicPermission createPermission(String classLoaderClassName, String actions);
}
