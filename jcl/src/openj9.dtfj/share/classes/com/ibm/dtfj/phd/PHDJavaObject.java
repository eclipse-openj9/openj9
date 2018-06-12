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
import java.util.Iterator;
import java.util.List;
import java.util.NoSuchElementException;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.phd.util.LongEnumeration;

/** 
 * @author ajohnson
 */
public class PHDJavaObject implements JavaObject {

	private final long address;
	private final PHDJavaHeap heap;
	private JavaClass cls;
	private Object refs;
	private int hashCode;
	private long instanceSize;
	
	/** bit 0 -> hashed (persistent), bit 1 -> hashed and moved (size changed?), bit 2 -> no valid hash */
	static final int HASHED = 1;
	static final int HASHED_AND_MOVED = 2;
	static final int NO_HASHCODE = 4;
	private int flags;
	/** A non-array object of known type */
	static final int SIMPLE_OBJECT = -1;
	/** An object of unknown type - either array or simple object */
	static final int UNRESOLVED_TYPE = -2;
	/** An object where we will never know the type */
	static final int UNKNOWN_TYPE = -3;
	/** array length if >= 0, -1 means simple object, -2 means unresolved, -3 means type never known */
	private int length = UNRESOLVED_TYPE;
	/** instance size is an unsigned int in the datastream, so -1L is a value that cannot collide */
	public static final long UNSPECIFIED_INSTANCE_SIZE = -1L;
	
	/**
	 * The constructors for PHDJavaObject used to take up to 11 arguments, many of which were ints. This made it hard to tie up which 
	 * argument corresponded to which parameter and invited errors since there can be no type checking.
	 * <p>
	 * This is now fixed by the use of the Builder pattern as described in
	 * Effective Java Second Edition by Joshua Bloch (http://cyclo.ps/books/Prentice.Hall.Effective.Java.2nd.Edition.May.2008.pdf), 
	 * item 2 "Consider a builder when faced with many constructor parameters".
	 * <p>
	 * The only way to construct a PHDJavaObject is now using this Builder. Required arguments are set with 
	 * a call to the Builder constructor, then optional arguments are set, then build() is called, usually all 
	 * in one line using a fluent interface.  
	 */
	public static class Builder {
		// required parameters, all final, assigned once in the one constructor
		private final PHDJavaHeap heap;
		private final long address;
		private final JavaClass cls;
		private final int flags;
		private final int hashCode;
		
		// optional parameters with default values
		private Object refs = null;
		private int length = UNRESOLVED_TYPE;
		private long instanceSize = UNSPECIFIED_INSTANCE_SIZE;
		
		/**
		 * Initialize a Builder for a PHDJavaClass with the five required parameters.
		 * @param heap
		 * @param address
		 * @param cls
		 * @param flags
		 * @param hashCode
		 */
		public Builder(PHDJavaHeap heap, long address, JavaClass cls, int flags, int hashCode) {
			this.heap = heap;
			this.address = address;
			this.cls = cls;
			this.flags = flags;
			this.hashCode = hashCode;
		}
			
		/**
		 * Add the refs attribute to a PHDJavaObject before building it.
		 * @param refs
		 * @param skipped
		 * @return the builder for further attributes or a call to build()
		 */
		public Builder refs(LongEnumeration refs, int skipped) {
			this.refs = heap.runtime.convertRefs(refs, skipped);
			return this;
		}
		
		/**
		 * Add the refs attribute to a PHDJavaObject before building it.
		 * @param refs
		 * @param skipped
		 * @return the builder for further attributes or a call to build()
		 */
		public Builder refsAsArray(long refs[], int skipped) {
			this.refs = heap.runtime.convertRefs(refs, skipped);
			return this;
		}
		
		/**
		 * Add the length attribute to a PHDJavaObject before building it.
		 * @param length
		 * @return
		 */
		public Builder length(int length) {
			this.length = length;
			return this;
		}	
		
		/**
		 * Add the instance size attribute to a PHDJavaObject before building it.
		 * @param instance size
		 * @return
		 */
		public Builder instanceSize(long instanceSize) {
			this.instanceSize = instanceSize;
			return this;
		}
		
		/**
		 * Build the PHDJavaObject using all the required and optional values given so far. 
		 * @return the PHDJavaObject
		 */
		public PHDJavaObject build() {
			return new PHDJavaObject(this);
		}
	}
	
	public PHDJavaObject(Builder builder) {
		heap = builder.heap;
		address = builder.address;
		cls = builder.cls;
		flags = builder.flags;
		hashCode = builder.hashCode;
		refs = builder.refs;
		length = builder.length;
		instanceSize = builder.instanceSize;
	}
	
	public void arraycopy(int srcStart, Object dst, int dstStart, int length)
			throws CorruptDataException, MemoryAccessException {		
		if (dst == null) throw new NullPointerException("destination null");
		fillInDetails(true);
		if (!isArray()) throw new IllegalArgumentException(this+" is not an array");
		JavaClass jc = getJavaClass();
		String type;
		try {
			type = jc.getName();
		} catch (CorruptDataException e) {
			// Presume all primitive arrays will have a real name
			type = "[L";
		}

		//System.out.println("array "+srcStart+" "+dst+" "+dstStart+" "+length);
		if (srcStart < 0 || length < 0 || dstStart < 0 || srcStart + length < 0 || dstStart + length < 0)
			throw new IndexOutOfBoundsException(srcStart+","+dstStart+","+length);
			
		if (srcStart + length > getArraySize()) 
			throw new IndexOutOfBoundsException(srcStart+"+"+length+">"+getArraySize()+jc);
		
		if (dst instanceof JavaObject[]) {
			if (!type.startsWith("[[") && !type.startsWith("[L")) throw new IllegalArgumentException("Expected "+type+" not "+dst);
			JavaObject dst1[] = (JavaObject[])dst; 
			
			// Get to the right point in the refs
			int count;
			if (refs instanceof LongEnumeration) {
				LongEnumeration le = (LongEnumeration)refs;
				count = le.numberOfElements();
				// Skip over the elements before the required start
				for (int idx = 0; idx < srcStart && idx < count; ++idx) {
					le.nextLong();
				}
			} else if (refs instanceof int[]) {
				int arefs[] = (int[]) refs;
				count = arefs.length;
			} else if (refs instanceof long[]) {
				long arefs[] = (long[]) refs;
				count = arefs.length;
			} else {
				throw new CorruptDataException(new PHDCorruptData("Unknown array contents", getID()));
			}
			
			// Copy the data to the destination
			for (int idx = srcStart; idx < srcStart + length; ++idx) {
				JavaObject target;
				if (idx < count) {
					long ref;
					if (refs instanceof LongEnumeration) {
						LongEnumeration le = (LongEnumeration) refs;
						ref = le.nextLong();
					} else if (refs instanceof int[]) {
						int arefs[] = (int[]) refs;
						ref = heap.getJavaRuntime().expandAddress(arefs[idx]);
					} else {
						long arefs[] = (long[]) refs;
						ref = arefs[idx];
					}
					target = new PHDJavaObject.Builder(heap, ref, null, PHDJavaObject.NO_HASHCODE, -1).build();
				} else {
					target = null;
				}
				int dstIndex = dstStart + (idx - srcStart);
				if (dstIndex >= dst1.length) {
					throw new IndexOutOfBoundsException("Array " + jc + " 0x"
							+ Long.toHexString(address) + "[" + getArraySize()
							+ "]" + "," + srcStart + "," + dst1 + "["
							+ dst1.length + "]" + "," + dstStart + "," + length
							+ " at " + idx);
				}
				dst1[dstIndex] = target;
			}
		} else if (dst instanceof byte[]) {
			if (!type.startsWith("[B")) throw new IllegalArgumentException("Expected "+type+" not "+dst);
			byte dst1[] = (byte[])dst;
			if (dstStart + length > dst1.length) throw new IndexOutOfBoundsException();
		} else if (dst instanceof short[]) {
			if (!type.startsWith("[S")) throw new IllegalArgumentException("Expected "+type+" not "+dst);
			short dst1[] = (short[])dst;
			if (dstStart + length > dst1.length) throw new IndexOutOfBoundsException();
		} else if (dst instanceof int[]) {
			if (!type.startsWith("[I")) throw new IllegalArgumentException("Expected "+type+" not "+dst);
			int dst1[] = (int[])dst;
			if (dstStart + length > dst1.length) throw new IndexOutOfBoundsException();
		} else if (dst instanceof long[]) {
			if (!type.startsWith("[J")) throw new IllegalArgumentException("Expected "+type+" not "+dst);
			long dst1[] = (long[])dst;
			if (dstStart + length > dst1.length) throw new IndexOutOfBoundsException();
		} else if (dst instanceof boolean[]) {
			if (!type.startsWith("[Z")) throw new IllegalArgumentException("Expected "+type+" not "+dst);
			boolean dst1[] = (boolean[])dst;
			if (dstStart + length > dst1.length) throw new IndexOutOfBoundsException();
		} else if (dst instanceof char[]) {
			if (!type.startsWith("[C")) throw new IllegalArgumentException("Expected "+type+" not "+dst);
			char dst1[] = (char[])dst;
			if (dstStart + length > dst1.length) throw new IndexOutOfBoundsException();
		} else if (dst instanceof float[]) {
			if (!type.startsWith("[F")) throw new IllegalArgumentException("Expected "+type+" not "+dst);
			float dst1[] = (float[])dst;
			if (dstStart + length > dst1.length) throw new IndexOutOfBoundsException();
		} else if (dst instanceof double[]) {
			if (!type.startsWith("[D")) throw new IllegalArgumentException("Expected "+type+" not "+dst);
			double dst1[] = (double[])dst;
			if (dstStart + length > dst1.length) throw new IndexOutOfBoundsException();			
		} else {
			throw new IllegalArgumentException("Expected "+type+" not "+dst);
		}
	}

	public int getArraySize() throws CorruptDataException {
		if (!isArray()) throw new IllegalArgumentException(this+" is not an array");
		if (length == UNKNOWN_TYPE) throw new CorruptDataException(new PHDCorruptData("Unknown length", getID()));
		return length;
	}

	public long getHashcode() throws DataUnavailable, CorruptDataException {
		getJavaClass();
		if ((flags & NO_HASHCODE) != 0) throw new DataUnavailable("no hashcode available");
		return hashCode;
	}

	public JavaHeap getHeap() throws CorruptDataException, DataUnavailable {
		return heap;
	}

	public ImagePointer getID() {
		return heap.getImageAddressSpace().getPointer(address);
	}

	public JavaClass getJavaClass() throws CorruptDataException {
		fillInDetails(false);
		if (cls == null) {
			CorruptData cd = new PHDCorruptJavaClass("Unable to get type for object", getID(), null);
			throw new CorruptDataException(cd);
		}
		return cls;
	}

	public long getPersistentHashcode() throws DataUnavailable,
			CorruptDataException {
		getJavaClass();
		if ((flags & NO_HASHCODE) != 0) throw new DataUnavailable("no hashcode available");
		if ((flags & (HASHED | HASHED_AND_MOVED)) == 0) throw new DataUnavailable("hashcode not persistent");
		return hashCode;
	}

	public Iterator<JavaReference> getReferences() {
		fillInDetails(true);
		final JavaObject source = this;
		return new Iterator<JavaReference>() {
			int count = -1;
			Iterator<JavaClass> loaderCls = heap.runtime.getLoaderClasses(source);
			public boolean hasNext() {
				if (count < 0) {
					return true;
				} else if (loaderCls.hasNext()) {
					return true;
				} else if (refs instanceof LongEnumeration) {
					LongEnumeration le = (LongEnumeration)refs;
					return count < le.numberOfElements();
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
				JavaClass cls1;
				if (count == -1) {
					// Add the type
					cls1 = cls;
					ref = 0; // Doesn't matter
					++count;
					refType = PHDJavaReference.REFERENCE_CLASS;
				} else if (loaderCls.hasNext()) {
					refType = PHDJavaReference.REFERENCE_LOADED_CLASS;
					cls1 = loaderCls.next();
					ref = 0; // Doesn't matter
				} else {
					if (refs instanceof LongEnumeration) {
						LongEnumeration le = (LongEnumeration)refs;
						ref = le.nextLong();
						++count;
					} else if (refs instanceof int[]) {
						int arefs[] = (int[])refs;
						ref = heap.getJavaRuntime().expandAddress(arefs[count++]);
					} else {
						long arefs[] = (long[])refs;	
						ref = arefs[count++];
					}
					if (length >= 0) {
						refType = PHDJavaReference.REFERENCE_ARRAY_ELEMENT;
					} else {
						refType = PHDJavaReference.REFERENCE_FIELD;
					}
				    cls1 = heap.getJavaRuntime().findClass(ref);
				}
				if (cls1 != null) {
					return new PHDJavaReference(
							cls1,
							source,
							PHDJavaReference.REACHABILITY_STRONG,
							refType,
							PHDJavaReference.HEAP_ROOT_UNKNOWN,"?");
				} else {
					return new PHDJavaReference(
							new PHDJavaObject.Builder(heap,ref,null,PHDJavaObject.NO_HASHCODE,-1).build(),
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

	public Iterator<ImageSection> getSections() {
		List<ImageSection> c = new ArrayList<ImageSection>();
		ImageSection s;
		try {
			s = new PHDImageSection("Object section",getID(),getSize());
		} catch (CorruptDataException e) {
			s = new PHDCorruptImageSection("Corrupt object section",getID());
		}
		c.add(s);
		return c.iterator();
	}

	public long getSize() throws CorruptDataException {
		if (instanceSize != UNSPECIFIED_INSTANCE_SIZE) {
			return instanceSize;
		}
		JavaClass cls = getJavaClass();
		if (isArray()) { 
			return ((PHDJavaClass)cls).getArraySize(length);
		} else {
			return ((PHDJavaClass)cls).getInstanceSize();
		}
	}

	public boolean isArray() throws CorruptDataException {
		if (length >= 0) return true;
		if (length == SIMPLE_OBJECT) return false;
		return getJavaClass().isArray();
	}
	
	public boolean equals(Object o) {
		if (!(o instanceof PHDJavaObject)) return false;
		PHDJavaObject p = (PHDJavaObject)o;
		return heap.equals(p.heap) && address == p.address;
	}
	
	public int hashCode() {
		return (int)address ^ (int)(address >>> 32); 
	}
	
	/**
	 * Sometimes a JavaObject is constructed without knowing anything about the type etc.
	 * If this info is needed then try to retrieve it now.
	 * @param withRefs Do we need to fill in the references array too? 
	 */
	private void fillInDetails(boolean withRefs) {
		if (length == UNRESOLVED_TYPE || withRefs && refs == null) {
			JavaObject jo = heap.getObjectAtAddress(getID(),withRefs);
			if (equals(jo)) {
				PHDJavaObject po = (PHDJavaObject)jo;
				if (po.cls != null) {
					this.cls = po.cls;
					this.hashCode = po.hashCode;
					this.flags = po.flags;
				} else {
					//System.out.println("Oops null cls "+jo+" "+cls);
				}
				this.length = po.length;
				this.refs = po.refs;
			} else {
				//System.out.println("Oops "+jo+" "+this);
				length = UNKNOWN_TYPE;
			}
		}
	}

	public String toString() {
		try {
			String className = getJavaClass().getName();
			return "Instance of " + className + " @ 0x"+ Long.toHexString(address);
		} catch (CorruptDataException e) {
			return super.toString();
		}
	}
}
