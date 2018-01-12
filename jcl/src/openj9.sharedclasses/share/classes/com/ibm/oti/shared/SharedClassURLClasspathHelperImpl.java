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

import com.ibm.oti.util.Msg;

/**
 * <p>Implementation of SharedClassURLClasspathHelper.</p>
 * @see SharedClassURLClasspathHelper
 * @see SharedClassHelperFactory
 * @see SharedClassAbstractHelper
 */
final class SharedClassURLClasspathHelperImpl extends SharedClassAbstractHelper implements
		SharedClassURLClasspathHelper {
	private URL[] urls, origurls;
	private boolean[] validated;
	private int urlCount, confirmedCount;
	private boolean invalidURLExists;
	
	private static native void init();
	
	static {
		init();
	}

	/* Not public - should only be created by factory */
	SharedClassURLClasspathHelperImpl(ClassLoader loader, URL[] classpath, int id, boolean canFind, boolean canStore) {
		this.origurls = classpath;
		this.urls = new URL[classpath.length];
		this.urlCount = classpath.length;
		this.validated = new boolean[classpath.length];
		this.confirmedCount = 0;
		this.invalidURLExists = false;
		initialize(loader, id, canFind, canStore);
		initializeShareableClassloader(loader);
		initializeURLs();
	}

	private void initializeURLs() {
		for (int i=0; i<urlCount; i++) {
			urls[i] = convertJarURL(origurls[i]);
			if (!validateURL(urls[i], false)) {
				invalidURLExists = true;
			}
		}
	}

	private native int findSharedClassImpl2(int loaderId, String partition, String className, ClassLoader loader, URL[] loaderURLs, 
			boolean doFind, boolean doStore, int loaderURLCount, int confirmedURLCount, byte[] romClassCookie);

	private native boolean storeSharedClassImpl2(int loaderid, String partition, ClassLoader loader, URL[] loaderURLs, int loaderURLCount, int cpLoadIndex, Class<?> clazz, byte[] flags);

	/* Before setClasspath(), classpath changes were detected by change in urlCount. However, this is now
	 * broken by the addition of setClasspath(). All the classpath cacheing optimizations in the cache
	 * also depend on the urlCount change, which is also broken by setClasspath(). Hence, this native
	 * essentially flushes the classpath caches and forces them to rebuild.
	 */
	private native void notifyClasspathChange2(ClassLoader classloader);

	@Override
	public byte[] findSharedClass(String className, IndexHolder indexFoundAtHolder) {
		return findSharedClass(null, className, indexFoundAtHolder);
	}
	
	@Override
	public synchronized byte[] findSharedClass(String partition, String className, IndexHolder indexFoundAtHolder) {
		ClassLoader loader = getClassLoader();
		if (loader == null) {
			/*[MSG "K059f", "ClassLoader has been garbage collected. Returning null."]*/
			printVerboseInfo(Msg.getString("K059f")); //$NON-NLS-1$
			return null;
		}
		if (!canFind) {
			return null;
		}
		if (className==null) {
			/*[MSG "K05a1", "Cannot call findSharedClass with null class name. Returning null."]*/
			printVerboseError(Msg.getString("K05a1")); //$NON-NLS-1$
			return null;
		}
		if (invalidURLExists) {
			/* Any URL which has its protocol other than 'jar:' or 'file:' is not supported by
			 * shared class cache and is considered invalid.
			 * invalidURLExists = true indicates classpath contains an invalid URL,
			 * As such there is no point in calling native method findSharedClassImpl2() 
			 * since it is bound to fail when creating classpath entries.
			 */
			/*[MSG "K05a4", "Classpath contains an invalid URL. Returning null."]*/
			printVerboseInfo(Msg.getString("K05a4")); //$NON-NLS-1$
			return null;
		}
		/* Important not to call findSharedClassImpl if confirmedCount==0 as 0 means "confirmedCount not set" */
		if (confirmedCount==0) {
			/*[MSG "K05a5", "There are no confirmed elements in the classpath. Returning null."]*/
			printVerboseInfo(Msg.getString("K05a5")); //$NON-NLS-1$
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
		int indexFoundAt = findSharedClassImpl2(this.id, partition, className, loader, this.urls, doFind, doStore, this.urlCount, this.confirmedCount, romClassCookie);
		/* indexFoundAt will be -1 if class is not found */
		if (indexFoundAt < 0) {
			return null;
		}
		if (indexFoundAtHolder!=null) {
			indexFoundAtHolder.setIndex(indexFoundAt);
		}
		return romClassCookie;
	}
	
	@Override
	public boolean storeSharedClass(Class<?> clazz, int foundAtIndex) {
		return storeSharedClass(null, clazz, foundAtIndex);
	}

	@Override
	public synchronized boolean storeSharedClass(String partition, Class<?> clazz, int foundAtIndex) {
		if (!canStore) {
			return false;
		}
		if (clazz==null) {
			/*[MSG "K05a3", "Cannot call storeSharedClass with null Class. Returning false."]*/
			printVerboseError(Msg.getString("K05a3")); //$NON-NLS-1$
			return false;
		}
		if (urlCount==0) {
			/*[MSG "K05a6", "Classpath has zero elements. Cannot call storeSharedClass without classpath. Returning false."]*/
			printVerboseError(Msg.getString("K05a6")); //$NON-NLS-1$
			return false;
		}
		if (foundAtIndex<0) {
			/*[MSG "K05a7", "foundAtIndex cannot be <0 for storeSharedClass. Returning false."]*/
			printVerboseError(Msg.getString("K05a7")); //$NON-NLS-1$
			return false;
		}
		if (foundAtIndex>=urlCount) {
			/*[MSG "K05a8", "foundAtIndex cannot be >= urls in classpath for storeSharedClass. Returning false."]*/
			printVerboseError(Msg.getString("K05a8")); //$NON-NLS-1$
			return false;
		}
		if (invalidURLExists) {
			/* Any URL which has its protocol other than 'jar:' or 'file:' is not supported by
			 * shared class cache and is considered invalid.
			 * invalidURLExists = true indicates classpath contains an invalid URL,
			 * As such there is no point in calling native method storeSharedClassImpl2() 
			 * since it is bound to fail when creating classpath entries.
			 */			
			/*[MSG "K05a9", "Classpath contains an invalid URL. Returning false."]*/
			printVerboseInfo(Msg.getString("K05a9")); //$NON-NLS-1$
			return false;
		}
		if (!validated[foundAtIndex]) {
			/* Because we only check each element once, we can afford to also check whether the URL exists */
			if (!validateURL(urls[foundAtIndex], true)) {
				return false;
			}
			validated[foundAtIndex]=true;
		}
		ClassLoader actualLoader = getClassLoader();
		if (!validateClassLoader(actualLoader, clazz)) {
			/* Attempt to call storeSharedClass with class defined by a different classloader */
			return false;
		}
		if (confirmedCount <= foundAtIndex) {
			confirmedCount = foundAtIndex+1;
			/*[MSG "K05aa", "Number of confirmed entries is now {0}"]*/
			printVerboseInfo(Msg.getString("K05aa", confirmedCount)); //$NON-NLS-1$
		}
		return storeSharedClassImpl2(this.id, partition, actualLoader, this.urls, this.urlCount, foundAtIndex, clazz, nativeFlags);
	}

	private synchronized void growURLs(int toMinSize) {
		/*[MSG "K05ab", "Growing URL array to {0}"]*/
		printVerboseInfo(Msg.getString("K05ab", toMinSize)); //$NON-NLS-1$
		int newSize = ((toMinSize+1)*2);		/* toMinSize could be zero! */
		URL[] newOrigUrls = new URL[newSize];
		URL[] newUrls = new URL[newSize];
		boolean[] newValidated = new boolean[newSize];
		System.arraycopy(origurls, 0, newOrigUrls, 0, urlCount);
		System.arraycopy(urls, 0, newUrls, 0, urlCount);
		System.arraycopy(validated, 0, newValidated, 0, urlCount);
		origurls = newOrigUrls;
		urls = newUrls;
		validated = newValidated;
	}

	@Override
	public synchronized void addClasspathEntry(URL cpe) {
		ClassLoader loader = getClassLoader();
		if (loader != null) {
			if (cpe==null) {
				/*[MSG "K05ac", "URL is null for addClasspathEntry."]*/
				printVerboseError(Msg.getString("K05ac")); //$NON-NLS-1$
				return;
			}
			/* If array is not big enough, grow it */
			if (urls.length < (urlCount+1)) {
				growURLs(urlCount+1);
			}
			origurls[urlCount] = cpe;
			urls[urlCount] = convertJarURL(cpe);
			if (!validateURL(urls[urlCount], false)) {
				invalidURLExists = true;
			}
			++urlCount;
			notifyClasspathChange2(loader);
		}
	}

	/* Function required by the factory */
	synchronized URL[] getClasspath() {
		URL[] correctLengthArray = new URL[urlCount];
		System.arraycopy(origurls, 0, correctLengthArray, 0, urlCount);
		return correctLengthArray;
	}

	@Override
	public synchronized void confirmAllEntries() {
		confirmedCount = urlCount;
	}

	@Override
	public synchronized void setClasspath(URL[] newClasspath) throws CannotSetClasspathException {
		boolean changeMade = false;
		boolean invalidURLFound = false;
		int commonURLsLength = (origurls.length < newClasspath.length) ? origurls.length : newClasspath.length;
		
		ClassLoader loader = getClassLoader();
		if (loader == null) {
			/*[MSG "K059a", "ClassLoader has been garbage collected. Cannot set sharing filter."]*/
			throw new CannotSetClasspathException(Msg.getString("K059a")); //$NON-NLS-1$
		}

		if (newClasspath.length < confirmedCount) {
			/*[MSG "K05ad", "New classpath cannot be shorter than confirmed elements of original"]*/
			throw new CannotSetClasspathException(Msg.getString("K05ad")); //$NON-NLS-1$
		}
		for (int i=0; i<confirmedCount; i++) {
			if (!newClasspath[i].equals(origurls[i])) {
				/*[MSG "K05ae", "Index {0} of newClasspath does not match confirmed original"]*/
				throw new CannotSetClasspathException(Msg.getString("K05ae", i)); //$NON-NLS-1$
			}
		}
		
		/* Grow urls if necessary */
		if (newClasspath.length > origurls.length) {
			growURLs(newClasspath.length);
		}
		
		/* Having ensured that confirmed URLs are the same, validate the others if required, and copy them if they have been modified */
		for (int i = confirmedCount; i < commonURLsLength; i++) {
			boolean urlUpdated = !newClasspath[i].equals(origurls[i]);

			/* If the original classpath had any invalid URL then unconfirmed URLs in original classpath 
			 * should be validated again in case invalid URL has been corrected now.
			 */
			if (invalidURLExists || urlUpdated) {
				URL temp = null;
				if (urlUpdated) {
					temp = convertJarURL(newClasspath[i]);
				} else {
					temp = urls[i];
				}
				/* if an invalid URL is found, no need to validate any more URLs */
				if (!invalidURLFound && !validateURL(temp, false)) {
					invalidURLFound = true;
					/*[MSG "K05af", "setClasspath() found an invalid URL {0} at index {1}"]*/
					printVerboseInfo(Msg.getString("K05af", newClasspath[i], Integer.valueOf(i))); //$NON-NLS-1$
				}
				if (urlUpdated) {
					origurls[i] = newClasspath[i];
					urls[i] = temp;
					changeMade = true;
				}
			}
			/* validated[i] will already be false as it would be confirmed otherwise, so no need to set */
		}
		if (invalidURLFound) {
			invalidURLExists = true;
		} else {
			invalidURLExists = false;
		}

		for (int i = commonURLsLength; i < newClasspath.length; i++) {
			origurls[i] = newClasspath[i];
			urls[i] = convertJarURL(newClasspath[i]);
			
			/* if 'invalidURLExists' is already set to true, no need to validate any more URLs */
			if (!invalidURLExists && !validateURL(urls[i], false)) {
				/*[MSG "K05b0", "setClasspath() added new invalid URL {0} at index {1}"]*/
				printVerboseInfo(Msg.getString("K05b0", newClasspath[i], Integer.valueOf(i))); //$NON-NLS-1$
				
				invalidURLExists = true;
			}
			changeMade = true;
		}
		
		/* If new classpath is shorter, remaining entries will be ignored as they are > urlCount */
		if (urlCount != newClasspath.length) {
			urlCount = newClasspath.length;
			changeMade = true;
		}
		if (!invalidURLExists) {
			/*[MSG "K05b1", "setClasspath() updated classpath. No invalid URLs found"]*/
			printVerboseInfo(Msg.getString("K05b1")); //$NON-NLS-1$
		}
		/* Only call notifyClasspathChange if a genuine change has been made, otherwise internal data caches
		 * will be cleared unnecessarily */
		if (changeMade) {
			/*[MSG "K05b2", "setClasspath() updated classpath. Now urlCount={0}"]*/
			printVerboseInfo(Msg.getString("K05b2", urlCount)); //$NON-NLS-1$
			notifyClasspathChange2(loader);
		}
	}

	@Override
	String getHelperType() {
		return "SharedClassURLClasspathHelper"; //$NON-NLS-1$
	}
}
