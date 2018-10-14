/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

import com.ibm.oti.util.Msg;
import com.ibm.oti.util.Util;
import java.io.File;
import java.net.MalformedURLException;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.URI;
import java.security.AccessController;
import java.security.PrivilegedAction;

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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
 * SharedClassAbstractHelper provides common functions and data to class helper subclasses.
 * <p>
 * @see SharedClassHelper
 * @see SharedAbstractHelper
 */
public abstract class SharedClassAbstractHelper extends SharedAbstractHelper implements SharedClassHelper {

	int ROMCLASS_COOKIE_SIZE;
	private static final int URI_EXCEPTION = 1;
	private static final int FILE_EXIST = 2;
	private static final int FILE_NOT_EXIST = 3;
	private SharedClassFilter sharedClassFilter; 

	static byte[] nativeFlags = new byte[1];
	static final int CACHE_FULL_FLAG = 0;
	
	static {
		nativeFlags[CACHE_FULL_FLAG] = 0;
	}
	 
	/**
	 * Utility function. Determines whether a byte array being passed to defineClass is a class found
	 * in the shared class cache, or a class found locally.
	 * <p>
	 * @param 		classBytes A potential shared class cookie
	 * @return 		true if bytes are a cookie.
	 */
	public boolean isSharedClassCookie(byte[] classBytes) {
		return (classBytes.length == ROMCLASS_COOKIE_SIZE);
	}

	private native int initializeShareableClassloaderImpl(ClassLoader loader);

	void initializeShareableClassloader(ClassLoader loader) {
		/* Allow ClassLoader to be Garbage Collected by keeping a WeakReference. 
		 * This is important as any live references to the ClassLoader will 
		 * prevent it being collected. CMVC 98943. */
		ROMCLASS_COOKIE_SIZE = initializeShareableClassloaderImpl(loader);
	}

	URL convertJarURL(URL url) {
		if (url==null)
			return null;

		if (!url.getProtocol().equals("jar")) { //$NON-NLS-1$
			return url;
		} else {
			String jarPath = recursiveJarTrim(url.getPath());
			try {
				return new URL(jarPath);
			} catch (MalformedURLException e) {
				return url;
			}
		}
	}

	private String recursiveJarTrim(String jarName) {
		if (jarName==null)
			return null;

		boolean startsWithJar = jarName.startsWith("jar:"); //$NON-NLS-1$
		boolean endsWithBang = jarName.endsWith("!/");//$NON-NLS-1$
		int subStringStart = startsWithJar ? 4 : 0;
		int len = jarName.length();
		/*
		 * The possible separator "!/" at the end of the URL path is useless for us. Throw it away. 
		 * We only care about the actual path string before "!/" .
		 * 
		 * We do not trim "!/" in the middle of the URL path. We need the string after "!/" to distinguish 
		 * between different entries from the same nested jar.
		 * e.g. /path/A.jar!/lib/B.jar, /path/A.jar!/lib/C.jar, /path/A.jar are treated as different paths even though they are from the same jar.
		 */
		int subStringEnd = endsWithBang ? len -2 : len;

		if (!startsWithJar && !endsWithBang) {
			return jarName;
		} else {
			return recursiveJarTrim(jarName.substring(subStringStart, subStringEnd));
		}
	}
	
	private static URL getURLToCheck(URL url) {
		String pathString = url.toString();
		int indexBang = pathString.indexOf("!/"); //$NON-NLS-1$

		if (-1 != indexBang) {
			/* For a nested jar (e.g. /path/A.jar!/lib/B.jar), validate the external jar file only (/path/A.jar), 
			 * so trim the entry within the jar after "!/" 
			 */
			pathString = pathString.substring(0, indexBang);
			try {
				return new URL(pathString);
			} catch (MalformedURLException e) {
				return url;
			}
		}
		return url;
	}

	boolean validateClassLoader(ClassLoader loader, Class<?> clazz) {
		if (loader==null) {
			/*[MSG "K0595", "Attempt to store {0} into garbage collected ClassLoader."]*/
			printVerboseError(Msg.getString("K0595", clazz.getName())); //$NON-NLS-1$
			return false;
		}
		if (!loader.equals(clazz.getClassLoader())) {
			/*[MSG "K0596", "ClassLoader of SharedClassHelper and ClassLoader of class {0} do not match."]*/
			printVerboseError(Msg.getString("K0596", clazz.getName())); //$NON-NLS-1$
			return false;
		}
		return true;
	}

	boolean validateURL(URL url, boolean checkExists) {
		if (null == url) {
			return true;
		}
		String protocol = url.getProtocol();
		if (!(protocol.equals("file") || protocol.equals("jar"))) { //$NON-NLS-1$ //$NON-NLS-2$
			/*[MSG "K0597", "URL {0} does not have required file or jar protocol."]*/
			printVerboseInfo(Msg.getString("K0597", url.toString())); //$NON-NLS-1$
			return false;
		}
		if (checkExists) {
			final URL urlToCheck = getURLToCheck(url);
			Integer fExists = AccessController.doPrivileged(new PrivilegedAction<Integer>() {
				@Override
				public Integer run() {
					File f;
					try {
						f = new File(urlToCheck.toURI());
					} catch (URISyntaxException e) {
						// try URL encoding the path
						try {
							f = new File(new URI(Util.urlEncode(urlToCheck.toExternalForm())));
						} catch (URISyntaxException e2) {
							// try the simple string path
							f = new File(urlToCheck.getPath());
						} catch (IllegalArgumentException e2) {
							// as below
							f = new File(urlToCheck.getPath());
						}
					} catch (IllegalArgumentException e) {
						/* There is a bug in the URI code which does not handle all UNC paths correctly 
						 * If we hit this, revert to using simple string path */
						f = new File(urlToCheck.getPath());
					}
					return Integer.valueOf((f.exists() ? FILE_EXIST : FILE_NOT_EXIST));
				}
			});
			if (fExists.intValue() == FILE_NOT_EXIST) {
				/*[MSG "K0598", "URL resource {0} does not exist."]*/
				printVerboseError(Msg.getString("K0598", url.toString())); //$NON-NLS-1$
				return false;
			} else
			if (fExists.intValue() == URI_EXCEPTION) {
				/*[MSG "K0599", "URI could not be created from the URL {0}"]*/
				printVerboseError(Msg.getString("K0599", url.toString())); //$NON-NLS-1$
				return false;
			}
		}
		return true;
	}

	/**
	 * Sets the SharedClassFilter for a helper instance.  Supplying
	 * null removes any filter that is currently associated with the
	 * helper instance.
	 * <p>
	 * @param filter The filter to use when finding and storing classes.
	 */
	@Override
	public synchronized void setSharingFilter(SharedClassFilter filter) {
		if (System.getSecurityManager()!=null) {
			ClassLoader loader = getClassLoader();
			if (loader == null) {
				/*[MSG "K059a", "ClassLoader has been garbage collected. Cannot set sharing filter."]*/
				printVerboseInfo(Msg.getString("K059a")); //$NON-NLS-1$
				return;
			}
			if (!checkReadPermission(loader)) {
				/*[MSG "K059b", "Read permission denied. Cannot set sharing filter."]*/
				printVerboseError(Msg.getString("K059b")); //$NON-NLS-1$
				return;
			}
			if (!checkWritePermission(loader)) {
				/*[MSG "K059c", "Write permission denied. Cannot set sharing filter."]*/
				printVerboseError(Msg.getString("K059c")); //$NON-NLS-1$
				return;
			}
		}
		this.sharedClassFilter = filter;
	}

	/**
	 * Returns the SharedClassFilter associated with this helper.
	 * <p>
	 * @return The filter instance, or null if none is associated
	 */
	@Override
	public synchronized SharedClassFilter getSharingFilter() {
		return this.sharedClassFilter;
	}
}
