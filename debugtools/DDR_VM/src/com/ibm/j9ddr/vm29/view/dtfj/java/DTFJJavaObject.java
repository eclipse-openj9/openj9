/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.view.dtfj.java;

import static com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils.corruptIterator;

import java.lang.reflect.Modifier;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.image.J9DDRCorruptData;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageSection;
import com.ibm.j9ddr.view.dtfj.java.helper.DTFJJavaRuntimeHelper;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.j9.ObjectAccessBarrier;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.generated.J9ArrayClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9IndexableObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.structure.J9Object;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;
import com.ibm.j9ddr.vm29.view.dtfj.java.corrupt.CorruptJavaObject;

public class DTFJJavaObject implements JavaObject {
	private static final Class<?>[] whitelist = new Class<?>[]{NullPointerException.class, ArrayIndexOutOfBoundsException.class, IllegalArgumentException.class};
	protected final J9ObjectPointer object;
	protected DTFJJavaHeap heap;
	protected J9ArrayClassPointer arrayptr = null;
	
	private boolean hasDeferredData = true;
	private CorruptDataException corruptException = null;
	private long size = 0;
	boolean objectIsArray = false;
	private int arraySize = 0;
	private String label = null;				//the cached value returned by toString();
	private List<Object> references;
	
	public DTFJJavaObject(DTFJJavaHeap heap, J9ObjectPointer object) {
		this.object = object;
		this.heap = heap;
	}

	public DTFJJavaObject(J9ObjectPointer object) {
		this(null, object);
	}

	J9ObjectPointer getJ9ObjectPointer() {
		return object;
	}
	
	public void arraycopy(int srcStart, Object dst, int dstStart, int length) throws CorruptDataException, MemoryAccessException {
		fetchDeferredData();
		if(!objectIsArray) {
			throw new IllegalArgumentException("Object is not an array");
		}
		J9IndexableObjectPointer array = J9IndexableObjectPointer.cast(object);
		try {
			validateArrayCopyParameters(array, srcStart, dst, dstStart, length);
			
			//CMVC 171150 : use helper object to correctly get the class name
			String className = J9IndexableObjectHelper.getClassName(array);
			
			if ((null == className) || (className.length() < 2)) {
				J9DDRCorruptData cd = new J9DDRCorruptData(DTFJContext.getProcess(), "The class name for this object could not be determined", object.getAddress());
				throw new CorruptDataException(cd);
			}
			
			if (className.charAt(1) == 'L' || className.charAt(1) == '[') {
				//For objects we need to add an intermediate layer of J9ObjectPointer array and do our own args checking
				// JExtract/DTFJ can cope with dst of either Object[] or JavaObject[] - but we need to detect other things
				if ( !dst.getClass().equals(Object[].class) && !(dst instanceof JavaObject[])) {
					throw new IllegalArgumentException("Type of dst object (" + dst.getClass().getName() + ") incompatible with Object array. Should be JavaObject[] or Object[]");
				}
				
				J9ObjectPointer[] intermediateArray = new J9ObjectPointer[length];
				Object[] castedDst = (Object[])dst;

				if (dstStart + (long) length > castedDst.length) {
					throw new ArrayIndexOutOfBoundsException("Supplied destination array too small. Requires: " + (dstStart + (long) length) + ", was " + castedDst.length);
				}


				J9IndexableObjectHelper.getData(array, intermediateArray,srcStart,length,0);

				for (int i=0; i < length; i++) {
					if (intermediateArray[i].isNull()) {
						castedDst[dstStart + i] = null;
					} else {
						castedDst[dstStart + i] = new DTFJJavaObject(intermediateArray[i]);
					}
				}
			} else {
				//For primitives we can pass through the client object. The type verification will be done in J9IndexableObjectPointer
				J9IndexableObjectHelper.getData(array, dst, srcStart, length, dstStart);
			}

		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}

	private static void validateArrayCopyParameters(J9IndexableObjectPointer array, int srcStart, Object dst, int dstStart, int length) throws com.ibm.j9ddr.CorruptDataException {
		if(dst == null) {
			throw new NullPointerException("Destination array is null");
		}
		if(srcStart < 0) {
			throw new ArrayIndexOutOfBoundsException("Source copy start is out of range: " + srcStart);
		}
		if(dstStart < 0) {
			throw new ArrayIndexOutOfBoundsException("Destination copy start is out of range: " + dstStart);
		}
		if(length < 0) {
			throw new ArrayIndexOutOfBoundsException("Copy length is out of range: " + length);
		}
		long size = J9IndexableObjectHelper.size(array).longValue();
		if (srcStart + (long) length > size) {
			throw new ArrayIndexOutOfBoundsException("source array index out of range: " + (srcStart + (long) length));
		}
		if (! dst.getClass().isArray()) {
			throw new IllegalArgumentException("Destination object of type " + dst.getClass().getName() + " is not an array");
		}
	}
	
	public int getArraySize() throws CorruptDataException {
		fetchDeferredData();
		if(objectIsArray) {
			return arraySize;
		} else {
			throw new IllegalArgumentException("This object is not an array");
		}
	}

	public long getHashcode() throws DataUnavailable, CorruptDataException {
		try {
			return ObjectAccessBarrier.getObjectHashCode(object).longValue();
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButDataUnavailAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public JavaHeap getHeap() throws CorruptDataException, DataUnavailable {
		try {
			if (heap == null) {
				DTFJJavaRuntime runtime = DTFJContext.getRuntime();
				heap = runtime.getHeapFromAddress(DTFJContext.getImagePointer(object.getAddress()));
				if (heap == null) {
					throw new CorruptDataException(new CorruptJavaObject(null, object));
				}
			}
			return heap;
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public ImagePointer getID() {
		return DTFJContext.getImagePointer(object.getAddress());
	}

	public JavaClass getJavaClass() throws CorruptDataException {
		try {
			//CMVC 168912 : go through helper class to allow for SWH changes
			J9ClassPointer ptr = J9ObjectHelper.clazz(object);
			return new DTFJJavaClass(ptr);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public long getPersistentHashcode() throws DataUnavailable,	CorruptDataException {
		try {
			/*
			 * DTFJ Spec:  If the VM uses a 'hasBeenHashed' bit, the value of this bit can be inferred by calling getPersistentHashcode(). 
			 * If the persistent hash code is not available, then the 'hasBeenHashed' bit has not been set, and the hash of the object 
			 * could change if the object moves between snapshots
			 */
			if (ObjectModel.hasBeenHashed(object)) {
				return this.getHashcode();
			} else {
				throw new DataUnavailable("Object has never been hashed and therefore has no persistent hash code"); 
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButDataUnavailAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
	@SuppressWarnings("rawtypes")
	public Iterator getReferences() {
		boolean isClass = false;
		boolean isClassLoader = false;
		boolean isWeakReference = false;
		boolean isSoftReference = false;
		boolean isPhantomReference = false;
		
		if (null == references) {
			references = new LinkedList<Object>();
			try {
				// find this object's class
				JavaClass jClass = getJavaClass();

				//add a reference to the object's class
				if (null != jClass) {
					JavaReference ref = new DTFJJavaReference(this,jClass,"Class",JavaReference.REFERENCE_CLASS,JavaReference.HEAP_ROOT_UNKNOWN,JavaReference.REACHABILITY_STRONG);
					references.add(ref);
				}

				if (isArray()) {
					if (isObjectArray()) {
						addObjectArrayReferences();
					}
				} else {
					isClass = (jClass != null) && jClass.getName().equals("java/lang/Class");
					
					List<JavaClass> superClasses = new LinkedList<JavaClass>();
					
					//Do superclass walk
					while (jClass != null) {
						String className = jClass.getName();
						isClassLoader |= className.equals("java/lang/ClassLoader");
						isWeakReference |= className.equals("java/lang/ref/WeakReference");
						isSoftReference |= className.equals("java/lang/ref/SoftReference");
						isPhantomReference |= className.equals("java/lang/ref/PhantomReference");
						
						superClasses.add(jClass);
						
						jClass = jClass.getSuperclass();
					}
					
					int reachability = isWeakReference ? JavaReference.REACHABILITY_WEAK : 
									isSoftReference ? JavaReference.REACHABILITY_SOFT :
									isPhantomReference ? JavaReference.REACHABILITY_PHANTOM : 
									JavaReference.REACHABILITY_STRONG;
					
					
					for (JavaClass clazz : superClasses) {
						addFieldReferences(clazz,reachability);
					}
				}
				
			} catch (CorruptDataException e) {
				// Corrupt data, so add it to the container.
				references.add(e.getCorruptData());
			} catch (Throwable t) {
				CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
				references.add(cd);
			}
			
			// Now add association-specific references
			if (isClassLoader) {
				try {
					JavaClassLoader associatedClassLoader = getAssociatedClassLoader();
					
					if (associatedClassLoader != null) {
						for (Iterator classes = associatedClassLoader.getDefinedClasses(); classes.hasNext();) {
							Object potentialClass = classes.next();
							if (potentialClass instanceof JavaClass) {
								JavaClass currentClass = (JavaClass)potentialClass;
								JavaReference ref = new DTFJJavaReference(this, currentClass, "Loaded class", JavaReference.REFERENCE_LOADED_CLASS, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
								references.add(ref);
							}
						}
					} else {
						references.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Couldn't find associated JavaClassLoader for classloader object " + this));
					}
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				} 
			}

			if (isClass) {
				try {
					JavaClass associatedClass = getAssociatedClass();
					if (associatedClass != null) {
						JavaReference ref = new DTFJJavaReference(this, associatedClass, "Associated class", JavaReference.REFERENCE_ASSOCIATED_CLASS,JavaReference.HEAP_ROOT_UNKNOWN,JavaReference.REACHABILITY_STRONG);
						references.add(ref);
					} else {
						// No associated class found. For anonymous classes this is expected, the class is not defined in a classloader.  
						J9ClassPointer j9Class = ConstantPoolHelpers.J9VM_J9CLASS_FROM_HEAPCLASS(object);
						if (!J9ClassHelper.isAnonymousClass(j9Class)) {
							// Not an anonymous class, so something is wrong/damaged, add a corrupt data object to the references list
							references.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Couldn't find associated JavaClass for Class object " + this));
						}
					}
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
			
		}
		return references.iterator(); 
	}

	private JavaClassLoader getAssociatedClassLoader()
	{
		Iterator<?> classloaderIt = DTFJContext.getRuntime().getJavaClassLoaders();
		
		while (classloaderIt.hasNext()) {
			Object clObj = classloaderIt.next();
			
			if (clObj instanceof JavaClassLoader) {
				JavaClassLoader loader = (JavaClassLoader)clObj;
				
				try {
					if (loader.getObject().equals(this)) {
						return loader;
					}
				} catch (CorruptDataException e) {
					//Deliberately do nothing. If this is the loader we're interesting in, we'll
					//insert a CorruptData object later when it isn't found. If it's not the classloader
					//we want, we don't need to worry
				}
			}
		}
		
		return null;
	}

	private JavaClass getAssociatedClass() {
		// Retrieve the class loader for this class for using introspection to speed up
		// lookup when lots of classes are loaded by lots of class loaders.
		JavaObject targetClassLoaderObject = null;
		try {
			JavaClass javaLangClass = this.getJavaClass();
			Iterator<?> fields = javaLangClass.getDeclaredFields();
			while( fields.hasNext() ) {
				Object o = fields.next();
				if( o instanceof JavaField) {
					JavaField field = (JavaField) o;
					if( "classLoader".equals(field.getName()) ) {
						targetClassLoaderObject = (JavaObject) field.get(this);
						break;
					}
				}
			}
		} catch (CorruptDataException e) {
			// This is only an optimisation to save us walking all class loaders. Continue
		} catch (MemoryAccessException e) {
			// This is only an optimisation to save us walking all class loaders. Continue
		}
		return getAssociatedClass(targetClassLoaderObject);
	}
	
	private JavaClass getAssociatedClass(JavaObject targetClassLoaderObject)
	{
		Iterator<?> classloaderIt = DTFJContext.getRuntime().getJavaClassLoaders();
		
		while (classloaderIt.hasNext()) {
			Object clObj = classloaderIt.next();
			
			if (clObj instanceof JavaClassLoader) {
				JavaClassLoader loader = (JavaClassLoader)clObj;
				try {
					if( targetClassLoaderObject != null && !loader.getObject().equals(targetClassLoaderObject) ) {
						continue;
					}
				} catch (CorruptDataException e1) {
					// This is an optimisation so if it fails, continue.
				}
				
				Iterator<?> classesIt = loader.getDefinedClasses();
				
				while (classesIt.hasNext()) {
					Object classObj = classesIt.next();
					
					if (classObj instanceof JavaClass) {
						JavaClass clazz = (JavaClass) classObj;
						
						try {
							if (clazz.getObject().equals(this)) {
								return clazz;
							}
						} catch (CorruptDataException e) {
							//Deliberately do nothing. If this is the class we're interesting in, we'll
							//insert a CorruptData object later when it isn't found. If it's not the class
							//we want, we don't need to worry
						}
					}
				}
			}
		}
		return null;
	}

	private void addFieldReferences(JavaClass jClass, int referentReachabilityType) throws CorruptDataException, MemoryAccessException
	{
		Iterator<?> fieldIt = jClass.getDeclaredFields();
		
		while (fieldIt.hasNext()) {
			Object fieldObj = fieldIt.next();
			
			if (fieldObj instanceof JavaField) {
				JavaField thisField = (JavaField)fieldObj;
				
				if ((thisField.getModifiers() & Modifier.STATIC) == 0) {
					String signature = thisField.getSignature();
					
					//From a reference point of view, only objects are interesting
					if (signature.startsWith("L") || signature.startsWith("[")) {
						Object targetObj = thisField.get(this);
						
						if (targetObj == null) {
							continue;
						}
						
						if (targetObj instanceof JavaObject) {
							String fieldName = thisField.getName();
							String declaringClassName = null;
							try {
								declaringClassName = thisField.getDeclaringClass().getName();
							} catch (DataUnavailable e) {
								// declaringClassName will be null, we will add this as a strong ref.
							}
							String description = "Object Reference [field name:" + fieldName + "]";
							
							// Reachability only applies to the referent field declared in java/lang/ref/Reference.
							// (Not any referent field declared in subclasses.)
							if (fieldName.equals("referent") &&
								"java/lang/ref/Reference".equals(declaringClassName)) {
								references.add(new DTFJJavaReference(this,targetObj,description,JavaReference.REFERENCE_FIELD,JavaReference.HEAP_ROOT_UNKNOWN,referentReachabilityType));
							} else {
								references.add(new DTFJJavaReference(this,targetObj,description,JavaReference.REFERENCE_FIELD,JavaReference.HEAP_ROOT_UNKNOWN,JavaReference.REACHABILITY_STRONG));
							}
						} else if (targetObj instanceof CorruptData) {
							references.add(targetObj);
						} else {
							references.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Unexpected type from field get: " + targetObj + ", class=" + targetObj.getClass().getName()));
						}
					}
				}
				
			} else if (fieldObj instanceof CorruptData) {
				references.add(fieldObj);
			} else {
				references.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Unexpected type from field iteration: " + fieldObj + ", class=" + fieldObj.getClass().getName()));
			}
		}
	}

	private void addObjectArrayReferences() throws CorruptDataException, MemoryAccessException
	{
		final int arraySize = this.getArraySize();

		// i.e. size of the array if all it contained were object headers
		long lowestPossibleSizeOfThisArray = arraySize * J9Object.SIZEOF;
		
		// Sanity check to see if the array size is at all legal (i.e. smaller than the whole heap).
		if (lowestPossibleSizeOfThisArray > DTFJJavaRuntimeHelper.getTotalHeapSize(DTFJContext.getRuntime(), DTFJContext.getProcess()) || lowestPossibleSizeOfThisArray < 0) {
			throw new CorruptDataException(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Array longer than available heap"));
		}
		
		final int ARRAY_SIZE_SAFETY_LIMIT = 1000000;
		
		if (arraySize > ARRAY_SIZE_SAFETY_LIMIT) {
			addObjectArrayReferencesWithLimitCheck(ARRAY_SIZE_SAFETY_LIMIT);
		} else {
			addAllObjectArrayReferences();
		}
	}
	
	/**
 	 * If the array is corrupt, it can report having some huge number of elements. This can lead to OOM (see defect 181042).
	 * To avoid that, try getting the first safetyLimit references - if they're all valid, then this is just a large object,
	 * so carry on. Otherwise, throw CorruptDataException.
	 * @param safetyLimit
	 * @throws CorruptDataException
	 * @throws MemoryAccessException
	 */
	private void addObjectArrayReferencesWithLimitCheck(final int safetyLimit) throws CorruptDataException, MemoryAccessException {
		JavaObject[] elements = new JavaObject[arraySize];
		JavaObject[] limitedElements = new JavaObject[safetyLimit];
		this.arraycopy(0, limitedElements, 0, safetyLimit); // also checks references are valid
		elements = new JavaObject[this.getArraySize() - safetyLimit];
		this.arraycopy(safetyLimit, elements, 0, this.getArraySize() - safetyLimit);

		// all went fine (no exception from arraycopy), so copy the results into the list
		for (int i=0; i!= limitedElements.length; i++) {
			JavaObject thisObj = limitedElements[i];
			if (thisObj != null) {
				references.add(new DTFJJavaReference(this,thisObj,"Array Reference [" + i + "]",JavaReference.REFERENCE_ARRAY_ELEMENT,JavaReference.HEAP_ROOT_UNKNOWN,JavaReference.REACHABILITY_STRONG));
			}
		}
		
		for (int i=0; i!= elements.length; i++) {
			JavaObject thisObj = elements[i];
			if (thisObj != null) {
				references.add(new DTFJJavaReference(this,thisObj,"Array Reference [" + i + "]",JavaReference.REFERENCE_ARRAY_ELEMENT,JavaReference.HEAP_ROOT_UNKNOWN,JavaReference.REACHABILITY_STRONG));
			}
		}
	}
	
	private void addAllObjectArrayReferences() throws CorruptDataException, MemoryAccessException {
		getReferences();
		JavaObject[] elements = new JavaObject[arraySize];
		this.arraycopy(0, elements, 0, arraySize);
		
		for (int i=0; i!= elements.length; i++) {
			JavaObject thisObj = elements[i];
			if (thisObj != null) {
				references.add(new DTFJJavaReference(this,thisObj,"Array Reference [" + i + "]",JavaReference.REFERENCE_ARRAY_ELEMENT,JavaReference.HEAP_ROOT_UNKNOWN,JavaReference.REACHABILITY_STRONG));
			}
		}
	}

	private boolean isObjectArray() throws CorruptDataException, com.ibm.j9ddr.CorruptDataException
	{
		return ((DTFJJavaClass)getJavaClass()).isObjectArray();
	}

	public Iterator getSections() {
		try {
			fetchDeferredData();
			LinkedList<ImageSection> sections = new LinkedList<ImageSection>();
			long mainSectionSize = ObjectModel.getConsumedSizeInBytesWithHeader(object).longValue();
			String name = getSectionName(mainSectionSize);
			J9DDRImageSection section = DTFJContext.getImageSection(object.getAddress(), name);
			section.setSize(mainSectionSize);
			sections.add(section);
			return sections.iterator();
		} catch (CorruptDataException e) {
			return corruptIterator(e.getCorruptData());
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return corruptIterator(cd);
		}
	}

	private String getSectionName(long mainSectionSize) {
		StringBuilder name = new StringBuilder();
		name.append("In-memory Object section at 0x");
		name.append(Long.toHexString(object.getAddress()));
		name.append(" (0x");
		name.append(Long.toHexString(mainSectionSize));
		name.append(" bytes)");
		return name.toString();
	}
	
	public long getSize() throws CorruptDataException {
		try {
			fetchDeferredData();
			return size;
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public boolean isArray() throws CorruptDataException {
		try {
			fetchDeferredData();
			return objectIsArray;
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	/**
	 * Get all the data that has been deferred until a property has been touched
	 */
	private void fetchDeferredData() throws CorruptDataException {
		if(corruptException != null) {				//if this object is marked as corrupt then re-throw the exception
			throw corruptException;
		}
		if(hasDeferredData) {
			hasDeferredData = false;				//we will only try and load the missing data once even if it fails
			try {
				objectIsArray = ObjectModel.isIndexable(object);
				if (objectIsArray) { // this is an array object, so cast to the underlying type
					arrayptr = J9ArrayClassPointer.cast(object.clazz());
					arraySize = ObjectModel.getSizeInElements(object).intValue();
				}
				size = ObjectModel.getTotalFootprintInBytesWithHeader(object).longValue();
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
	}

	@Override
	public String toString() {
		if(label == null) {
			try {
				StringBuilder text = new StringBuilder();
				text.append("Instance of ");
				String name = J9ObjectHelper.getClassName(object);
				text.append(name);
				if (objectIsArray && name.endsWith("L")) {
					text.append(J9ClassHelper.getName(arrayptr.leafComponentType())).append(";");
				}
				text.append(" @ 0x");
				text.append(Long.toHexString(object.getAddress()));
				label = text.toString();
			} catch (Throwable t) {
				// Note we catch but in this case do not re-throw
				J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
				label = super.toString();
			}
		}
		return label;
	}

	@Override
	public boolean equals(Object obj) {
		try {
			if (obj instanceof DTFJJavaObject) {
				DTFJJavaObject local = (DTFJJavaObject) obj;
				return object.equals(local.object);		//objects are equal if the underlying j9object pointer is the same
			}
		} catch (Throwable t) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
		return false;
	}

	@Override
	public int hashCode() {
		return object.hashCode();
	}
	
	public boolean isPacked() {
		return false; // 29 vm does not support packed
	}

	/*
	 * (non-Javadoc)
	 * Returns false if this object is java packed, or not packed.
	 * @see com.ibm.dtfj.java.JavaObject#isNativePacked()
	 */
	public boolean isNativePacked() {
		return false;
	}

	/*
	 * (non-Javadoc)
	 * Return 0 if this is not a native packed object;
	 * @see com.ibm.dtfj.java.JavaObject#getNativePackedDataPointer()
	 */
	public long getNativePackedDataPointer() {
		return 0;
	}

	/*
	 * (non-Javadoc)
	 * Returns false if this object is not a derived object or is not a packed object at all. 
	 * @see com.ibm.dtfj.java.JavaObject#isDerivedObject()
	 */
	public boolean isDerivedObject() {
		return false;
	}

	/*
	 * (non-Javadoc)
	 * Return null if this object is not a derived object or not a packed object at all.
	 * @see com.ibm.dtfj.java.JavaObject#getTargetJavaObject()
	 */
	public JavaObject getTargetJavaObject() {
		return null;
	}

	/*
	 * (non-Javadoc)
	 * Return 0 if this object is not a derived object or not a packed object at all. 
	 * @see com.ibm.dtfj.java.JavaObject#getTargetOffset()
	 */
	public long getTargetOffset() {
		return 0;
	}

	/*
	 * (non-Javadoc)
	 * Return false if this object is not a nested packed object or not a packed object at all.
	 * @see com.ibm.dtfj.java.JavaObject#isNestedPacked()
	 */
	public boolean isNestedPacked() {
		return false;
	}
}

