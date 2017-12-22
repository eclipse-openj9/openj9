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
 * <p>SharedClassHelper API that stores and finds classes by using URL classpaths.</p>
 * <h3>Description</h3>
 * <p>A SharedClassURLClasspathHelper is obtained by calling getURLClasspathHelper(ClassLoader, URL[]) on a SharedClassHelperFactory.</p>
 * <p>The SharedClassURLClasspathHelper is designed for ClassLoaders that load classes using a classpath comprised of a sequence of URLs.
 * It is assumed that classpaths are searched left-most to right-most and can only be modified in predictable ways as described in <q>Usage</q>.</p>
 * <h3>Usage</h3>
 * <p>The ClassLoader should call findSharedClass after asking its parent (if one exists).
 * If findSharedClass does not return null, the ClassLoader calls defineClass on the byte[] that is returned.</p>
 * <p>The ClassLoader calls storeSharedClass immediately after a class is defined, unless the class that is being defined was loaded from the shared cache.</p>
 * <p>The ClassLoader must keep the helper up to date with regard to classpath changes. If the ClassLoader classpath is appended to, the ClassLoader must call addClasspathEntry(URL) on the helper.
 * If the ClassLoader classpath is modified in any other way, the ClassLoader must call setClasspath(URL[]).</p>
 * <p>If partitions are required, the ClassLoader is responsible for coordinating the creation and use of partition Strings (see <q>Using classpaths</q>).</p>
 * <p>Classes can be stored only by using URLs that have file or jar protocols, and that refer to existing resources.
 * The presence of any other protocol in a URL prevents SharedClassURLClasspathHelper from locating and storing classes in the shared cache.</p>
 * <h3>Using classpaths</h3>
 * <p>When a findSharedClass request is made from a SharedClassURLClasspathHelper, the shared cache determines whether to return a class 
 * by comparing the caller's classpath to the classpath or URL that the class was stored against. Classpaths do not have to explicitly match.</p>
 * <p>
 * For example, a class c1 loaded from c.jar and stored using classpath a.jar;b.jar;c.jar;d.jar may be found by using the following classpaths:</p>
 * <ul>
 * <li>a.jar;c.jar ... (It is known that a.jar does not contain a c1)</li>
 * <li>b.jar;c.jar ... (It is known that b.jar does not contain a c1)</li>
 * <li>b.jar;a.jar;c.jar ... (likewise in whatever order)</li>
 * <li>c.jar ...</li>
 * </ul>
 * <p>However, it will not be found by using the following classpath:</p>
 * <ul>
 * <li>d.jar;a.jar;b.jar;c.jar ... (It is not known whether a different c1 exists in d.jar or not)</li>
 * </ul>
 * <p>To ensure that findSharedClass returns correct results, it returns only classes that are found for entries up to and including the right-most
 * <q>confirmed</q> entry. The definition of a confirmed entry is that it has been opened and read by the ClassLoader. Entries are confirmed by calling
 * storeSharedClass (every entry up to and including the index used is confirmed) or by calling confirmAllEntries(). The latter should only be used if
 * the classpath is guaranteed not to change (but can still be appended to).</p>
 * <p>Note that if findSharedClass returns null for whatever reason (even though the cache has the required class), calling storeSharedClass(...) 
 * on the class loaded from disk is the correct thing to do. This confirms the classpath entry, and the cache does not store a duplicate class.</p>
 * <p>In this way, classes can be shared as effectively as possible between class loaders using different classpaths or URL helper types.</p>
 * <p>Note that two identical classes are never stored twice in the class cache, but many entries may exist for the same class. 
 * For example, if class X is stored from a.jar and also from b.jar, the class will exist once in the cache, but will have two entries.</p>
 * <h3>Modifying classpaths</h3>
 * <p>It is possible that the classpath of a ClassLoader may change after it is initially set. There is a jar extension mechanism, for example,
 * that allows a jar to define a lookup order of other jars within its manifest. If this causes the classpath to change, then the helper
 * <b>must</b> be informed of the update.</p>
 * <p>After an initial classpath has been set in SharedClassURLClasspathHelper, classpath entries in it are <q>confirmed</q> as classes are stored in the cache
 * from these entries. For example given classpath a.jar;b.jar;c.jar;d.jar, if a class is stored from b.jar then both a.jar and b.jar are confirmed
 * because the ClassLoader must have opened both of them.</p>
 * <p>After classpath entries are confirmed, they cannot be changed, so setClasspath may only make changes to parts of the classpath that are not yet confirmed.
 * For example in the given example, a.jar;b.jar;x.jar;c.jar is acceptable, whereas a.jar;x.jar;b.jar;c.jar;d.jar is not. If an attempt is made to make
 * changes to confirmed classpath entries, a CannotSetClasspathException is thrown.</p>
 * <h3>Dynamic cache updates</h3> 
 * <p>Because the shared cache persists beyond the lifetime of a JVM, classes in the shared cache can become out of date (stale).
 * Classes in the cache are automatically kept up to date by default.</p>
 * <p>If findSharedClass is called for a class that exists in the cache but which has been updated on the filesystem since it was stored,
 * then the old version in the cache is automatically marked stale and findSharedClass returns null.
 * The ClassLoader should then store the updated version of the class.</p>
 * <p>Note that jar/zip updates cause all cache entries that are loaded from that jar/zip to be marked stale.
 * Since a classpath has a specific search order, an update to a jar/zip will also cause all classes that are
 * loaded from URLs to the right-side of that entry to also become stale.
 * This is because the updated zip/jar may now contain classes that <q>hide</q> other classes to the right-side of it in the classpath.</p>
 * <p>For example, class c1 is loaded from c.jar and is stored in the cache using classpath a.jar;b.jar;c.jar.</p>
 * <p>a.jar is then updated to include a version of c1 and findSharedClass is called for c1.</p>
 * <p>It is essential that findSharedClass does not then return the version of c1 in c.jar.
 * It will detect the change, return null and c1 from a.jar should then be stored.</p>
 * <p>(This behaviour can be disabled by using the correct command-line option, but this is not recommended. See -Xshareclasses:help.)</p>
 * <p>It is also assumed that after a jar/zip has been opened, the ClassLoader maintains a read lock on that file during its lifetime, 
 * preventing its modification. This prevents the cache from having to constantly check for updates.
 * However, it is understood that non-existent jars/zips on a classpath can only be locked if/when they exist. </p>
 * <h3>Partitions</h3>
 * <p>A partition can be used when finding or storing a class, which allows modified versions of the same class 
 * to be stored in the cache, effectively creating <q>partitions</q> in the cache.</p>
 * <p>Partitions are designed for bytecode modification such as the use of Aspects. It is the responsibility of the ClassLoader 
 * to create partitions that describe the type of modification performed on the class bytes.</p>
 * <p>If a class is updated on the filesystem and automatic dynamic updates are enabled, then all versions of the class across 
 * all partitions will be marked stale.</p>
 * <h3>Class metadata</h3>
 * <p>A ClassLoader can create metadata when loading and defining classes, such as a jar manifest or security data.
 * None of this metadata can be stored in the cache, so if a ClassLoader is finding classes in the shared cache, it must load 
 * any metadata that it needs from disk before defining the classes.</p>
 * <p>
 * Example:<br>
 * For static metadata specific to URL classpath entries (such as Manifest/CodeSource information), a suggested solution is to create
 * a local array to cache the metadata when it is loaded from disk. When findSharedClass is called and an index is returned, look to see
 * if metadata is already cached in the local array for that index. If it is, define the class. If not, load the class from disk to obtain
 * the metadata, cache it in the local array, define the class, and then call storeSharedClass on it (it doesn't matter if it is already in the cache). 
 * All future results from findSharedClass for that classpath entry can then use the cached metadata.</p>
 * <p>If findSharedClass returns null, then load the class from disk, cache the metadata from the entry anyway, define the class, and store it.</p>
 * <h3>Security</h3>
 * <p>A SharedClassHelper will only allow classes that were defined by the ClassLoader that owns the SharedClassHelper to be stored in the cache.</p>
 * <p>If a SecurityManager is installed, SharedClassPermissions must be used to permit read/write access to the shared class cache.
 * Permissions are granted by ClassLoader classname in the java.policy file and are fixed when the SharedClassHelper is created.</p>
 * <p>Note also that if the createClassLoader RuntimePermission is not granted, ClassLoaders cannot be created, 
 * which in turn means that SharedClassHelpers cannot be created.</p>
 * <h3>Efficient use of the SharedClassURLClasspathHelper</h3>
 * Here are some recommendations on using the SharedClassURLClasspathHelper:<br>
 * <ol>
 * <li>It is best not to start with a zero length classpath and gradually grow it. Each classpath change causes a new classpath to be added to the
 * cache and reduces the number of optimizations that can be made when matching classpaths.</li>
 * <li>If the ClassLoader will never call setClasspath, then use confirmAllEntries immediately after obtaining the helper. This ensures that
 * findSharedClass will never return null due to unconfirmed entries.</li>
 * </ol>
 * <h3>Compatibility with other SharedClassHelpers</h3>
 * <p>Classes stored by using the SharedClassURLClasspathHelper can be retrieved by using the SharedClassURLHelper and vice versa.
 * This is also true for partitions that can be used across these two helpers.</p>
 *
 * @see SharedClassURLClasspathHelper
 * @see SharedClassHelperFactory
 * @see SharedClassPermission
 */
public interface SharedClassURLClasspathHelper extends SharedClassHelper {

	/**
	 * <p>Interface that allows an index to be returned from findSharedClass calls.</p>
	 */
	public interface IndexHolder {
		/**
		 * Sets the index in the caller ClassLoader's classpath at which the class was found.
		 * 
		 * @param index The index.
		 */
		public void setIndex(int index);
	}

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
	 * @param 		indexFoundAt IndexHolder.
	 * 					The index in the caller ClassLoader's classpath at which the class was found.
	 * 					This parameter can be null if this data is not needed.
	 * 
	 * @return		byte[].
	 * 					A byte array describing the class found, or null.
	 */
	public byte[] findSharedClass(String className, IndexHolder indexFoundAt);

	/**
	 * <p>Finds a class in the shared cache by using the class name and partition given (implicitly using the caller's classpath).</p>
	 * <p>See <q>Using classpaths</q> for rules on when a class will be found.<br>
	 * Null is returned if the class cannot be found, if it is stale (see <q>Dynamic cache updates</q>) 
	 * or if it is found for an unconfirmed entry (see <q>Using classpaths</q>).</p>
	 * 
	 * @param 		partition String.
	 * 					User-defined partition if finding modified bytecode (see <q>Partitions</q>). 
	 * 					Passing null is equivalent of calling non-partition findSharedClass call.
	 * 
	 * @param 		className String.
	 * 					The name of the class to be found
	 * 
	 * @param 		indexFoundAt IndexHolder.
	 * 					The index in the caller ClassLoader's classpath at which the class was found.
	 * 					This parameter can be null if this data is not needed.
	 * 
	 * @return		byte[].
	 * 					A byte array describing the class found, or null.
	 */
	public byte[] findSharedClass(String partition, String className, IndexHolder indexFoundAt);
	
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
	public boolean storeSharedClass(Class<?> clazz, int foundAtIndex);

	/**
	 * <p>Stores a class in the shared cache by using the caller's URL classpath and with a user-defined partition.</p>
	 * 
	 * <p>The class that is being stored must have been defined by the caller ClassLoader and must exist in the URL location specified.</p>
	 * <p>Returns <code>true</code> if the class is stored successfully or <code>false</code> otherwise.</p>
	 * <p>Will return <code>false</code> if the class that is being stored was not defined by the caller ClassLoader.</p>
	 * <p>Also returns <code>false</code> if the URL at foundAtIndex is not a file URL or if the resource it refers to does not exist. </p>
	 * 
	 * @param 		partition String.
	 * 					User-defined partition if storing modified bytecode (see <q>Partitions</q>).
	 * 					Passing null is equivalent of calling non-partition storeSharedClass call.
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
	public boolean storeSharedClass(String partition, Class<?> clazz, int foundAtIndex);

	/**
	 * <p>Updates the helper's classpath by appending a URL (see <q>Usage</q>).</p>
	 * 
	 * <p><b>Note:</b> It is <b>essential</b> that the helper's classpath is kept up-to-date with the classloader.</p>
	 * 
	 * @param 		cpe URL.
	 * 					The classpath entry to append to the classpath
	 */
	public void addClasspathEntry(URL cpe);
	
	/**
	 * <p>Updates the helper's classpath with a new classpath.</p>
	 * 
	 * <p>This function is useful for ClassLoaders that compute their classpath lazily. The initial classpath
	 * is passed to the constructor optimistically, but if the classloader discovers a change while reading
	 * an entry, it can update the classpath by using this function.</p>
	 * <p><b>Note:</b> It is essential that the helper's classpath is kept up-to-date with the classloader.</p>
	 * 
	 * <p>The classpath that is passed to this function must be exactly the same as the original
	 * classpath up to and including the right-most entry that classes have been loaded from (the right-most <q>confirmed</q> entry).</p>
	 * <p>Throws a CannotSetClasspathException if this is not the case (see <q>Modifying classpaths</q>).</p>
	 * 
	 * <p>After the classpath has been updated, any indexes passed to storeSharedClass and returned from
	 * findSharedClass correspond to the new classpath.</p>
	 * 
	 * @param 		newClasspath
	 * 					The new URL classpath array
	 * 
	 * @throws 		CannotSetClasspathException when the class path cannot be set.
	 */
	public void setClasspath(URL[] newClasspath) throws CannotSetClasspathException;
	
	/**
	 * <p>Confirms all entries in the current classpath. 
	 * Any new entries added will not automatically be confirmed.</p>
	 * 
	 * <p>Note that if all entries are confirmed, setClasspath cannot be used to modify the classpath, only to append new entries.
	 * (see <q>Efficient use of the SharedClassURLClasspathHelper</q>).</p>
	 * 
	 * <p>
	 */
	public void confirmAllEntries();
}
