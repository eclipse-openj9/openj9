/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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

import java.io.EOFException;
import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.phd.parser.HeapdumpReader;
import com.ibm.dtfj.phd.parser.PortableHeapDumpListener;
import com.ibm.dtfj.phd.util.LongEnumeration;

/** 
 * @author ajohnson
 */
class PHDJavaClassLoader implements JavaClassLoader {

	/** Ordinary classes and array classes found in dump, with real addresses */
	private final Map<Long,JavaClass> classes = new LinkedHashMap<Long,JavaClass>();
	/** Mapping from array type in dump to whole array class, also holds created array classes without addresses
	 * which will not be in the id to classes map */
	private final Map<Long,JavaClass> arrayClasses = new LinkedHashMap<Long,JavaClass>();
	long maxAddress = Long.MIN_VALUE;
	long minAddress = Long.MAX_VALUE;
	long maxObjClass;
	int maxObjLen;
	long bitsAddress = 0L;
	long maxClassAddress = Long.MIN_VALUE;
	long minClassAddress = Long.MAX_VALUE;
	int minInstanceSize = Integer.MAX_VALUE;
	private final HashMap<String,JavaClass>classNameCache = new LinkedHashMap<String,JavaClass>();
	private final LinkedHashSet<JavaClass> allClasses = new LinkedHashSet<JavaClass>();
	private final HashSet<String> duplicateClassNames = new HashSet<String>();
	private JavaObject obj;
	private long jlo;

	PHDJavaClassLoader(ImageInputStream stream, final PHDImage parentImage, final ImageAddressSpace space, final PHDJavaRuntime runtime) throws IOException {
		HeapdumpReader reader = new HeapdumpReader(stream, parentImage);
		processData(reader, parentImage, space, runtime);
	}
	
	PHDJavaClassLoader(File file, final PHDImage parentImage, final ImageAddressSpace space, final PHDJavaRuntime runtime) throws IOException {
		HeapdumpReader reader = new HeapdumpReader(file, parentImage);
		processData(reader, parentImage, space, runtime);
	}
	
	private void processData(HeapdumpReader reader, final PHDImage parentImage, final ImageAddressSpace space, final PHDJavaRuntime runtime) throws IOException {
		final JavaClassLoader loader = this;
		final int adjustLen = reader.version() == 4 && reader.isJ9() ? 1 : 0;
		final int realClasses[] = new int[1];
		try {
			reader.parse(new PortableHeapDumpListener() {
				long prevAddress;
				PHDJavaClass prevObjClass;
				private void updateSizes(long address) {
					if (prevObjClass != null) prevObjClass.updateSize(prevAddress, address);
					prevObjClass = null;
					prevAddress = address;
				}

				public void classDump(long address, long superAddress, String name, int size,
						int flags, int hashCode, LongEnumeration refs) throws Exception {
					updateSizes(address);
					classes.put(address, 
							new PHDJavaClass.Builder(space,runtime,loader, address, superAddress, name)
					.size(size).flags(flags).hashCode(hashCode).refs(refs).build());
					updateAddresses(address, size, name);
					++realClasses[0];
				}

				public void objectArrayDump(long address, long classAddress, int flags,
						int hashCode, LongEnumeration refs, int length, long instanceSize) throws Exception {
					int refsLen = refs.numberOfElements();
					int adjustLen2 = Math.min(adjustLen, refsLen);
					updateSizes(address);
					updateAddresses(address, classAddress, length - adjustLen2);
					long classAddress2 = adjustLen2 == 1 ? refs.nextLong() : 0;
					genArrayClasses(space, runtime, loader, adjustLen2,
							classAddress, classAddress2);
				}

				public void objectDump(long address, long classAddress, int flags, 
						int hashCode, LongEnumeration refs, long instanceSize) throws Exception {
					updateSizes(address);
					updateAddresses(address, classAddress, -1);
					genObjectClass(space, runtime, loader, classAddress,
							hashCode);
				}

				public void primitiveArrayDump(long address, int type, int length, int flags, 
						int hashCode, long instanceSize) throws Exception {
					updateSizes(address);
					updateAddresses(address, type, length);
				}

				private void genObjectClass(final ImageAddressSpace space,
						final PHDJavaRuntime runtime, final JavaClassLoader loader,
						long classAddress, int hashCode) {
					if (!classes.containsKey(classAddress)) {
						PHDJavaClass objClass = new PHDJavaClass.Builder(space,runtime,loader, classAddress, 
								PHDJavaClass.UNKNOWN_SUPERCLASS, PHDJavaClass.UNKNOWN_NONARRAY).build();
						classes.put(classAddress, objClass);
						updateAddresses(classAddress, 100, null);
					}
					prevObjClass = (PHDJavaClass)findClass(classAddress);
				}
				
				private PHDJavaClass genArrayClass(final ImageAddressSpace space,
						final PHDJavaRuntime runtime, final JavaClassLoader loader,
						long classAddress, long sup, String name) {
					PHDJavaClass elemCls;
					int size = 100;
					elemCls = new PHDJavaClass.Builder(space,runtime,loader, classAddress, sup, name).build();
					classes.put(classAddress, elemCls);
					updateAddresses(classAddress, size, name);
					return elemCls;
				}

				private void genArrayClasses(final ImageAddressSpace space,
						final PHDJavaRuntime runtime, final JavaClassLoader loader,
						final int adjustLen, long classAddress,
						long classAddress2) {
					if (adjustLen == 1) {
						// Java 5.0 arrays with type = base component type e.g. String
						// First reference = actual array type e.g. [[Ljava.lang.String;
						JavaClass arrayCls;
						if (!classes.containsKey(classAddress2)) {
							String name = PHDJavaClass.UNKNOWN_ARRAY;
							arrayCls = genArrayClass(space, runtime, loader,
									classAddress2, jlo, name);
						} else {
							arrayCls = findClass(classAddress2);
						}
						arrayClasses.put(classAddress, arrayCls);
						
						if (!classes.containsKey(classAddress)) {
							String name = PHDJavaClass.UNKNOWN_NONARRAY;
							genArrayClass(space, runtime, loader,
									classAddress, PHDJavaClass.UNKNOWN_SUPERCLASS, name);
						}
					} else {
						arrayClasses.put(classAddress, null);
					}
					return;
				}

			});
		} catch (EOFException e) {
			classes.put(runtime.nextDummyClassAddr(), new PHDCorruptJavaClass("Truncated dump found building class "+realClasses[0], null, e));
		} catch (IOException e) {
			classes.put(runtime.nextDummyClassAddr(), new PHDCorruptJavaClass("Corrupted dump found building class "+realClasses[0], null, e));			
		} catch (Exception e) {
			classes.put(runtime.nextDummyClassAddr(), new PHDCorruptJavaClass("Building class "+realClasses[0], null, e));
		} finally {
			reader.close();
			reader = null;
		}
		
		jlo = 0;
		// Use an uncached search as the index hasn't been built.
		JavaClass jco = findClassUncached("java/lang/Object");
		if (jco != null) {
			ImagePointer ip = jco.getID();
			if (ip != null) jlo = ip.getAddress();
		} else {
			// If the dump is very corrupt it won't have java.lang.Object - so create one
			jco = new PHDJavaClass.Builder(space,runtime,loader, 0, 0, "java/lang/Object").build();
			jlo = runtime.nextDummyClassAddr();
			classes.put(jlo, jco);
		}
		
		// If the dump is very corrupt it won't have java.lang.Class - so create one
		JavaClass jcl = findClassUncached("java/lang/Class");
		if (jcl == null) {
			jcl = new PHDJavaClass.Builder(space,runtime,loader, 0, jlo, "java/lang/Class").build();
			classes.put(runtime.nextDummyClassAddr(), jcl);
		}
		// nor will it have primitive array classes
		for (int i = 0; i < PHDJavaRuntime.arrayTypeName.length; ++i) {
			JavaClass jc = findClassUncached(PHDJavaRuntime.arrayTypeName[i]);
			if (jc == null) {
				jc = new PHDJavaClass.Builder(space,runtime,loader, 0, jlo, PHDJavaRuntime.arrayTypeName[i]).build();
				classes.put(runtime.nextDummyClassAddr(), jc);
			}
		}
		
		// Bug in PHD dumps - some Java 6 types have the type of an array being the whole array, not the elements
		// Is the type of the array the whole array or the type of an element?
		boolean arrayTypeIsArray = true;
		for (Long id : arrayClasses.keySet()) {
			JavaClass cl1 = findClass(id);
			if (cl1 == null) continue; // Corrupt dump might not have the class!
			try {
				if (!cl1.isArray()) {
					arrayTypeIsArray = false;
				}
			} catch (CorruptDataException e) {
				// If we don't know the type then presume they are not arrays
				arrayTypeIsArray = false;
			}
		}
		
		// Create a mapping from the phd id of an array object to the type of the whole array
		for (Long id : arrayClasses.keySet()) {
			JavaClass cl1 = findClass(id);
			
			if (cl1 == null) {
				String name = arrayTypeIsArray ? PHDJavaClass.UNKNOWN_ARRAY : null;
				long sup = arrayTypeIsArray ? jlo : PHDJavaClass.UNKNOWN_SUPERCLASS;
				cl1 = new PHDJavaClass.Builder(space, runtime, loader, id, sup,	name).build();
				classes.put(id, cl1);
			}
			
			JavaClass ar = arrayClasses.get(id);
			if (ar != null) {
				ImagePointer ip = ar.getID();
				if (ip != null) {
					JavaClass ar2 = findClass(ip.getAddress());
					if (ar2 != null) {
						ar = ar2;
					}
				}
				// cl1 is not a component type for multidimensional arrays, but is the scalar type
				//((PHDJavaClass)ar).setComponentType(cl1);
			}
			
			JavaClass cl2;
			if (arrayTypeIsArray) {
				cl2 = cl1;
			} else if (ar != null) {
				cl2 = ar;
			} else {
				try {
					String name = cl1.getName();
					String arrayName = arrayName(name);
					Set<JavaClass> s = findClasses(arrayName);
					if (s.size() == 0) {
						// Array class doesn't exist, so create a dummy one
						cl2 = new PHDJavaClass.Builder(space, runtime, loader, 0, jlo, arrayName).componentType(cl1).build();
					} else if (s.size() == 1) {
						// Only one, so use it
						cl2 = s.iterator().next();
					} else {
						// Multiple classes, so if possible choose one
						cl2 = null;
						for (JavaClass cl3 : s) {
							// Does the array class have a reference to the
							// component type. E.g. Java 5.0
							// If so then it is the one.
							if (PHDJavaClass.referencesClass(cl3, cl1)) {
								cl2 = cl3;
								break;
							}
						}
					}
				} catch (CorruptDataException e) {
					// Array class doesn't exist, so create a dummy one
					cl2 = new PHDJavaClass.Builder(space, runtime, loader, 0, jlo, PHDJavaClass.UNKNOWN_ARRAY).componentType(cl1).build();
				}
				if (cl2 instanceof PHDJavaClass) {
					// We know the component type, so remember it for later
					((PHDJavaClass)cl2).setComponentType(cl1);
				}
			}
			// Even if the array class is null (we couldn't decide), mark it so we know for later
			arrayClasses.put(id, cl2);
		}		
		initCache();
	}

	/**
	 * The class has been moved from one loader to this one.
	 * Does the array type need to be set up? It could be that there was a choice of arrays with the same name
	 * but now the arrays are in different class loaders there is only one to choose.
	 * @param runtime
	 * @param from
	 * @param cl1
	 * @return
	 */
	JavaClass setArrayType(PHDJavaRuntime runtime, PHDJavaClassLoader from, JavaClass cl1) {
		boolean lookInBootLoader = false;
		JavaClass cl2 = null;
		ImagePointer ip = cl1.getID();
		if (ip != null) {
			long addr = ip.getAddress();
			if (arrayClasses.containsKey(addr)) {
				cl2 = arrayClasses.get(addr);
				if (cl2 == null) {
					// Need an array type, but it hasn't yet been found
					try {
						String name = cl1.getName();
						String arrayName = arrayName(name);
						Set<JavaClass> s = findClasses(arrayName);

						// Does the array class exist in this loader?
						lookInBootLoader = s.size() == 0;
						if (lookInBootLoader) {
							// Try the boot loader instead
							s = from.findClasses(arrayName);
						}

						if (s.size() == 0) {
							// Array class doesn't exist, so create a dummy one.
							// Unlikely to happen as a dummy array has been built earlier.
							// May happen if the class is loader by multiple loaders, but only some loaders have the array
							cl2 = new PHDJavaClass.Builder(cl1.getID().getAddressSpace(),runtime,this, 0, jlo, arrayName).componentType(cl1).build();
						} else if (s.size() == 1) {
							// Only one class, so use it
							cl2 = s.iterator().next();
						} else {
							for (JavaClass cl3 : s) {
								// Default - first class found
								if (cl2 == null) cl2 = cl3;
								// See if this class is unused as an array type
								if (cl3 instanceof PHDJavaClass) {
									// Choose one array class not already set as an array of another class
									if (((PHDJavaClass)cl3).setComponentType(null) == null) {
										cl2 = cl3;
										break;
									}
								}
							}
						}
					} catch (CorruptDataException e) {
						cl2 = new PHDJavaClass.Builder(cl1.getID().getAddressSpace(),runtime,this, 0, jlo, PHDJavaClass.UNKNOWN_ARRAY).componentType(cl1).build();
					}
					if (cl2 instanceof PHDJavaClass) {
						// We know the component type, so remember it for later
						JavaClass c1 = ((PHDJavaClass)cl2).setComponentType(cl1);
					}
					arrayClasses.put(addr, cl2);
				}
			}
		}
		// If the class was found in the boot loader, return it so that it can be removed.
		return lookInBootLoader ? cl2 : null;
	}

	/**
	 * Get the name of an array given the name of the component type.
	 * @param name
	 * @return
	 */
	private String arrayName(String name) {
		String arrayName;
		if (name.startsWith("[")) {
			arrayName = "["+name;
		} else if (name.equals("byte")) {
			arrayName = "[B";
		} else if (name.equals("short")) {
			arrayName = "[S";
		} else if (name.equals("int")) {
			arrayName = "[I";
		} else if (name.equals("long")) {
			arrayName = "[J";
		} else if (name.equals("boolean")) {
			arrayName = "[Z";
		} else if (name.equals("char")) {
			arrayName = "[C";
		} else if (name.equals("float")) {
			arrayName = "[F";
		} else if (name.equals("double")) {
			arrayName = "[D";					
		} else {
			arrayName = "[L"+name+";";
		}
		return arrayName;
	}
	
	PHDJavaClassLoader(JavaObject obj) {
		this.obj = obj;
	}

	/**
	 * Find a class via the name. If there are duplicates return the first. Return null if none found.
	 */
	public JavaClass findClass(String className) throws CorruptDataException {
		JavaClass jc = classNameCache.get(className);
		if (jc == null) {
			jc = findClassUncached(className);
			if (jc != null) {
				classNameCache.put(className, jc);
			}	
		}
		return jc;
	}
	
	JavaClass findClassUncached(String className) {
		for (JavaClass cls : classes.values()) {
			try {
				if (cls.getName().equals(className)) {
					return cls;
				}
			} catch (CorruptDataException e) {
			}
		}
		for (JavaClass cls : arrayClasses.values()) {
			try {
				if (cls != null && cls.getName().equals(className)) {
					return cls;
				}
			} catch (CorruptDataException e) {
			}
		}
		return null;
	}
	
	/**
	 * Finds a class from a name, presuming it is unique. Relies on cache being set up with initCache.
	 * @param className
	 * @return The only class of this name, null if not found or not unique.
	 * @throws CorruptDataException
	 */
	JavaClass findClassUnique(String className) throws CorruptDataException {
		JavaClass ret = null;
		if (!duplicateClassNames.contains(className)) {
			ret = classNameCache.get(className);
		}
		return ret;
	}
	
	
	private Set<JavaClass> findClasses(String className) {
		Set<JavaClass> ret = new LinkedHashSet<JavaClass>();
		for (JavaClass cls : classes.values()) {
			try {
				if (cls.getName().equals(className)) {
					ret.add(cls);
				}
			} catch (CorruptDataException e) {
			}
		}
		return ret;
	}
	
	void initCache() {
		allClasses.clear();
		classNameCache.clear();
		for (JavaClass cls : classes.values()) {
			cacheClass(cls);
		}
		for (JavaClass cls : arrayClasses.values()) {
			// Initially the array class list might not have the actual array class
			// if there were multiple classes of the same name and the right one has not yet
			// been found.
			if (cls != null) cacheClass(cls);
		}
		return;
	}
	
	/**
	 * Add the class to the list of all classes, cache it by name and record whether the name is duplicated or unique.
	 * @param cls
	 */
	private void cacheClass(JavaClass cls) {
		allClasses.add(cls);
		try {
			String clsName = cls.getName();
			if (classNameCache.containsKey(clsName)) {
				duplicateClassNames.add(clsName);
			} else {
				classNameCache.put(clsName, cls);
			}
		} catch (CorruptDataException e) {
			// Ignore
		}
	}
	
	JavaClass findClass(long id) {
		return classes.get(id);
	}
	
	JavaClass findArrayOfClass(long id) {
		return arrayClasses.get(id);
	}
	
	public Iterator<JavaClass> getCachedClasses() {
		return getDefinedClasses();
	}

	public Iterator<JavaClass> getDefinedClasses() {
		return allClasses.iterator();
	}

	public JavaObject getObject() throws CorruptDataException {
		// Fix system class loader for 1.4.2 as null, not object at 0
		if (obj != null && obj.getID().getAddress() == 0) return null;
		return obj;
	}

	
	/**
	 * Get reader to move a class from one loader to another, including the array refs.
	 * @param from
	 * @param j1
	 */
	void prepareToMove(PHDJavaClassLoader from, JavaClass j1) {
		setLoader(j1);
		ImagePointer ip = j1.getID();
		if (ip != null) {
			long address = ip.getAddress();
			JavaClass j2 = from.arrayClasses.get(address);
			if (j2 != null) setLoader(j2);
		}
	}
	
	/**
	 * Set the class loader for a class to this loader
	 * to indicate it should be moved later on
	 * @param j1
	 */
	private void setLoader(JavaClass j1) {
		if (j1 instanceof PHDJavaClass) {
			PHDJavaClass pjc = (PHDJavaClass)j1;
			pjc.setClassLoader(this);
		}
	}
	
	/**
	 * Complete the move from one class loader to another.
	 * @param from
	 * @param j1
	 */
	void move(PHDJavaClassLoader from, JavaClass j1, long lastDummyClassAddress) {
		ImagePointer ip = j1.getID();
		long address = 0;
		if (ip != null) {
			address = ip.getAddress();
		} else {
			// Fix up objects with pseudo-addresses
			for (long i = 1; i <= lastDummyClassAddress; ++i) {
				if (j1.equals(from.findClass(i))) {
					address = i;
					break;
				}	
			}
		}
		if (address != 0) {
			classes.put(address, j1);
			from.classes.remove(address);
			if (from.arrayClasses.containsKey(address)) {
				arrayClasses.put(address, from.arrayClasses.remove(address));
			}
		}
	}

	private void updateAddresses(long addr, long classAddr, int len) {
		if (addr > maxAddress) {
			maxObjClass = classAddr;
			maxObjLen = len;
		}
		maxAddress = Math.max(maxAddress, addr);
		minAddress = Math.min(minAddress, addr);
		bitsAddress |= addr;
	}
	
	private void updateAddresses(long addr, int size, String className) {
		maxClassAddress = Math.max(maxClassAddress, addr);
		minClassAddress = Math.min(minClassAddress, addr);
		if (className != null && !className.startsWith("[")) {
			// Instance size doesn't make sense for arrays
			minInstanceSize = Math.min(minInstanceSize, size);
		}
		bitsAddress |= addr;
	}
	
	public int hashCode() {
		if (obj != null) return obj.hashCode();
		return (int)(minClassAddress ^ maxClassAddress);
	}
}
