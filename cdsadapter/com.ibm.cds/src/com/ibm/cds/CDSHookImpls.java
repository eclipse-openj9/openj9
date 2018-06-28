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

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;

import org.eclipse.osgi.internal.hookregistry.BundleFileWrapperFactoryHook;
import org.eclipse.osgi.internal.hookregistry.ClassLoaderHook;
import org.eclipse.osgi.internal.loader.ModuleClassLoader;
import org.eclipse.osgi.internal.loader.classpath.ClasspathEntry;
import org.eclipse.osgi.internal.loader.classpath.ClasspathManager;
import org.eclipse.osgi.storage.BundleInfo.Generation;
import org.eclipse.osgi.storage.bundlefile.BundleEntry;
import org.eclipse.osgi.storage.bundlefile.BundleFile;
import org.eclipse.osgi.storage.bundlefile.BundleFileWrapper;
import org.eclipse.osgi.storage.bundlefile.BundleFileWrapperChain;

import com.ibm.oti.shared.HelperAlreadyDefinedException;
import com.ibm.oti.shared.Shared;
import com.ibm.oti.shared.SharedClassHelperFactory;
import com.ibm.oti.shared.SharedClassURLHelper;

public class CDSHookImpls extends ClassLoaderHook implements BundleFileWrapperFactoryHook {
	private static SharedClassHelperFactory factory = Shared.getSharedClassHelperFactory();
	private static java.lang.reflect.Method minimizeMethod = null;
	private static boolean hasMinimizeMethod = true;		/* Assume true to begin with */

	// With Equinox bug 226038 (v3.4), the framework will now pass an instance
	// of BundleFileWrapperChain rather than the wrapped BundleFile.  This is
	// so that multiple wrapping hooks can each wrap the BundleFile and all
	// wrappers are accessible.
	//
	// The Wrapper chain will look like below:
	// WrapperChain -> Wrapper<N> -> WrapperChain -> CDSBundleFile -> WrapperChain -> BundleFile
	//
	private static CDSBundleFile getCDSBundleFile(BundleFile bundleFile) {
		CDSBundleFile cdsBundleFile = null;

		if (bundleFile instanceof BundleFileWrapperChain) {
			// Equinox > 3.4
			BundleFile wrapped = null;
			do {
				wrapped = ((BundleFileWrapperChain) bundleFile).getWrapped();
				if (wrapped instanceof CDSBundleFile) {
					cdsBundleFile = (CDSBundleFile) wrapped;
					break;
				} 	
				
				//Go to next wrapper chain.
				bundleFile = ((BundleFileWrapperChain) bundleFile).getNext();				
			} while (wrapped != null);
		}
		return cdsBundleFile;
	}
	

	public void recordClassDefine(String name, Class clazz,
			byte[] classbytes, ClasspathEntry classpathEntry,
			BundleEntry entry, ClasspathManager manager) {		// only attempt to record the class define if:
		// 1) the class was found (clazz != null)
		// 2) the class has the magic class number CAFEBABE indicating a real class
		// 3) the bundle file for the classpath entry is of type CDSBundleFile
		// 4) class bytes is same as passed to weaving hook i.e. weaving hook did not modify the class bytes
		if ((null == clazz) || (false == hasMagicClassNumber(classbytes)) || (null == getCDSBundleFile(classpathEntry.getBundleFile()))) {
			return;
		}
		try {
			// check if weaving hook modified the class bytes
			byte originalClassBytes[] = entry.getBytes();
			if (originalClassBytes != classbytes) {
				// weaving hook has potentially modified the class bytes
				boolean modified = false;
				if (originalClassBytes.length == classbytes.length) {
					// do a byte-by-byte comparison
					modified = !Arrays.equals(classbytes, originalClassBytes);
				} else {
					modified = true;
				}
				if (modified) {
					// Class bytes have been modified by weaving hooks. 
					// Such classes need to be stored as Orphans, so skip the call to storeSharedClass()
					return;
				}
			}
		} catch (IOException e) {
			// this should never happen, but in case it does, its safe to return
			return;
		}
		
		CDSBundleFile cdsFile = getCDSBundleFile(classpathEntry.getBundleFile());
		
		if (null == cdsFile.getURL()) {
			// something went wrong trying to determine the url to the real bundle file
			return; 
		}

		// look for the urlHelper; if it does not exist then we are not sharing for this class loader
		SharedClassURLHelper urlHelper = cdsFile.getURLHelper(); 
		if (urlHelper == null) {
			// this should never happen but just in case get the helper from the base host bundle file.
			CDSBundleFile hostBundleFile = getCDSBundleFile(manager.getGeneration().getBundleFile());
			if (null != hostBundleFile) {
				// try getting the helper from the host base cdsFile
				urlHelper = hostBundleFile.getURLHelper();
			}
			
			if (null != urlHelper) {
				cdsFile.setURLHelper(urlHelper);
			}		
		}
		if (null != urlHelper) {
			// store the class in the cache
			urlHelper.storeSharedClass(cdsFile.getURL(), clazz);
			cdsFile.setPrimed(true);
		}
	}

	/* Calling setMinimizeUpdateChecks() on the urlHelper tells it to only check the plugin jar for updates
	 * once on startup. This removes the need to "prime" plugins by always cacheing the first class from the jar.
	 * 
	 * Java5 does not have a runMinimizeUpdateChecks method, but Java6 does. The text below explains why.
	 * 
	 * Java6 has an improved jar update detection mechanism which is event-driven and listens for
	 * real jar open and close events. It will check jar timestamps on every class-load for closed jars (when
	 * loading cached classes from those jars) and not check them if it knows the jars are open.
	 *  
	 * Java5 didn't know about jar open/close events so simply assumed that the first class to be stored by
	 * a plugin implied that its jar was opened indefinitely. This is why it helps to "prime" a plugin when 
	 * running under Java5 - by storing a class, the jar is opened and the JVM stops checking its timestamp 
	 * which results in faster startup.
	 * 
	 * While the Java6 behaviour is more correct (it will pick up changes if a jar is closed after having been opened),
	 * if the jars are not opened or "primed", then it will perform constant checks on their timestamps which hurts startup times.
	 * This is why setMinimizeUpdateChecks was introduced - it's a means of saying to the urlHelper - regardless of
	 * whether my container(s) is open or closed, I only want you to check it once for updates.
	 * 
	 * The consequence of this is that less file handles are open on startup in Java6.
	 * 
	 * This has been written in such a way that this adaptor will continue to work exactly the same with Java5, but
	 * will adapt its behaviour when used with Java6 to do the right thing.
	 */
	private boolean runMinimizeMethod(SharedClassURLHelper urlHelper) {
		if (hasMinimizeMethod && (urlHelper != null)) {
			if (minimizeMethod == null) {
				hasMinimizeMethod = false;		/* Assume failure - prove success below */
				try {
					Class c = urlHelper.getClass();
					/* Not supported in the Java6 GA, but will be in SR1 + */
					java.lang.reflect.Field isSupported = c.getField("MINIMIZE_ENABLED");
					if (isSupported != null) {			/* Field doesn't exist in Java5 */
						if (isSupported.getBoolean(urlHelper)) {
							minimizeMethod = c.getMethod("setMinimizeUpdateChecks", null);
							hasMinimizeMethod = true;
						}
					}
				} catch (Exception e) {
					/* hasMinimizeMethod will be false and we won't try this again */
				}
			}
			if (minimizeMethod != null) {
				try {
					minimizeMethod.invoke(urlHelper, null);
					return true;
				} catch (Exception e) {
					hasMinimizeMethod = false;
				}
			}
		}
		return false;
	}
	
	private boolean hasMagicClassNumber(byte[] classbytes) {
		if (classbytes == null || classbytes.length < 4)
			return false;
		// TODO maybe there is a better way to do this? I'm not sure why I had to AND each byte with the value I was checking ...
		return (classbytes[0] & 0xCA) == 0xCA && (classbytes[1] & 0xFE) == 0xFE && (classbytes[2] & 0xBA) == 0xBA && (classbytes[3] & 0xBE) == 0xBE;
	}

	public void classLoaderCreated(ModuleClassLoader classLoader) {
		// try to get the url helper for this class loader
		if (factory == null) {
			return;
		}
		CDSBundleFile hostFile = null;
		try {
			SharedClassURLHelper urlHelper = factory.getURLHelper(classLoader);
			boolean minimizeSucceeded = runMinimizeMethod(urlHelper);
			// set the url helper for the host base CDSBundleFile
			hostFile = getCDSBundleFile(classLoader.getClasspathManager().getGeneration().getBundleFile());
			if (hostFile != null) {
				hostFile.setURLHelper(urlHelper);
				if (minimizeSucceeded) {
					/* In Java6, there is no longer a requirement to "prime" plugins */
					hostFile.setPrimed(true);
				}
			}
		} catch (HelperAlreadyDefinedException e) {
			// We should never get here. 
			// If we do, we simply won't share for this ClassLoader
		}
	}
	
	public boolean addClassPathEntry(ArrayList cpEntries,
		String cp, ClasspathManager hostmanager, Generation sourceGeneration) {
		CDSBundleFile hostFile = getCDSBundleFile(hostmanager.getGeneration().getBundleFile());
		CDSBundleFile sourceFile = getCDSBundleFile(sourceGeneration.getBundleFile());		
		if ((hostFile != sourceFile) && (null != hostFile) && (null != sourceFile)) {
			// set the helper that got set on the host base bundle file in initializedClassLoader
			SharedClassURLHelper urlHelper = hostFile.getURLHelper();
			sourceFile.setURLHelper(urlHelper);
		}
		
		return false;
	}

	//////////////// BundleFileWrapperFactoryHook //////////////
	public BundleFileWrapper wrapBundleFile(BundleFile bundleFile, Generation generation, boolean base) {
		// wrap the real bundle file for purposes of loading shared classes.
		CDSBundleFile newBundleFile;
		if (!base && generation.getBundleInfo().getBundleId() != 0) {
			// initialize the urlHelper from the base one.
			SharedClassURLHelper urlHelper = null;
			BundleFile baseFile = generation.getBundleFile();
			if ((baseFile = getCDSBundleFile(baseFile)) != null) {
				urlHelper = ((CDSBundleFile) baseFile).getURLHelper();
			}
			newBundleFile = new CDSBundleFile(bundleFile, urlHelper);
		} else {
			newBundleFile = new CDSBundleFile(bundleFile);
		}
					
		return newBundleFile;
	}
}
