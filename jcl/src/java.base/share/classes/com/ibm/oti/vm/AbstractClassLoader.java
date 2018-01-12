/*[INCLUDE-IF Sidecar16 & !Sidecar19-SE]*/

package com.ibm.oti.vm;

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
 

import java.lang.ref.SoftReference;
import java.net.URL;
import java.net.MalformedURLException;
import java.net.URLStreamHandler;
import java.util.Enumeration;
import java.util.Vector;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.concurrent.ConcurrentHashMap;
import java.util.jar.JarFile;
import java.util.jar.Manifest;
import java.util.jar.Attributes;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.io.*;

public abstract class AbstractClassLoader extends ClassLoader {
	private static ClassLoader systemClassLoader;
	String[] parsedPath;
	int[] types;
	Object[] cache;
	private static final class CacheLock {}
	static Object cacheLock = new CacheLock();
	FilePermission permissions[];
	volatile SoftReference<ConcurrentHashMap<String, Vector>> resourceCacheRef;

	private static final class ManifestLock {}
	private static Object manifestLock = new ManifestLock();
	/*[PR JAZZ 88959] Use URLStreamHandler when creating bootstrap resource URLs */
	private static URLStreamHandler	urlJarStreamHandler;
	private static URLStreamHandler	urlFileStreamHandler;
	private static URLStreamHandler	urlJrtStreamHandler;
	
public AbstractClassLoader(){
}

public AbstractClassLoader(ClassLoader p1){
	super(p1);
}


void fillCache(final int i) {
	setTypeElement(i, VM.getClassPathEntryType(this, i));
	switch (types[i]) {
		case VM.CPE_TYPE_UNKNOWN:
			setCacheElement(i, cache);
			return;
		case VM.CPE_TYPE_JAR:
		case VM.CPE_TYPE_DIRECTORY:
			if (parsedPath[i] == null) {
				setParsedPathElement(i, com.ibm.oti.util.Util.toString(VM.getPathFromClassPath(i)));
			}
			File f = new File(parsedPath[i]);
			String path;
			try {
				path = f.getCanonicalPath();
			} catch (IOException e) {
				path = f.getAbsolutePath();
			}
			if (types[i] == VM.CPE_TYPE_DIRECTORY) {
				if (path.charAt(path.length() -1) != File.separatorChar) {
					StringBuilder buffer = new StringBuilder(path.length() + 1);
					path = buffer.append(path).append(File.separatorChar).toString();
				}
				setParsedPathElement(i, path);
				setCacheElement(i, cache);
			} else {
				// Must store absolute path so correct URL is created
				setParsedPathElement(i, path);
				try {
					ZipFile zf;
					zf = new JarFile(parsedPath[i]);
					setCacheElement(i, zf);
					return;
				} catch (IOException e) {}
				setTypeElement(i, VM.CPE_TYPE_UNUSABLE);
				setCacheElement(i, cache);
			}
			return;
		case VM.CPE_TYPE_JIMAGE:
		case VM.CPE_TYPE_UNUSABLE:
			setCacheElement(i, cache);
			return;
	}
}

private void setCacheElement(int i, Object value) {
	synchronized(cacheLock) {
		cache[i] = value;
	}
}

private void setTypeElement(int i, int value) {
	synchronized(cacheLock) {
		types[i] = value;
	}
}

private void setParsedPathElement(int i, String value) {
	synchronized(cacheLock) {
		parsedPath[i] = value;
	}
}


/**
 * Answers a string representing the URL which matches the
 * given filename. The argument should be specified using the
 * standard platform rules. If it is not absolute, then it
 * is converted before use. The result will end in a slash
 * if the original path ended in a file separator.
 *
 * @param filename the filename String to convert to URL form
 * @param cpType an int which indicates type of the URL
 * 
 * @return the URL formatted filename
 */
static String toURLString(String filename, int cpType) {
	String name = filename;
	if (File.separatorChar != '/') {
		// Must convert slashes.
		name = name.replace(File.separatorChar, '/');
	}
	/*[PR CMVC 130715] java.net.URISyntaxException with space in path */
	name = com.ibm.oti.util.Util.urlEncode(name);
	int length = name.length() + 6;
	if (cpType == VM.CPE_TYPE_JAR) {
		length += 4;
	}
	StringBuilder buf = new StringBuilder(length);
	if (cpType == VM.CPE_TYPE_JAR) {
		buf.append("jar:file:"); //$NON-NLS-1$
	} else if (cpType == VM.CPE_TYPE_DIRECTORY) {
	buf.append("file:"); //$NON-NLS-1$
	} else if (cpType == VM.CPE_TYPE_JIMAGE) {
		buf.append("jrt:"); //$NON-NLS-1$
	}
	if (!name.startsWith("/")) { //$NON-NLS-1$
		// On Windows, absolute paths might not start with sep.
		buf.append('/');
	}
	name = buf.append(name).toString();
	return name;
}

public static void setBootstrapClassLoader(ClassLoader bootstrapClassLoader) {
	// Should only be called once
	if (systemClassLoader != null)
		throw new IllegalArgumentException();
	systemClassLoader = bootstrapClassLoader;
	urlJarStreamHandler = new sun.net.www.protocol.jar.Handler();
	urlFileStreamHandler = new sun.net.www.protocol.file.Handler();
/*[IF Sidecar19-SE]*/
	urlJrtStreamHandler = new sun.net.www.protocol.jrt.Handler();
/*[ENDIF] Sidecar19-SE */
}

Package definePackage(String packageName, final int cacheIndex) {
	if (cacheIndex >= 0 && cache[cacheIndex] == null) {
		AccessController.doPrivileged(new PrivilegedAction() {
		public Object run() {
			fillCache(cacheIndex);
			return null;
		}});
	}

	if (cacheIndex >= 0 && types[cacheIndex] == VM.CPE_TYPE_JAR) {
		Manifest manifest = null;
		try {
			JarFile jf = (JarFile)cache[cacheIndex];
			manifest = jf.getManifest();
		} catch (IOException e) {}
		if (manifest != null) {
			/*[PR CMVC 76124] Deadlock loading char converter classes */
			synchronized(manifestLock) {
				String specTitle = null, specVersion = null, specVendor = null;
				String implTitle = null, implVersion = null, implVendor = null;
				/*[PR CMVC 98059] do not recursively call BootstrapClassLoader.getPackage() */
				Package result = super.getPackage(packageName);
				if (result != null) return result;
				Attributes mainAttributes = manifest.getMainAttributes();
				String value = mainAttributes.getValue(Attributes.Name.SEALED);
				boolean sealed = value != null &&
					value.toLowerCase().equals ("true"); //$NON-NLS-1$
				String dirName = packageName.replace('.', '/') + "/"; //$NON-NLS-1$
				Attributes attributes = manifest.getAttributes(dirName);
				if (attributes != null) {
					value = attributes.getValue(Attributes.Name.SEALED);
					if (value != null)
						sealed = value.toLowerCase().equals("true"); //$NON-NLS-1$
					/*[PR 122738] use package specific version information */
					specTitle = attributes.getValue(Attributes.Name.SPECIFICATION_TITLE);
					specVersion = attributes.getValue(Attributes.Name.SPECIFICATION_VERSION);
					specVendor = attributes.getValue(Attributes.Name.SPECIFICATION_VENDOR);
					implTitle = attributes.getValue(Attributes.Name.IMPLEMENTATION_TITLE);
					implVersion = attributes.getValue(Attributes.Name.IMPLEMENTATION_VERSION);
					implVendor = attributes.getValue(Attributes.Name.IMPLEMENTATION_VENDOR);
				}
				URL url = null;
				try {
					if (sealed) {
						/*[PR CMVC 193930] Must use the saved context when creating new URLs */
						url = new URL(null, toURLString(parsedPath[cacheIndex], VM.CPE_TYPE_DIRECTORY), urlFileStreamHandler);
					}
				} catch (MalformedURLException e) {}
				if (specTitle == null) specTitle = mainAttributes.getValue(Attributes.Name.SPECIFICATION_TITLE);
				if (specVersion == null) specVersion = mainAttributes.getValue(Attributes.Name.SPECIFICATION_VERSION);
				if (specVendor == null) specVendor = mainAttributes.getValue(Attributes.Name.SPECIFICATION_VENDOR);
				if (implTitle == null) implTitle = mainAttributes.getValue(Attributes.Name.IMPLEMENTATION_TITLE);
				if (implVersion == null) implVersion = mainAttributes.getValue(Attributes.Name.IMPLEMENTATION_VERSION);
				if (implVendor == null) implVendor = mainAttributes.getValue(Attributes.Name.IMPLEMENTATION_VENDOR);
				
				return definePackage(packageName, 
					specTitle, specVersion, specVendor,
					implTitle, implVersion, implVendor,
					url);
			}
		}
	}
	/*[PR CMVC 76124] Deadlock loading char converter classes */
	synchronized(manifestLock) {
		/*[PR CMVC 98059] do not recursively call BootstrapClassLoader.getPackage() */
		Package result = super.getPackage(packageName);
		if (result != null) return result;
		return definePackage(packageName, null, null, null, null, null, null, null);
	}
}

/**
 * Answers the name of the package to which newClass belongs.
 */
String getPackageName(Class theClass)
{
	String name;
	int index;

	/*[PR 126182] Do not intern bootstrap class names when loading */
	name = VM.getClassNameImpl(theClass);
	if((index = name.lastIndexOf('.')) == -1) return null;
	return name.substring(0, index);
}

/*[IF Sidecar19-SE]*/
/*[IF AnnotateOverride]*/
@Override
/*[ENDIF]*/
protected URL findResource(String moduleName, final String res) throws IOException {
	return jdk.internal.loader.BootLoader.findResource(moduleName, res);
}
/*[ENDIF] Sidecar19-SE*/

/*[IF !Sidecar19-SE]*/

/*[IF AnnotateOverride]*/
@Override
/*[ENDIF]*/
protected URL findResource(final String res) {
	URL result = (URL)AccessController.doPrivileged(new PrivilegedAction() {
		public Object run() {
			for (int i=0; i<cache.length; i++) {
				URL result = findResourceImpl(i, res);
				if (result != null) return result;
			}
			return null;
		}});
	if (result != null) {
		SecurityManager sm = System.getSecurityManager();
		if (sm != null) {
			try {
				sm.checkPermission(result.openConnection().getPermission());
			} catch (IOException e) {
				return null;
			} catch (SecurityException e) {
				return null;
			}
		}
	}
	return result;
}

private URL findResourceImpl(int i, String res) {
	/*[PR 123382] Throw NullPointerException when res is null */
	if (res.length() > 0 && res.charAt(0) == '/')
		return null;	// Do not allow absolute resource references!

	if (cache[i] == null) fillCache(i);
	try {
		switch (types[i]) {
			case VM.CPE_TYPE_JAR:
				ZipFile zf = (ZipFile)cache[i];
				if (zf.getEntry(res) != null) {
					/*[PR CMVC 193930] Must use the saved context when creating new URLs */
					return new URL(null, toURLString(parsedPath[i] + "!/" + res, types[i]), urlJarStreamHandler); //$NON-NLS-1$
				}
				return null;
			case VM.CPE_TYPE_DIRECTORY:
				StringBuilder buffer = new StringBuilder(parsedPath[i].length() + res.length());
				String resourcePath = buffer.append(parsedPath[i]).append(res).toString();
				File f = new File(resourcePath);
				if (f.exists()) {
					/*[PR CMVC 193930] Must use the saved context when creating new URLs */
					return new URL(null, toURLString(resourcePath, types[i]), urlFileStreamHandler);
				}
				return null;
			case VM.CPE_TYPE_JIMAGE:
				/* Do not find resource in jimage */
				return null;
		}
	} catch (MalformedURLException e) {
	}
	return null;
}

/*[IF AnnotateOverride]*/
@Override
/*[ENDIF]*/
protected Enumeration findResources(final String res) throws IOException {
	if (resourceCacheRef != null) {
		ConcurrentHashMap<String, Vector> resourceCache = resourceCacheRef.get();
		if (resourceCache != null) {
			Vector result = resourceCache.get(res);
			if (result != null) return result.elements();
		}
	}
	Vector result = (Vector)AccessController.doPrivileged(new PrivilegedAction() {
		public Object run() {
			Vector resources = new Vector();
			for (int i=0; i < cache.length; i++) {
				URL resource = findResourceImpl(i, res);
				if (resource != null) resources.addElement(resource);
			}
			return resources;
		}});
	SecurityManager sm;
	int length = result.size();
	if (length > 0 && (sm = System.getSecurityManager()) != null) {
		Vector reduced = new Vector(length);
		for (int i=0; i<length; i++) {
			URL url = (URL)result.elementAt(i);
			try {
				sm.checkPermission(url.openConnection().getPermission());
				reduced.addElement(url);
			} catch (IOException e) {
			} catch (SecurityException e) {
			}
		}
		result = reduced;
	}
	
	ConcurrentHashMap<String, Vector> resourceCache;
	if (resourceCacheRef == null || (resourceCache = resourceCacheRef.get()) == null) {
		synchronized(cacheLock) {
			if (resourceCacheRef == null || (resourceCache = resourceCacheRef.get()) == null) {
				resourceCache = new ConcurrentHashMap<String, Vector>(64);
				resourceCacheRef = new SoftReference(resourceCache);
			}
		}
	}
	resourceCache.put(res,  result);
	return result.elements();
}

/**
 * Answers a stream on a resource found by looking up
 * resName using the class loader's resource lookup
 * algorithm.
 *
 * @return		InputStream
 *					a stream on the resource or null.
 * @param		resName	String
 *					the name of the resource to find.
 */
/*[IF AnnotateOverride]*/
@Override
/*[ENDIF]*/
public InputStream getResourceAsStream(String resName) {
	if (resName == null || resName.length() < 1 || resName.charAt(0) == '/')
		return null;	// Do not allow absolute resource references!
	InputStream answer;
	if (this != systemClassLoader) {
		if (getParent() == null)
			answer = systemClassLoader.getResourceAsStream(resName);
		else
			answer = getParent().getResourceAsStream(resName);
		if (answer != null) return answer;
	}

	int length;
	synchronized(cacheLock) {
		length = cache.length;
	}
	for (int i = 0; i < length; ++i) {
		try {
			if (cache[i] == null) fillCache(i);
			switch (types[i]) {
				case VM.CPE_TYPE_JAR:
					final ZipFile zf = (ZipFile)cache[i];
					ZipEntry entry;
					if ((entry = zf.getEntry(resName)) != null) {
						SecurityManager security = System.getSecurityManager();
						if (security != null) {
							initalizePermissions();
							if (permissions[i] == null) {
								setPermissionElement(i, new FilePermission(parsedPath[i], "read")); //$NON-NLS-1$
							}
							security.checkPermission(permissions[i]);
						}
						try {
							return zf.getInputStream(entry);
						} catch (IOException e) {
						}
					}
					break;
				case VM.CPE_TYPE_DIRECTORY:
					StringBuilder buffer = new StringBuilder(parsedPath[i].length() + resName.length());
					String resourcePath = buffer.append(parsedPath[i]).append(resName).toString();
					InputStream result = openFile(resourcePath);
					if (result!=null)
						return result;
					break;
				case VM.CPE_TYPE_JIMAGE:
					/* Do not find resource in jimage */
					break;
			}
		} catch (SecurityException e) {}
	}
	return null;
}

private void initalizePermissions() {
	synchronized(cacheLock) {
		if (permissions == null) {
			permissions = new FilePermission[cache.length];
		}
	}
}

private InputStream openFile(String resourcePath) {
	File f = new File(resourcePath);
	if (f.exists()) {
		try {
			return new BufferedInputStream(new FileInputStream(f));
		} catch (FileNotFoundException e) { }
	}
	return null;
}

private void setPermissionElement(int i, FilePermission value) {
	synchronized(cacheLock) {
		permissions[i] = value;
	}
}

/*[ENDIF]*/  /*  !Sidecar19-SE */

}
