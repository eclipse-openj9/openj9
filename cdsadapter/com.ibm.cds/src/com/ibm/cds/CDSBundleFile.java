/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package com.ibm.cds;


import java.net.MalformedURLException;
import java.net.URL;

import org.eclipse.osgi.storage.bundlefile.BundleEntry;
import org.eclipse.osgi.storage.bundlefile.BundleFile;
import org.eclipse.osgi.storage.bundlefile.BundleFileWrapper;

import com.ibm.oti.shared.SharedClassURLHelper;

/**
 * Wraps an actual BundleFile object for purposes of loading classes from the
 * shared classes cache. 
 */
public class CDSBundleFile extends BundleFileWrapper  {
	private URL url; // the URL to the content of the real bundle file
	private SharedClassURLHelper urlHelper; // the url helper set by the classloader
	private boolean primed = false;

	/**
	 * The constructor
	 * @param wrapped the real bundle file
	 */
	public CDSBundleFile(BundleFile wrapped) {
		super(wrapped);
		// get the url to the content of the real bundle file
		try {
			this.url = new URL("file", "", wrapped.getBaseFile().getAbsolutePath());
		} catch (MalformedURLException e) {
			// do nothing
		}
	}

	public CDSBundleFile(BundleFile bundleFile, SharedClassURLHelper urlHelper) {
		this(bundleFile);
		this.urlHelper = urlHelper;
	}

	/*
	 * (non-Javadoc)
	 * @see org.eclipse.osgi.storage.bundlefile.BundleFile#getEntry(java.lang.String)
	 * 
	 * If path is not for a class then just use the wrapped bundle file to answer the call. 
	 * If the path is for a class, it returns a CDSBundleEntry object.
	 * If the path is for a class, it will look for the magic cookie in the 
	 * shared classes cache. If found, the bytes representing the magic cookie are stored in CDSBundleEntry object.
	 */
	public BundleEntry getEntry(String path) {
		String classFileExt = ".class";
		BundleEntry wrappedEntry = super.getEntry(path);
		if (wrappedEntry == null) {
			return null;
		}
		if ((false == primed) || (false == path.endsWith(classFileExt))) {
			return wrappedEntry;
		}
		
		byte[] classbytes = getClassBytes(path.substring(0, path.length()-classFileExt.length()));
		BundleEntry be = new CDSBundleEntry(path, classbytes, wrappedEntry);
		return be;
	}

	/**
	 * Returns the file url to the content of the actual bundle file 
	 * @return the file url to the content of the actual bundle file
	 */
	URL getURL() {
		return url;
	}

	/**
	 * Returns the url helper for this bundle file.  This is set by the 
	 * class loading hook
	 * @return the url helper for this bundle file
	 */
	SharedClassURLHelper getURLHelper() {
		return urlHelper;
	}

	/**
	 * Sets the url helper for this bundle file.  This is called by the 
	 * class loading hook.
	 * @param urlHelper the url helper
	 */
	void setURLHelper(SharedClassURLHelper urlHelper) {
		this.urlHelper = urlHelper;
		this.primed = false; // always unprime when a new urlHelper is set
	}

	/**
	 * Sets the primed flag for the bundle file.  This is called by the 
	 * class loading hook after the first class has been loaded from disk for 
	 * this bundle file.
	 * @param primed the primed flag
	 */
	void setPrimed(boolean primed) {
		this.primed = primed;
	}

	/**
	 * Searches in the shared classes cache for the specified class name.
	 * @param name the name of the class
	 * @return the magic cookie to the shared class or null if the class is not in the cache.
	 */
	private byte[] getClassBytes(String name) {
		if (urlHelper == null || url == null)
			return null;
		return urlHelper.findSharedClass(url, name);
	}
}

