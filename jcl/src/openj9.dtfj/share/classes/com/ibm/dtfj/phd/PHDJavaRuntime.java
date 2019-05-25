/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2019 IBM Corp. and others
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
package com.ibm.dtfj.phd;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Properties;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.dtfj.java.JavaVMInitArgs;
import com.ibm.dtfj.phd.parser.HeapdumpReader;
import com.ibm.dtfj.phd.parser.PortableHeapDumpListener;
import com.ibm.dtfj.phd.util.LongEnumeration;

/** 
 * @author ajohnson
 */
class PHDJavaRuntime implements JavaRuntime {

	private final List<PHDJavaHeap> heaps = new ArrayList<PHDJavaHeap>();
	private final LinkedHashMap<JavaObject,PHDJavaClassLoader> loaders = new LinkedHashMap<JavaObject,PHDJavaClassLoader>();
	private final LinkedHashMap<JavaThread,JavaThread> threads = new LinkedHashMap<JavaThread,JavaThread>();
	private final ArrayList<JavaMonitor> monitors = new ArrayList<JavaMonitor>();
	private final HashMap<Long,JavaClass>classIdCache = new HashMap<Long,JavaClass>();
	private final HashMap<String,JavaClass>classNameCache = new HashMap<String,JavaClass>();
	private final HashMap<Long, JavaObject> extraObjectsCache = new HashMap<Long, JavaObject>();
	static final String arrayTypeName[]={"[Z","[C","[F","[D","[B","[S","[I","[J"};
	private final JavaClass arrayClasses[] = new JavaClass[arrayTypeName.length];
	private static final long[] NOREFS = {};
	private final ImageAddressSpace space;
	private final JavaRuntime metaJavaRuntime;
	private final PHDImageProcess process;
	private String full_version;
	long minAddress;
	long maxAddress;
	long minClassAddress;
	long maxClassAddress;
	long maxObjClass;
	int maxObjLen;
	int minInstanceSize;
	private long compressAddressBase1;
	private long compressAddressTop1;
	private long compressAddressBase2;
	private int compressIndexBase1;
	private int compressIndexBase2;
	private int compressShift;
	private long nextClsAddr;

	PHDJavaRuntime(ImageInputStream stream, final PHDImage parentImage, ImageAddressSpace space, PHDImageProcess process, JavaRuntime metaJavaRuntime) throws IOException {
		this.space = space;
		this.process = process;
		this.metaJavaRuntime = metaJavaRuntime;
		HeapdumpReader reader = new HeapdumpReader(stream, parentImage);
		heaps.add(new PHDJavaHeap(stream, parentImage, space, this));
		PHDJavaClassLoader loader = new PHDJavaClassLoader(stream, parentImage, space, this);
		processData(reader, parentImage, loader);
		findLoaders(reader);
		initClassCache();
		initMonitors();
		initThreads();
	}

	PHDJavaRuntime(File file, final PHDImage parentImage, ImageAddressSpace space, PHDImageProcess process, JavaRuntime metaJavaRuntime) throws IOException {
		this.space = space;
		this.process = process;
		this.metaJavaRuntime = metaJavaRuntime;
		HeapdumpReader reader = new HeapdumpReader(file, parentImage);
		heaps.add(new PHDJavaHeap(file, parentImage, space, this));
		PHDJavaClassLoader loader = new PHDJavaClassLoader(file, parentImage, space, this);
		processData(reader, parentImage, loader);
		findLoaders(reader);
		initClassCache();
		initMonitors();
		initThreads();
	}
	
	private void processData(HeapdumpReader reader, final PHDImage parentImage, PHDJavaClassLoader loader) throws IOException {
//		this.space = space;
//		this.process = process;
//		this.metaJavaRuntime = metaJavaRuntime;
		this.full_version = reader.full_version();
		reader.close();
		reader = null;
//		heaps.add(new PHDJavaHeap(file, parentImage, space, this));
//		PHDJavaClassLoader loader = new PHDJavaClassLoader(file, parentImage, space, this);
		minAddress = loader.minAddress;
		maxAddress = loader.maxAddress;
		minClassAddress = loader.minClassAddress;
		maxClassAddress = loader.maxClassAddress;
		minInstanceSize = loader.minInstanceSize;
		maxObjClass = loader.maxObjClass;
		maxObjLen = loader.maxObjLen;
		/*
		 * Find out the range of object and class addresses. We can then use these ranges to compress a long
		 * address to an int.
		 * [compressAddressBase1, compressAddressTop1] gap [compressAddressBase2, top]
		 * [na,xa] [nc,xc]
		 * Possible arrangements
		 * na,xa,nc,xc => gap
		 * na,nc,xa,xc
		 * nc,na,xa,xc
		 * na,nc,xc,xa
		 * nc,na,xc,xa
		 * nc,xc,na,xa => gap
		 */
		compressAddressBase1 = Math.min(minAddress, minClassAddress);
		compressAddressTop1 = Math.min(maxAddress, maxClassAddress);
		long l1 = Math.max(minAddress, maxClassAddress);
		long l2 = Math.max(minClassAddress, maxAddress);
		compressAddressBase2 = Math.min(l1 ,l2);
		int i;
		for (i = 0; i < 10; ++i) {
			if (((1L << i) & loader.bitsAddress) != 0) break;
		}
		compressShift = i;
		compressIndexBase1 = Integer.MIN_VALUE;
		compressIndexBase2 = (int)((compressAddressTop1 - compressAddressBase1) >>> compressShift) + compressIndexBase1 + 1;
		
		loaders.put(null, loader);
		// Initialize the PHD primitive array types list
		for (int j = 0; j < arrayTypeName.length; ++j) {
			arrayClasses[j] = findClass(arrayTypeName[j]);
		}
		prepThreads();
		prepMonitors();
		prepClassLoaders();
//		findLoaders(file, parentImage);
//		initClassCache();
//		initMonitors();
//		initThreads();
	}

	public Iterator<JavaMethod> getCompiledMethods() {
		return Collections.<JavaMethod>emptyList().iterator();
	}

	public Iterator<? extends JavaHeap> getHeaps() {
		return heaps.iterator();
	}

	public Iterator<? extends JavaClassLoader> getJavaClassLoaders() {
		return loaders.values().iterator();
	}

	public ImagePointer getJavaVM() throws CorruptDataException {
		long addr = 0;
		if (metaJavaRuntime != null) addr = metaJavaRuntime.getJavaVM().getAddress();
		return space.getPointer(addr);
	}

	public JavaVMInitArgs getJavaVMInitArgs() throws DataUnavailable,
			CorruptDataException {
		if (metaJavaRuntime != null) return metaJavaRuntime.getJavaVMInitArgs();
		throw new DataUnavailable();
	}

	public Iterator<JavaMonitor> getMonitors() {
		return monitors.iterator();
	}

	public Iterator<? extends JavaThread> getThreads() {
		return threads.values().iterator();
	}

	public Object getTraceBuffer(String arg0, boolean arg1)
			throws CorruptDataException {
		throw new CorruptDataException(new PHDCorruptData("No trace data", null));
	}

	public String getFullVersion() throws CorruptDataException {
		return full_version;
	}

	public String getVersion() throws CorruptDataException {
		if (metaJavaRuntime != null) {
			//try {
				return metaJavaRuntime.getVersion();
			//} catch (CorruptDataException e) {				
			//}
		}
		return getFullVersion();
	}

	public Iterator<JavaReference> getHeapRoots() {
		return Collections.<JavaReference>emptyList().iterator();
	}

	public JavaObject getObjectAtAddress(ImagePointer address)
			throws CorruptDataException, IllegalArgumentException,
			MemoryAccessException, DataUnavailable {
		// Is it a class object?
		final long addr = address.getAddress();
		JavaClass cls = findClass(addr);
		JavaObject jo;
		if (cls != null) {
			jo = cls.getObject();
			if (jo != null && address.equals(jo.getID())) return jo;
		}
		if ((jo = extraObjectsCache.get(addr)) != null) {
			return jo;
		} else {
			// See if we already have this object
			for (PHDJavaHeap heap: heaps) {
				try {
					jo = heap.getCachedObjectAtAddress(address, false);
				} catch (IOException e) {
					throw new DataUnavailable("The requested object could not be read from the PHD file");
				}
				if (jo != null) return jo;
			}
			// Return a place holder object, may return corrupt data later
			jo = new PHDJavaObject.Builder(heaps.get(0),addr,null,PHDJavaObject.NO_HASHCODE,-1).build();
		}
		return jo;
	}

	private void initClassCache() {
		for (PHDJavaClassLoader ldr : loaders.values()) {
			for (Iterator<JavaClass> it = ldr.getDefinedClasses(); it.hasNext(); ) {
				JavaClass cls = it.next();
				ImagePointer ip = cls.getID();
				if (ip != null) {
					classIdCache.put(ip.getAddress(), cls);
				} else {
					// Some classes have pseudo ids
					for (long l = 1; l <= lastDummyClassAddr(); ++l) {
						if (cls.equals(ldr.findClass(l))) {
							classIdCache.put(l, cls);
						}
					}
				}
			}
		}
	}
	
	JavaClass findClass(long id) {
		JavaClass cls;
		if (classIdCache.isEmpty()) {
			cls = findClassUncached(id);
		} else {
			cls = classIdCache.get(id);
		}
		return cls;
	}
	
	private JavaClass findClassUncached(long id) {
		for (PHDJavaClassLoader ldr : loaders.values()) {
			JavaClass cls = ldr.findClass(id);
			if (cls != null) return cls;
		}
		return null;
	}
	
	JavaClass findArrayOfClass(long id) {
		JavaClass cl1 = findClass(id);
		if (cl1 != null) {
			try {
				// Try the class loader of the component type first
				JavaClassLoader jcl = cl1.getClassLoader();
				if (jcl instanceof PHDJavaClassLoader) {
					PHDJavaClassLoader ldr = (PHDJavaClassLoader)jcl;
					JavaClass cls = ldr.findArrayOfClass(id);
					if (cls != null) {
						return cls;
					}
				}
			} catch (CorruptDataException e) {
			}
		}
		return findArrayOfClass2(id);
	}
		
	private JavaClass findArrayOfClass2(long id) {
		for (PHDJavaClassLoader ldr : loaders.values()) {
			JavaClass cls = ldr.findArrayOfClass(id);
			if (cls != null) return cls;
		}
		return null;
	}
	
	JavaClass findArrayOfType(int id) {
		return arrayClasses[id];
	}
	

	/**
	 * Allow for different versions of PHD.
	 * Some versions of PHD store the array type as the first reference. If so, then remove and use it.
	 * Other versions might just give the type of the element as the array class. If so, then convert it
	 * to a true array class. 
	 * @param id
	 * @param refs
	 * @param adjustLen 1 if the first reference should be the array type
	 * @return
	 * @throws CorruptDataException
	 */
	JavaClass arrayOf(long id, long[] refs, int adjustLen) throws CorruptDataException {
		JavaClass jcl = adjustLen == 1 ? refs.length > 0 ? findClass(refs[0]) : null : findArrayOfClass(id);
		return jcl;
	}

	/**
	 * Allow for different versions of PHD.
	 * Some versions of PHD store the array type as the first reference. If so, then remove and use it.
	 * Other versions might just give the type of the element as the array class. If so, then convert it
	 * to a true array class.
	 * @param id
	 * @param refs
	 * @param adjustLen 1 if the first reference should be the array type
	 * @return
	 * @throws CorruptDataException
	 */
	JavaClass arrayOf(long id, LongEnumeration refs, int adjustLen) throws CorruptDataException {
		JavaClass jcl = adjustLen == 1 ? refs.hasMoreElements() ? findClass(refs.nextLong()) : null : findArrayOfClass(id);
		return jcl;
	}	
	
	JavaClass findClass(String clsName) {
		JavaClass cls = classNameCache.get(clsName);
		if (cls == null) {
			cls = findClassUncached(clsName);
			if (cls != null) {
				classNameCache.put(clsName, cls);
			}
		}
		return cls;
	}
	
	JavaClass findClassUncached(String clsName) {
		for (PHDJavaClassLoader ldr : loaders.values()) {
			try {
				JavaClass cls = ldr.findClass(clsName);
				if (cls != null) return cls;
			} catch (CorruptDataException e) {
			}
		}
		return null;
	}
	
	/**
	 * Helper
	 * Create a copy of the enumeration, & try to save space 
	 * @param refs
	 * @param skipped how many references to ignore - as the first might be the array type
	 * @return either a long array of the refs or a int array of compressed refs
	 */
	Object convertRefs(LongEnumeration refs, int skipped) {
		int nrefs = refs.numberOfElements() - skipped;
		int ri[] = noCompress() ? null : new int[nrefs];
		long rl[] = noCompress() ? new long[nrefs] : null;
		for (int i = 0; i < nrefs; ++i) {
			long l = refs.nextLong();
			if (ri != null) {
				ri[i] = compressAddress(l);
				if (expandAddress(ri[i]) != l) {
					// Doesn't fit, use a long array 
					rl = new long[ri.length];
					for (int j = 0; j < i; ++j) {
						rl[j] = expandAddress(ri[j]);
					}
					rl[i] = l;
					ri = null;
				}
			} else {
				rl[i] = l;
			}
		}
		if (ri != null) {
			return ri;
		} else {
			return rl;
		}
	}
	
	/**
	 * Helper
	 * Create a copy of the long array & try to save space 
	 * @param refs
	 * @param skipped How many longs to skip at the beginning
	 * @return either a long array of the refs or a int array of compressed refs
	 */
	Object convertRefs(long refs[], int skipped) {
		int nrefs = refs.length - skipped;
		if (nrefs <= 0) 
			return refs;
		int ri[] = noCompress() ? null : new int[nrefs];
		long rl[] = noCompress() ? new long[nrefs] : null;
		for (int i = 0; i < nrefs; ++i) {
			long l = refs[skipped+i];
			if (ri != null) {
				ri[i] = compressAddress(l);
				if (expandAddress(ri[i]) != l) {
					// Doesn't fit, use a long array
					// If no offset, then use the original array
					// if (skipped == 0) return refs;
					rl = new long[ri.length];
					for (int j = 0; j < i; ++j) {
						rl[j] = expandAddress(ri[j]);
					}
					rl[i] = l;
					ri = null;
				}
			} else {
				rl[i] = l;
			}
		}
		if (ri != null) {
			return ri;
		} else {
			return rl;
		}
	}
	
	/** 
	 * If max and min or index bases haven't yet been initialized then we cannot compress/expand
	 * as the base will change later, invalidating the compression.
	 * @return
	 */
	boolean noCompress() {
		return compressIndexBase1 == 0;
	}
	
	/**
	 * If the address is in the range [compressAddressBase1, compressAddressTop1] return an
	 * index based on that range, otherwise return a bigger index based on compressAddressBase2
	 * and compressIndexBase2. 
	 * @param addr
	 * @return
	 */
	int compressAddress(long addr) {
		int ret;
		if (addr <= compressAddressTop1)
			ret = (int)((addr - compressAddressBase1 >>> compressShift) + compressIndexBase1);
		else
			ret = (int)((addr - compressAddressBase2 >>> compressShift) + compressIndexBase2);			
		//System.out.println("Compress "+Long.toHexString(addr)+" "+Integer.toHexString(ret)+" "+Long.toHexString(compressAddressBase1)+" "+Long.toHexString(compressAddressBase2)+" "+compressIndexBase2+" "+compressShift);
		return ret;
	}
	
	/**
	 * If the index is < compressIndexBase2, convert index to address based on compressAddressBase1,
	 * otherwise convert based on compressAddressBase2.
	 * @param ad
	 * @return
	 */
	long expandAddress(int ad) {
		long ret;
		if (ad < compressIndexBase2)
			ret = ((long)ad - compressIndexBase1 << compressShift) + compressAddressBase1;
		else
			ret = ((long)ad - compressIndexBase2 << compressShift) + compressAddressBase2;
		//System.out.println("Expand "+Integer.toHexString(ad)+" "+Long.toHexString(ret)+" "+compressShift);
		return ret;
	}
	
	boolean is64Bit() {
		return process.getPointerSize() == 64;
	}
	
	/**
	 * Calculate the size of pointers
	 * If a 32-bit dump then a pointer is 32-bits (4 bytes)
	 * If a 64-bit dump then if the smallest object is < 2 * 8 then we have compressed
	 * refs and 32-bit pointers, otherwise 64-bit pointers.
	 * minInstanceSize == 0 occurs on 1.4.2 without compressed refs
	 * @return in bytes
	 */
	int pointerSize() {
		return is64Bit() && (minInstanceSize == 0 || minInstanceSize > 16) ? 8 : 4;
	}
	
	
	/**
	 * Helper method to try to allocate classes to the correct class loader
	 * There isn't an explicit mention of the loader, but an outbound reference might be to the loader.
	 * First find all the objects which are class loaders.
	 * The look at each file, see if it has a reference to a loader object and if so allocate it to
	 * that loader.
	 * @param file
	 * @throws IOException 
	 */
	private void findLoaders(HeapdumpReader newreader) throws IOException {
		// Where all the classes originally have been put
		final PHDJavaClassLoader boot = loaders.get(null);
		
		// Might fail to find a class with a very corrupt dump
		final JavaClass jlc = findClass("java/lang/Class");
		final long jlcAddress = jlc == null || jlc.getID() == null ? 0 : jlc.getID().getAddress();
		// Find all the class loader classes
		final JavaClass jcl = findClass("java/lang/ClassLoader");
		final HashMap<Long,JavaClass>classLoaderClasses = new HashMap<Long,JavaClass>();
		for (Iterator<JavaClass> it = boot.getDefinedClasses(); it.hasNext();) {
			JavaClass cls = it.next();
			if (cls instanceof CorruptData) continue;
			try {
				// Avoid bug with superclass loops by remembering superclasses
				// PHD Version 4 bug - bad superclass: J2RE 5.0 IBM J9 2.3 AIX ppc64-64 build 20080314_17962_BHdSMr
				HashSet<JavaClass> supers = new HashSet<JavaClass>();
				for (JavaClass j1 = cls; j1 != null && supers.add(j1); j1 = j1.getSuperclass()) {
					/* 
					 * See if either a superclass is java.lang.ClassLoader
					 * or if no superclass information is available (old Java 5)
					 * whether the name ends with "ClassLoader"
					 */ 
					if (j1.equals(jcl) || cls.getSuperclass() == null && !j1.isArray() && j1.getName().endsWith("ClassLoader")) {
						ImagePointer ip = cls.getID();
						if (ip != null) {
							classLoaderClasses.put(ip.getAddress(), cls);
						}
					}
				}
			} catch (CorruptDataException e) {
				// Ignore
			}
		}
		
		// Number of class objects not found at class addresses
		final int onHeapClasses[] = new int[1];
		// Find all the objects which are class loaders
		final PHDJavaHeap heap = heaps.get(0);
		final HashMap<Long, JavaObject> classObjects = new HashMap<Long, JavaObject>();
//		HeapdumpReader newreader = new HeapdumpReader(file, parentImage);
		final int adjustLen = newreader.version() == 4 && newreader.isJ9() ? 1 : 0;
		try {
			newreader.parse(new PortableHeapDumpListener() {

				public void classDump(long address, long superAddress, String name, int size,
						int flags, int hashCode, LongEnumeration refs) throws Exception {
				}

				public void objectArrayDump(long address, long classAddress, int flags,
						int hashCode, LongEnumeration refs, int length, long instanceSize) throws Exception {
					if (extraObjectsCache.containsKey(address)) {
						// Don't bother saving reference information - we can get it later
						JavaObject jo = new PHDJavaObject.Builder(heap,address,arrayOf(classAddress, refs, adjustLen),flags,hashCode)
						.length(length-adjustLen).instanceSize(instanceSize).build();
						extraObjectsCache.put(address, jo);						
					}
				}

				public void objectDump(long address, long classAddress, int flags, 
						int hashCode, LongEnumeration refs, long instanceSize) throws Exception {
					JavaClass cls = classLoaderClasses.get(classAddress);
					JavaObject jo;
					if (cls != null) {
						// Object of type java.lang.ClassLoader, so create the object and the class loader
						jo = new PHDJavaObject.Builder(heap,address,cls,flags,hashCode)
							.refs(refs, 0).length(PHDJavaObject.SIMPLE_OBJECT)
							.instanceSize(instanceSize).build();
						PHDJavaClassLoader load = new PHDJavaClassLoader(jo);
						loaders.put(jo,load);
					} else if (classAddress == jlcAddress) {
						if (boot.findClass(address) == null) {
							++onHeapClasses[0];
						}
						jo = new PHDJavaObject.Builder(heap,address,jlc,flags,hashCode)
						.refs(refs, 0).length(PHDJavaObject.SIMPLE_OBJECT)
						.instanceSize(instanceSize).build();
						classObjects.put(address, jo);
					} else {
						jo = null;
					}
					if (extraObjectsCache.containsKey(address)) {
						if (jo == null) {
							jo = new PHDJavaObject.Builder(heap,address,findClass(classAddress),flags,hashCode)
							.refs(refs, 0).length(PHDJavaObject.SIMPLE_OBJECT).build();
						}
						extraObjectsCache.put(address, jo);
					}
				}

				public void primitiveArrayDump(long address, int type, int length, int flags, 
						int hashCode, long instanceSize) throws Exception {
					if (extraObjectsCache.containsKey(address)) {
						// Create a full object as we have the data
						JavaObject jo = new PHDJavaObject.Builder(heap,address,findArrayOfType(type),flags,hashCode)
						.refsAsArray(NOREFS,0).length(length).instanceSize(instanceSize).build();
						extraObjectsCache.put(address, jo);						
					}
				}
			});
		} catch (Exception e) {
			// Ignore the exception - we will have seen it elsewhere
			//e.printStackTrace();
		} finally {
			newreader.close();
			newreader = null;
		}
		
		// Assign classes to the correct loaders
		// Also try to set up on/off-heap class addresses
		PHDJavaClassLoader boot2 = null;
		int foundLoader = 0;
		int notFoundLoader = 0;
		// How many java/lang classes the possible boot loader has loaded
		int loaderJavaLangCount = 0;
		boolean useFirstObjectRefAsLoader = onHeapClasses[0] == 0;
		for (Iterator<JavaClass> it = boot.getDefinedClasses(); it.hasNext();) {
			JavaClass j1 = it.next();
			PHDJavaClassLoader bestLoader = null;
			for (Iterator<JavaReference> it2 = j1.getReferences(); it2.hasNext(); ) {
				JavaReference jr = it2.next();
				try {
					// Is the first outbound object reference to a class loader?
					if (jr.isObjectReference()) {
						JavaObject jo = (JavaObject)jr.getTarget();
						PHDJavaClassLoader newLoader = loaders.get(jo);
						if (newLoader != null) {
							if (bestLoader == null || !useFirstObjectRefAsLoader) {
								bestLoader = newLoader;
							}
						} else if (onHeapClasses[0] > 0) {
							long addr = jo.getID().getAddress();
							JavaObject jo2 = classObjects.get(addr);
							if (jo2 != null) {
								// For Java 6 jdmpview PHD files the on-heap class object is the last ref
								// retrieve the full JavaObject from walking the heap earlier
								((PHDJavaClass)j1).setJavaObject(jo2);
							}
						}
						// Avoid confusion with non-array classes having static fields holding class loaders.
						// Presume that only the first reference could be the class loader,
						// unless using off-heap classes when it is the last reference.
						if (!j1.isArray() && useFirstObjectRefAsLoader && onHeapClasses[0] == 0) break;
					}
				} catch (CorruptDataException e) {
					//e.printStackTrace();
				} catch (DataUnavailable e) {
					//e.printStackTrace();
				}
			}
			if (bestLoader != null) {
				++foundLoader;
				// Don't remove the classes from the original loader, nor change the loader
				// as otherwise finding the array type fails
				bestLoader.prepareToMove(boot, j1);
				// Is the class by any chance the type of the class loader?
				try {
					if (boot2 == null && (j1.equals(jlc) || j1.equals(bestLoader.getObject().getJavaClass()))) {
						// We have found the new bootstrap class loader
						// Beware java 1.4.2 com/ibm/rmi/util/ClassInfo$NULL_CL_CLASS passes this test!
						boot2 = bestLoader;
					}
					if (boot2 == bestLoader && j1.getName().startsWith("java/lang/")) ++loaderJavaLangCount;
				} catch (CorruptDataException e) {
				}
			} else {
				++notFoundLoader;
			}
			// Try retrieving the full JavaObject for the JavaClass
			try {
				JavaObject jo = j1.getObject();
				if (jo != null) {
					long addr = jo.getID().getAddress();
					JavaObject jo2 = classObjects.get(addr);
					if (jo2 != null) {
						((PHDJavaClass)j1).setJavaObject(jo2);
					}
				}
			} catch (CorruptDataException e) {
			}
		}
		
		// Ignore a bootstrap loader which hasn't loaded 5 java/lang classes
		if (loaderJavaLangCount < 5) boot2 = null;
		
		// Haven't found any loaders, but have a javacore file with loader information
		if (metaJavaRuntime != null) {
			for (Iterator i = metaJavaRuntime.getJavaClassLoaders(); i.hasNext(); ) {
				Object next = i.next();
				if (next instanceof CorruptData) continue;
				JavaClassLoader jcl2 = (JavaClassLoader)next;
				try {
					JavaObject lo = jcl2.getObject();
					if (lo != null) {
						ImagePointer addr = lo.getID();
						if (addr != null) {
							ImagePointer ip = space.getPointer(addr.getAddress());
							JavaObject jo = getObjectAtAddress(ip);
							PHDJavaClassLoader newLoader = loaders.get(jo);
							JavaClass loaderClass;
							if (newLoader == null) {
								try {
									// Should be safe to find the class of 'jo' without rereading the PHD file
									// as at least a dummy object should be in the extra objects cache.
									// It could be that the object is still a dummy one with no proper class.
									loaderClass = jo.getJavaClass();
								} catch (CorruptDataException e) {
									loaderClass = null;
								}
								JavaClass javacoreLoaderClass;
								try {
									javacoreLoaderClass = lo.getJavaClass();
								} catch (CorruptDataException e) {
									javacoreLoaderClass = null;
								}
								// Do the types match between javacore and PHD?
								// Mismatch occurs with J2RE 5.0 IBM J9 2.3 Linux amd64-64 build j9vmxa6423-20091104
								if (loaderClass != null && javacoreLoaderClass != null &&
									(loaderClass.isArray() ||
									 loaderClass.getID() != null && javacoreLoaderClass.getID() != null &&
									 loaderClass.getID().getAddress() != javacoreLoaderClass.getID().getAddress())) {
									//System.out.println("Skipping loader "+newLoader+" "+jo+" "+jo.getJavaClass()+" "+lo+" "+lo.getJavaClass()+" "+Long.toHexString(addr.getAddress())+" "+ip);
								}
								else
								{
									// The object should have been listed in the extra objects, so may now be the proper object
									newLoader = new PHDJavaClassLoader(jo);
									loaders.put(jo, newLoader);
								}
							} else {
								// Replace with the official object
								jo = newLoader.getObject();
								loaderClass = jo.getJavaClass();
							}
							if (newLoader != null) {
								for (Iterator i2 = jcl2.getDefinedClasses(); i2.hasNext(); ) {
									Object next2 = i2.next();
									if (next2 instanceof CorruptData) continue;
									JavaClass jc2 = (JavaClass)next2;
									ImagePointer ip2 = jc2.getID();
									JavaClass j1;
									if (ip2 != null) {
										long claddr = ip2.getAddress();
										j1 = boot.findClass(claddr);
										// Not found by address, so try by name.
										if (j1 == null) {
											// But only if it is the only class of that name
											j1 = boot.findClassUnique(jc2.getName());
										} else {
											// Found by address
											try {
												j1.getName();
											} catch (CorruptDataException e) {
												// Our class doesn't have a name, so perhaps the javacore has the name
												try {
													String actualName = jc2.getName();
													PHDJavaClass pj1 = (PHDJavaClass)j1;
													// We will need to reindex the classloader as the name as changed
													pj1.setName(actualName);
												} catch (CorruptDataException e2) {
												}
											}
										}
									} else {
										// But only if it is the only class of that name
										j1 = boot.findClassUnique(jc2.getName());
									}
									if (j1 != null) {
										newLoader.prepareToMove(boot, j1);
										// Is the class by any chance the type of the class loader?
										// or that of java.lang.Class, which avoids the problem of 1.4.2 javacore
										// files not having the bootloader type (*System*) in a loader. Moving everything
										// listed in javacore causes problems as byte etc. aren't listed
										if (j1.equals(loaderClass) || j1.equals(jlc)) {
											// We have found the new bootstrap class loader
											boot2 = newLoader;
										}
										for (Iterator i3 = jc2.getDeclaredMethods(); i3.hasNext(); ) {
											Object next3 = i3.next();
											if (next3 instanceof CorruptData) continue;
											JavaMethod jm = (JavaMethod)next3;
											PHDJavaClass pj1 = (PHDJavaClass)j1;
											pj1.addMethod(new PHDJavaMethod(space, pj1, jm));
										}
									}
								}
							}
						}
					}
				} catch (CorruptDataException e) {
				} catch (DataUnavailable e) {
				} catch (MemoryAccessException e) {	
				}
			}
		}
	
		// Move the classes to the correct class loaders
		for (Iterator<JavaClass> it = boot.getDefinedClasses(); it.hasNext();) {
			JavaClass j1 = it.next();
			try {
				JavaClassLoader jcl2 = j1.getClassLoader();
				if (!boot.equals(jcl2) && jcl2 instanceof PHDJavaClassLoader) {
					transferClass(boot, (PHDJavaClassLoader)jcl2, j1);
				}
			} catch (CorruptDataException e) {
			}
		}
		
		// Reindex the loaders to account for the removed classes
		for (PHDJavaClassLoader loader : loaders.values()) {
			loader.initCache();				
		}
				
		if (boot2 != null) {
			// Move remaining classes to new boot loader
			for (Iterator<JavaClass> it = boot.getDefinedClasses(); it.hasNext();) {
				JavaClass j1 = it.next();
				boot2.prepareToMove(boot, j1);
				transferClass(boot, boot2, j1);
			}
			// index the new boot loader to account for the added files
			boot2.initCache();
			// Remove the original boot class loader as it has no classes
			loaders.remove(null);
		} else {
			// There may be duplicate array classes in the boot loader 
			for (Iterator<JavaClass> it = boot.getDefinedClasses(); it.hasNext();) {
				JavaClass j1 = it.next();
				JavaClass j2 = boot.setArrayType(this, boot, j1);
			}
			// index the boot loader to account for the added files
			boot.initCache();
		}		
	}

	private void transferClass(final PHDJavaClassLoader from,
			PHDJavaClassLoader to, JavaClass j1) {
		to.move(from, j1, lastDummyClassAddr());
		// We have moved the class - also see if this is the type of an array object,
		// and so if the actual array type needs to be found now.
		JavaClass j2 = to.setArrayType(this, from, j1);
		if (j2 != null) {
			// The array type was found in the boot loader, so move it now.
			to.prepareToMove(from, j2);
			to.move(from, j2, lastDummyClassAddr());
		}
	}
	
	/**
	 * If the object is a class loader, return an iterator over all the classes it loaded.
	 * @param jo
	 * @return
	 */
	Iterator<JavaClass> getLoaderClasses(JavaObject jo) {
		PHDJavaClassLoader load = loaders.get(jo);
		if (load != null) return load.getDefinedClasses(); else return Collections.<JavaClass>emptyList().iterator();
	}
	
	/**
	 * Get a JavaThread corresponding to the metafile Javathread.
	 * If it doesn't exist, then create one.
	 * @param th
	 * @return
	 */
	JavaThread getThread(JavaThread th) {
		if (!threads.containsKey(th)) {
			threads.put(th, new PHDJavaThread(space, process, this, th));
		}
		return threads.get(th);
	}
	
	/**
	 * Remember objects associated with threads.
	 * Performance optimization - we can then find these objects on a scan through the heap
	 * and remember them, saving a fruitless search of the heap if they only exist in a javacore.
	 */
	private void prepThreads() {
		if (metaJavaRuntime != null) {
			final PHDJavaClassLoader boot = loaders.get(null);
			for (Iterator it = metaJavaRuntime.getThreads(); it.hasNext(); ) {
				Object next = it.next();
				if (next instanceof CorruptData) continue;
				JavaThread thr = (JavaThread)next;
				try {
					JavaObject jo = thr.getObject();
					saveExtraObject(boot, jo);
				} catch (CorruptDataException e) {
				}
			}
		}
	}

	private void saveExtraObject(final PHDJavaClassLoader boot, JavaObject jo) {
		if (jo != null) {
			long addr = jo.getID().getAddress();
			JavaClass cls;
			try {
				cls = boot.findClassUnique(jo.getJavaClass().getName());
			} catch (CorruptDataException e) {
				cls = null;
			}
			// Construct a dummy object with little information in case the object is not in the heap
			JavaObject jo2 = new PHDJavaObject.Builder(heaps.get(0),addr,cls,PHDJavaObject.NO_HASHCODE,-1)
			.refsAsArray(NOREFS,0).length(PHDJavaObject.UNKNOWN_TYPE).build();
			extraObjectsCache.put(addr, jo2);
		}
	}
	
	private void initThreads() {
		if (metaJavaRuntime != null) {
			for (Iterator it = metaJavaRuntime.getThreads(); it.hasNext(); ) {
				Object next = it.next();
				if (next instanceof CorruptData) {
					JavaThread thr = new PHDCorruptJavaThread(space, (CorruptData)next);
					threads.put(thr, thr);
				} else {
					getThread((JavaThread)next);
				}
			}
		}
	}
	
	/**
	 * Remember objects associated with monitors.
	 * Performance optimization - we can then find these objects on a scan through the heap
	 * and remember them, saving a fruitless search of the heap if they only exist in a javacore.
	 */
	private void prepMonitors() {
		if (metaJavaRuntime != null) {
			final PHDJavaClassLoader boot = loaders.get(null);
			for (Iterator it = metaJavaRuntime.getMonitors(); it.hasNext(); ) {
				Object next = it.next();
				if (next instanceof CorruptData) continue;
				JavaMonitor mon = (JavaMonitor)next;
				JavaObject jo = mon.getObject();
				saveExtraObject(boot, jo);
			}
		}
	}
	
	private void initMonitors() {
		if (metaJavaRuntime != null) {
			for (Iterator it = metaJavaRuntime.getMonitors(); it.hasNext(); ) {
				Object next = it.next();
				if (next instanceof CorruptData) {
					monitors.add(new PHDCorruptJavaMonitor(space, (CorruptData)next));
				} else {
					monitors.add(new PHDJavaMonitor(space, this, (JavaMonitor)next));
				}
			}
		}
	}
	
	/**
	 * Remember objects associated with class loaders
	 * Performance optimization - we can then find these objects on a scan through the heap
	 * and remember them, saving a fruitless search of the heap if they only exist in a javacore.
	 */
	private void prepClassLoaders() {
		if (metaJavaRuntime != null) {
			final PHDJavaClassLoader boot = loaders.get(null);
			for (Iterator it = metaJavaRuntime.getJavaClassLoaders(); it.hasNext(); ) {
				Object next = it.next();
				if (next instanceof CorruptData) continue;
				JavaClassLoader load = (JavaClassLoader)next;
				try {
					JavaObject jo = load.getObject();
					saveExtraObject(boot, jo);
				} catch (CorruptDataException e) {
				}
			}
		}
	}
	
	long nextDummyClassAddr() {
		return ++nextClsAddr;
	}
	
	long lastDummyClassAddr() {
		return nextClsAddr;
	}

	public Iterator getMemoryCategories() throws DataUnavailable
	{
		throw new DataUnavailable("This implementation of DTFJ does not support getMemoryCategories");
	}

	public Iterator getMemorySections(boolean includeFreed)
			throws DataUnavailable
	{
		throw new DataUnavailable("This implementation of DTFJ does not support getMemorySections");
	}

	public boolean isJITEnabled() throws DataUnavailable, CorruptDataException {
		throw new DataUnavailable("This implementation of DTFJ does not support isJITEnabled");
	}

	public Properties getJITProperties() throws DataUnavailable,	CorruptDataException {
		throw new DataUnavailable("This implementation of DTFJ does not support getJITProperies");
	}

	public long getStartTime() throws DataUnavailable, CorruptDataException {
		// Not supported in DTFJ for PHD dumps
		throw new DataUnavailable();
	}

	public long getStartTimeNanos() throws DataUnavailable, CorruptDataException {
		// Not supported in DTFJ for PHD dumps
		throw new DataUnavailable();
	}
}
