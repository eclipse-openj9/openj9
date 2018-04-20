/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2018 IBM Corp. and others
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

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.NoSuchElementException;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.phd.util.LongEnumeration;

/** 
 * @author ajohnson
 */
class PHDJavaClass implements JavaClass {

	private JavaClassLoader loader;
	private String name;
	private final long addr;
	private final long superAddress;
	private final ImageAddressSpace space;
	private final PHDJavaRuntime runtime;
	private Object refs;
	private final int flags;
	private final int hashCode;
	private int size;
	private JavaClass componentType;
	private List<JavaMethod>methods = new ArrayList<JavaMethod>();
	private JavaObject classObject;
	private static final int UNKNOWN_SIZE = Integer.MIN_VALUE;
	static final int UNKNOWN_SUPERCLASS = -1;
	static final String UNKNOWN_ARRAY = "[";
	static final String UNKNOWN_NONARRAY = "]";
	
	/**
	 * The constructors for PHDJavaClass used to take up to 9 arguments, many of which were ints. This made it hard to tie up which 
	 * argument corresponded to which parameter and invited errors since there can be no type checking.
	 * <p>
	 * This is now fixed by the use of the Builder pattern as described in
	 * Effective Java Second Edition by Joshua Bloch (http://cyclo.ps/books/Prentice.Hall.Effective.Java.2nd.Edition.May.2008.pdf), 
	 * item 2 "Consider a builder when faced with many constructor parameters".
	 * <p>
	 * The only way to construct a PHDJavaClass is now using this Builder. Required arguments are set with 
	 * a call to the Builder constructor and optional arguments are set before calling build() using a 
	 * fluent interface.  
	 */
	
	public static class Builder {
		// required parameters, all final, assigned once in the one constructor
		private final ImageAddressSpace space;
		private final PHDJavaRuntime runtime;
		private final JavaClassLoader loader;
		private final long addr;
		private final long superAddress;
		private final String name;
		
		// optional parameters with default values
		private int size = UNKNOWN_SIZE;
		private int flags = PHDJavaObject.NO_HASHCODE; 
		private int hashCode = -1;
		private Object refs = null;
		private JavaClass componentType = null;

		/**
		 * Initialize a Builder for a PHDJavaClass with the six required parameters.
		 * @param space
		 * @param runtime
		 * @param loader
		 * @param address
		 * @param superAddress
		 * @param name
		 */
		public Builder(ImageAddressSpace space, 
				PHDJavaRuntime runtime, 
				JavaClassLoader loader, 
				long address,
				long superAddress,
				String name) {
			this.space = space;
			this.runtime = runtime;
			this.loader = loader;
			this.addr = address;
			this.name = name;
			
			if ("byte".equals(name)||
				"short".equals(name)||
				"int".equals(name)||
				"long".equals(name)||
				"float".equals(name)||
				"double".equals(name)||
				"boolean".equals(name)||
				"char".equals(name)||
				"void".equals(name)) {
				superAddress = 0;
			}
			
			this.superAddress = superAddress;
		}
		
		/**
		 * Add the size attribute to a PHDJavaClass before building it.
		 * @param size
		 * @return the builder for further attributes or a call to build()
		 */
		public Builder size(int size) {
			this.size = size;
			return this;
		}
		
		/**
		 * Add the flags attribute to a PHDJavaClass before building it.
		 * @param flags
		 * @return the builder for further attributes or a call to build()
		 */
		public Builder flags(int flags) {
			this.flags = flags;
			return this;
		}
		
		/**
		 * Add the hashCode attribute to a PHDJavaClass before building it.
		 * @param hashCode
		 * @return the builder for further attributes or a call to build()
		 */
		public Builder hashCode(int hashCode) {
			this.hashCode = hashCode;
			return this;
		}
		
		/**
		 * Add the refs attribute to a PHDJavaClass before building it.
		 * @param refs
		 * @return the builder for further attributes or a call to build()
		 */
		public Builder refs(LongEnumeration refs) {
			this.refs = runtime.convertRefs(refs, 0);
			return this;			
		}

		/**
		 * Add the componentType attribute to a PHDJavaClass before building it.
		 * @param componentType
		 * @return the builder for further attributes or a call to build()
		 */
		public Builder componentType(JavaClass componentType) {
			this.componentType = componentType;
			return this;			
		}
		
		/**
		 * Build the PHDJavaClass using all the required and optional values given so far. 
		 * @return the PHDJavaClass
		 */
		public PHDJavaClass build() {
			return new PHDJavaClass(this);
		}		
	}
	
	public PHDJavaClass(Builder builder) {
		space = builder.space;
		runtime  = builder.runtime;
		loader = builder.loader;
		addr = builder.addr;
		superAddress = builder.superAddress;
		name = builder.name;
		size = builder.size;
		flags = builder.flags;
		hashCode = builder.hashCode;
		refs = builder.refs;
		componentType = builder.componentType;	
	}


	public JavaClassLoader getClassLoader() throws CorruptDataException {
		return loader;
	}
	
	JavaClassLoader setClassLoader(JavaClassLoader jcl) {
		JavaClassLoader ret = loader;
		loader = jcl;
		return ret;
	}

	public JavaClass getComponentType() throws CorruptDataException {
		if (!isArray()) throw new IllegalArgumentException(getName()+" is not an array");
		if (componentType == null) {
			String compName;
			String nm = getName();
			if (nm.startsWith("[[")) {
				compName = nm.substring(1);
			} else if (nm.startsWith("[L")) {
				// Remove "L" and semicolon ";"
				compName = nm.substring(2, nm.length()-1);			
			} else if (nm.equals("[B")) compName = "byte";
			else if (nm.equals("[C")) compName = "char";
			else if (nm.equals("[S")) compName = "short";
			else if (nm.equals("[I")) compName = "int";
			else if (nm.equals("[J")) compName = "long";
			else if (nm.equals("[F")) compName = "float";
			else if (nm.equals("[D")) compName = "double";
			else if (nm.equals("[Z")) compName = "boolean";
			else compName = "java/lang/Object";

			componentType = loader.findClass(compName);
			if (componentType == null) {
				componentType = new PHDCorruptJavaClass("component type for "+nm+" is "+compName+" which is not found from "+loader,getID(),null);
			}
		}
		if (componentType instanceof CorruptData) throw new CorruptDataException((CorruptData)componentType);
		return componentType;
	}
	
	/**
	 * Set the component type if it has not been already set.
	 * @param comp The component type, or null to query the type
	 * @return The cached type
	 */
	JavaClass setComponentType(JavaClass comp) {
		JavaClass ret = componentType;
		if (ret == null) {
			componentType = comp;
		}
		return ret;
	}
	
	public Iterator<JavaObject> getConstantPoolReferences() {
		return Collections.<JavaObject>emptyList().iterator();

	}

	public Iterator<JavaField> getDeclaredFields() {
		return Collections.<JavaField>emptyList().iterator();
	}

	public Iterator<JavaMethod> getDeclaredMethods() {
		return methods.iterator();
	}

	public ImagePointer getID() {
		return addr != 0 ? space.getPointer(addr) : null; 
	}

	public Iterator<String> getInterfaces() {
		return Collections.<String>emptyList().iterator();
	}

	public int getModifiers() throws CorruptDataException {
		return MODIFIERS_UNAVAILABLE; 
	}

	public String getName() throws CorruptDataException {
		if (name == null) {
			throw new CorruptDataException(new PHDCorruptData("No class name available (null)", getID()));
		} else if (name.equals(UNKNOWN_ARRAY)) {
			throw new CorruptDataException(new PHDCorruptData("No class name available (unknown array)", getID()));
		} else if (name.equals(UNKNOWN_NONARRAY)) {
			throw new CorruptDataException(new PHDCorruptData("No class name available (unknown non-array)", getID()));
		} else {
			return name;
		}
	}

	public JavaObject getObject() throws CorruptDataException {
		if (addr == 0) return null;
		if (classObject != null) return classObject;
		JavaClass jlc = runtime.findClass("java/lang/Class");
		// Early in initialization the runtime might not know about classes
		if (jlc == null) jlc = loader.findClass("java/lang/Class");
		// If the object isn't in the heap at this address then we can't get refs for the object
		// Here we presume there won't be any extra references, then in PHDJavaRuntime we look for a real object on the heap.
		classObject = new PHDJavaObject.Builder((PHDJavaHeap)(runtime.getHeaps().next()),addr,jlc,flags,hashCode)
		.refsAsArray(new long[0],0).length(PHDJavaObject.SIMPLE_OBJECT).build();
		return classObject;
	}

	public JavaClass getSuperclass() throws CorruptDataException {
		if (superAddress == 0) {
			return null;
		} else if (superAddress == UNKNOWN_SUPERCLASS) {
			throw new CorruptDataException(new PHDCorruptJavaClass("No superclass available", null, null));
		} else {
			JavaClass sup = runtime.findClass(superAddress);
			try { 
				// PHD Version 4 bug - bad superclass: J2RE 5.0 IBM J9 2.3 AIX ppc64-64 build 20080314_17962_BHdSMr
				if (sup != null && "java/lang/Class".equals(sup.getName())) sup = null;
			} catch (CorruptDataException e) {
				// Ignore
			}
			if (sup == null) throw new CorruptDataException(new PHDCorruptJavaClass("Superclass not found", space.getPointer(superAddress), null));
			return sup;
		}
	}

	public boolean isArray() throws CorruptDataException {
		return componentType != null 
			|| name != null && name.startsWith("[") 
			|| !UNKNOWN_NONARRAY.equals(name) && getName().startsWith("[");
	}

	public Iterator<JavaReference> getReferences() {
		final JavaClass source = this;
		return new Iterator<JavaReference>() {
			int count;
			JavaClass sup;
			JavaObject load;
			{
				try {
					sup = getSuperclass();
					if (sup != null) count = -1;
				} catch (CorruptDataException e) {					
				}
				try {
					load = loader.getObject();
					if (load != null) count = -2;
				} catch (CorruptDataException e) {					
				}
			}
			public boolean hasNext() {
				if (count < 0) {
					return true;
				} else if (refs instanceof LongEnumeration) {
					LongEnumeration le = (LongEnumeration)refs;
					return le.hasMoreElements();
				} else if (refs instanceof long[]) {
					long arefs[] = (long[])refs;
					return count < arefs.length;
				} else if (refs instanceof int[]) {
					int arefs[] = (int[])refs;
					return count < arefs.length;					
				} else {
					return false;
				}
			}

			public JavaReference next() {
				if (!hasNext()) throw new NoSuchElementException(""+count++);
				long ref;
				int refType = PHDJavaReference.REFERENCE_UNKNOWN;
				Object cls = null;
				if (count == -2) {
					cls = load;
					++count;
					// Skip over the superclass if not present
					if (sup == null) ++count;
					ref = 0;
					refType = PHDJavaReference.REFERENCE_CLASS_LOADER;
				} else if (count == -1) {
					cls = sup;
					++count;
					ref = 0;
					refType = PHDJavaReference.REFERENCE_SUPERCLASS;
				} else {
					if (refs instanceof LongEnumeration) {
						LongEnumeration le = (LongEnumeration)refs;
						ref = le.nextLong();
						++count;
					} else if (refs instanceof int[]) {
						int arefs[] = (int[])refs;
						ref = runtime.expandAddress(arefs[count++]);						
					} else {
						long arefs[] = (long[])refs;	
						ref = arefs[count++];
					}
					cls = runtime.findClass(ref);
				}
				if (cls != null) {
					return new PHDJavaReference(
							cls,
							source,
							PHDJavaReference.REACHABILITY_STRONG,
							refType,
							PHDJavaReference.HEAP_ROOT_UNKNOWN,"?");					 
				} else {
					return new PHDJavaReference(
							new PHDJavaObject.Builder((PHDJavaHeap)(runtime.getHeaps().next()),ref,null,PHDJavaObject.NO_HASHCODE,-1).build(),
							source,
							PHDJavaReference.REACHABILITY_STRONG,
							refType,
							PHDJavaReference.HEAP_ROOT_UNKNOWN,"?");
				}
			}

			public void remove() {
				throw new UnsupportedOperationException();
			}
			
		};
	}

	public String toString() {
		return "PHDJavaClass "+name+"@"+Long.toHexString(addr);
	}
	
	public int hashCode() {
		return (int)addr ^ (int)(addr >>> 32); 
	}
	
	/**
	 * Calculate the size of an instance.
	 * A negative size field means a size calculated from object spacings.
	 * Adjust the size if no class size includes the header.
	 * @return
	 * @throws CorruptDataException
	 */
	public long getInstanceSize() throws CorruptDataException {
		int header;
		if (size == UNKNOWN_SIZE) throw new CorruptDataException(new PHDCorruptData("No class size available", getID()));
		if (runtime.minInstanceSize == 0 && size >= 0) {
			// Java 1.4.2 SR10 returns 0 for some classes, so no header is allowed for
			if (runtime.is64Bit()) header = 3*8; else header = 3*4;
		} else {
			header = 0;
		}
		return header + Math.abs(size);
	}
	
	long getArraySize(int length) throws CorruptDataException {
		if (!isArray()) throw new IllegalArgumentException(getName()+" is not an array");
		int s1 = 0;
		if (name != null) {
			if (name.equals("[B")||name.equals("[Z")) s1 = 1;
			else if (name.equals("[S")||name.equals("[C")) s1 = 2;
			else if (name.equals("[I")||name.equals("[F")) s1 = 4;
			else if (name.equals("[D")||name.equals("[J")) s1 = 8;
		}
		// Header is 2 pointers + 2 * 32-bits?, except on 1.4.2 where primitive arrays are 2 pointers+1 32-bit 
		int prefix = runtime.pointerSize()*2 + (runtime.minInstanceSize == 0 && s1 != 0 ? 4 : 8);
		if (s1 == 0) s1 = runtime.pointerSize();
		// Rounding is excluded
		return prefix + (long)length*s1;
	}
	
	/**
	 * Is there a reference from one class to another?
	 * E.g. from an array class to the component type? This would be an indication that they
	 * are truly related.
	 * @param from
	 * @param to
	 * @return
	 */
	static boolean referencesClass(JavaClass from, JavaClass to) {
		for (Iterator i1 = from.getReferences(); i1.hasNext(); ) {
			Object o = i1.next();
			if (o instanceof CorruptData) continue;
			JavaReference jr = (JavaReference)o;
			try {
				if (jr.isClassReference() && jr.getTarget().equals(to)) {
					return true;
				}
				if (jr.isObjectReference() && jr.getTarget().equals(to.getObject())) {
					return true;
				}
			} catch (DataUnavailable e) {
			} catch (CorruptDataException e) {				
			}
		}
		return false;
	}

	void addMethod(PHDJavaMethod javaMethod) {
		methods.add(javaMethod);
	}
	
	/**
	 * Set the on-heap Java object for this class
	 * @param obj
	 */
	void setJavaObject(JavaObject obj) {
		classObject = obj;
	}

	/**
	 * Change the name of the class - normally only used to set an unknown name
	 * @param newName
	 */
	void setName(String newName) {
		name = newName;
	}
	
	/**
	 * For dummy classes get the size from the smallest instance of the class.
	 * @param instanceAddress The address of an instance of this class.
	 * @param nextObjectAddress The address of the following object.
	 * Use a negative size to indicate an approximate size so we don't adjust the size of real classes.
	 */
	void updateSize(long instanceAddress, long nextObjectAddress) {
		long delta = nextObjectAddress - instanceAddress;
		if (size < 0 && delta > 0 && delta < 0x10000 && -delta > size) {
			size = -(int)delta;
		}
	}

	public JavaObject getProtectionDomain() throws DataUnavailable,	CorruptDataException {
		throw new DataUnavailable("This implementation of DTFJ does not support getProtectionDomain");
	}
	
}
