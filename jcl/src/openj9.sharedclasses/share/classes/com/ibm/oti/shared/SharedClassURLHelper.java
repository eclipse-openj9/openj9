/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

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

import java.net.URL;

/**
 * <p>SharedClassHelper API that stores and finds classes by using URL paths.</p>
 * <h3>Description</h3>
 * <p>A SharedClassURLHelper is obtained by calling getURLHelper(ClassLoader) on a SharedClassHelperFactory.</p>
 * <p>The SharedClassURLHelper is designed for ClassLoaders that load classes from many different locations, without the concept of a classpath.
 * A ClassLoader may find and store classes in the cache using any URL.</p>
 * <h3>Usage</h3>
 * <p>The ClassLoader should call findSharedClass after looking in its local cache and asking its parent (if one exists).
 * If findSharedClass does not return null, the ClassLoader calls defineClass on the byte[] that is returned.</p>
 * <p>The ClassLoader calls storeSharedClass immediately after a class is defined, unless the class that is being defined was loaded from the shared cache.</p>
 * <p>If partitions are required, the ClassLoader is responsible for coordinating the creation and use of partition Strings.</p>
 * <p>Classes can be stored only by using URLs that have file or jar protocols, and that refer to existing resources.
 * The presence of any other protocol in a URL prevents SharedClassURLHelper from locating and storing classes in the shared cache.</p>
 * <h3>Dynamic Cache Updates</h3> 
 * <p>Because the shared cache persists beyond the lifetime of a JVM, classes in the shared cache can become out of date (stale).</p>
 * Classes in the cache are automatically kept up to date by default:<br>
 * <p>If findSharedClass is called for a class that exists in the cache but which has been updated on the filesystem since it was stored,
 * then the old version in the cache is automatically marked stale and findSharedClass returns null.
 * The ClassLoader should then store the updated version of the class.</p>
 * <p>Note that jar/zip updates will cause all cache entries that are loaded from that jar/zip to be marked stale.</p>
 * <p>(This behaviour can be disabled by using the correct command-line option. See -Xshareclasses:help)</p>
 * <p>It is also assumed that the ClassLoader maintains a read lock on jar/zip files opened during its lifetime, preventing their modification.
 * This prevents the cache from having to constantly check for updates.</p>
 * <h3>Partitions</h3>
 * <p>A partition can be used when finding or storing a class, which allows modified versions of the same class
 *   to be stored in the cache, effectively creating <q>partitions</q> in the cache.</p>
 * <p>Partitions are designed for bytecode modification such as the use of Aspects. It is the responsibility of the ClassLoader
 *   to create partitions that describe the type of modification performed on the class bytes.</p>
 * <p>If a class is updated on the filesystem and automatic dynamic updates are enabled, then all versions of the class across
 *   all partitions will be marked stale.</p>
 * <h3>Class metadata</h3>
 * <p>A ClassLoader might create metadata when loading and defining classes, such as a jar manifest or security data.
 * None of this metadata can be stored in the cache, so if a ClassLoader is finding classes in the shared cache, it must load
 *   any metadata that it needs from disk before defining the classes.</p>
 * <h3>Security</h3>
 * <p>A SharedClassHelper will only allow classes that were defined by the ClassLoader that owns the SharedClassHelper to be stored in the cache.</p>
 * <p>If a SecurityManager is installed, SharedClassPermissions must be used to permit read/write access to the shared class cache.
 * Permissions are granted by ClassLoader classname in the java.policy file and are fixed when the SharedClassHelper is created.</p>
 * <p>Note also that if the createClassLoader RuntimePermission is not granted, ClassLoaders cannot be created, 
 * which in turn means that SharedClassHelpers cannot be created.</p>
 * <h3>Compatibility with other SharedClassHelpers</h3>
 * <p>Classes stored by using the SharedClassURLHelper can be retrieved by using the SharedClassURLClasspathHelper and vice versa.
 * This is also true for partitions that can be used across these two helpers.</p>
 * 
 * @see SharedClassURLHelper
 * @see SharedClassHelperFactory
 * @see SharedClassPermission
 */
public interface SharedClassURLHelper extends SharedClassHelper {

	/**
	 * <p>Finds a class in the shared cache by using a specific URL and class name.
	 * A class will be returned if it was stored against the same URL.</p>
	 * <p>Null is returned if the class cannot be found or if it is stale (see <q>Dynamic cache updates</q>).</p>
	 * <p>To obtain an instance of the class, the byte[] returned must be passed to defineClass by the caller ClassLoader.</p>
	 * <p>Also returns false if the URL at foundAtIndex is not a file URL or if the resource it refers to does not exist. </p>
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
	public byte[] findSharedClass(URL path, String className);

	/**
	 * <p>Finds a class in the shared cache by using a specific URL, class name, and user-defined partition (see <q>Partitions</q>).</p>
	 * <p>A class returned only if it was stored against the same URL, and was using the same partition.
	 * Null is returned if the class cannot be found or if it is stale (see <q>Dynamic cache updates</q>).</p>
	 * <p>To obtain an instance of the class, the byte[] returned must be passed to defineClass by the caller ClassLoader.</p>
	 * <p>Also returns false if the URL at foundAtIndex is not a file URL or if the resource it refers to does not exist. </p>
	 *
	 * @param 		partition String.
	 * 					User-defined partition if finding modified bytecode (see <q>Partitions</q>).
	 * 					Passing null is equivalent of calling non-partition findSharedClass call.
	 *
	 * @param 		path URL.
	 * 					A URL path. Cannot be null.
	 *
	 * @param 		className String.
	 * 					The name of the class to be found
	 *
	 * @return		byte[].
	 * 					A byte array describing the found class, or null.
	 */
	public byte[] findSharedClass(String partition, URL path, String className);

	/**
	 * <p>Stores a class in the shared cache by using the URL location it was loaded from.
	 * The class that is being stored must have been defined by the caller ClassLoader and must exist in the URL location specified.
	 * Returns <code>true</code> if the class is stored successfully or <code>false</code> otherwise.</p>
	 * <p>Will return <code>false</code> if the class that is being stored was not defined by the caller ClassLoader.</p>
	 * <p>Also returns <code>false</code> if the URL at foundAtIndex is not a file URL or if the resource it refers to does not exist.</p>
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
	public boolean storeSharedClass(URL path, Class<?> clazz);

	/**
	 * <p>Stores a class in the shared cache by using the URL location it was loaded from and a user-defined partition (see <q>Partitions</q>).</p>
	 * <p>The class being stored must have been defined by the caller ClassLoader and must exist in the URL location specified.
	 * Returns <code>true</code> if the class is stored successfully or <code>false</code> otherwise.</p>
	 * <p>Will return <code>false</code> if the class being stored was not defined by the caller ClassLoader.</p>
	 * <p>Also returns <code>false</code> if the URL at foundAtIndex is not a file URL or if the resource it refers to does not exist.</p>
	 * 
	 * @param 		partition String.
	 * 					User-defined partition if storing modified bytecode (see <q>Partitions</q>).
	 * 					Passing null is equivalent of calling non-partition storeSharedClass call.
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
	public boolean storeSharedClass(String partition, URL path, Class<?> clazz);
	
	/**
	 * <p>Minimizes update checking on jar files for optimal performance.</p>
	 * <p>By default, when a class is loaded from the shared class cache, the timestamp of the container it was originally loaded from is
	 * compared with the timestamp of the actual container on the filesystem. If the two do not match, the original class is marked stale
	 * and is not returned by findSharedClass(). These checks are not performed if the container is held open by the ClassLoader.</p>
	 * <p>If the ClassLoader does not want to open the container, but doesn't want the timestamp to be constantly checked when 
	 * classes are loaded, it should call this function immediately after the SharedClassURLHelper object has been created.
	 * After this function has been called, each container timestamp is checked once and then is only checked again if the 
	 * container jar file is opened.<br>
	 * <p>This feature cannot be unset.</p>
	 *
	 * @return		boolean.
	 * 					True if the feature has been successfully set, false otherwise.
	 */
	public boolean setMinimizeUpdateChecks();
}
