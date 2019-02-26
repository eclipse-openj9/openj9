/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
import static com.ibm.j9ddr.vm29.events.EventManager.register;
import static com.ibm.j9ddr.vm29.events.EventManager.unregister;
import static com.ibm.j9ddr.vm29.j9.OptInfo.getSourceFileNameForROMClass;

import java.lang.ref.SoftReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.view.dtfj.DTFJConstants;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.image.J9DDRCorruptData;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImagePointer;
import com.ibm.j9ddr.view.dtfj.java.helper.DTFJJavaClassHelper;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ArrayClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags;
import com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;
import com.ibm.j9ddr.vm29.view.dtfj.java.j9.DTFJConstantPoolIterator;
import com.ibm.j9ddr.vm29.structure.J9Object;

public class DTFJJavaClass implements JavaClass {
	private final J9ClassPointer j9class;
	private DTFJJavaClassloader javaClassLoader;
	private final static Logger log = DTFJContext.getLogger();
	private J9ClassPointer superclass = null;
	private Boolean isObjectArray = null;
	private Boolean isInterface = null;
	private Boolean isArray = null;
	private SoftReference<DTFJJavaObject> classObject = null;
	
	private final static int MAX_CLASS_FIELDS = 65535;
	private final static int MAX_CLASS_METHODS = 65535;
	
	public DTFJJavaClass(J9ClassPointer j9class) {
		this.j9class = j9class;
	}
	
	J9ClassPointer getJ9Class() {
		return j9class;
	}
	
	public JavaClassLoader getClassLoader() throws CorruptDataException {
		if (javaClassLoader == null) {
			try {
				javaClassLoader = new DTFJJavaClassloader(j9class.classLoader());
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		return javaClassLoader;
	}

	private DTFJJavaClass componentType;
	public JavaClass getComponentType() throws CorruptDataException {	
		if (!isArray()) {
			// This exception is required in in this circumstance by DTFJ API
			throw new IllegalArgumentException("Class is not an array");
		}

		J9ArrayClassPointer arrayClass = J9ArrayClassPointer.cast(j9class);
		try {
			if(arrayClass.arity().eq(1)) {
				if (componentType == null) {
					try {
						componentType = new DTFJJavaClass(J9ArrayClassPointer.cast(j9class).leafComponentType());
					} catch (com.ibm.j9ddr.CorruptDataException e) {
						J9DDRDTFJUtils.newCorruptDataException(DTFJContext.getProcess(), e);
					}
				}
				return componentType;
			} else {
				return new DTFJJavaClass(J9ArrayClassPointer.cast(j9class).componentType());
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	@SuppressWarnings("rawtypes")
	public Iterator getConstantPoolReferences() {
		try {
			if(isArray()) {		//array classes don't have constant pools
				return J9DDRDTFJUtils.emptyIterator();
			} else {
				return new DTFJConstantPoolIterator(j9class);
			}
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return corruptIterator(cd);
		}
	}

	static final Map<J9ClassPointer,List<Object>> declaredFieldsCache = new HashMap<J9ClassPointer,List<Object>>();
	
	@SuppressWarnings("rawtypes")
	public Iterator getDeclaredFields() {
		List<Object> list = declaredFieldsCache.get(j9class);
		
		if (list == null) {
			list = new LinkedList<Object>();
			final List<Object> fieldList = list; 
			
			IEventListener corruptDataListener =  new IEventListener() {
				
				public void corruptData(String message,
						com.ibm.j9ddr.CorruptDataException e, boolean fatal) {
					J9DDRCorruptData cd = J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e);
					fieldList.add(cd);
				}
			};
			try {
				register(corruptDataListener);

				J9ClassPointer superclass = J9ClassPointer.NULL;
				try {
					superclass = getSuperClassPointer();
				} catch (com.ibm.j9ddr.CorruptDataException e1) {
					//pass null for the superclass to just list this classes fields as cannot determine superclass
					fieldList.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Unable to determine superclass"));
				}

				U32 flags = new U32(J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE);
				long fieldCount = j9class.romClass().romFieldCount().longValue();
				Iterator<J9ObjectFieldOffset> fields = null;
				if( fieldCount > MAX_CLASS_FIELDS) {
					fieldList.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Corrupt field count"));
				} else {
					fields = J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor(j9class, superclass, flags);
				}
				while(fields != null && fields.hasNext()) {
					try {
						J9ObjectFieldOffset field = fields.next();
						log.fine(String.format("Declared field : %s", field.getName()));
						if(field.isStatic()) {
							fieldList.add(new DTFJJavaFieldStatic(this, field));
						} else {
							fieldList.add(new DTFJJavaFieldInstance(this, field));
						}
					} catch (com.ibm.j9ddr.CorruptDataException e) {
						fieldList.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e));	//add the corrupted field to the iterator
					}
					if ( fieldList.size() > fieldCount ) {
						// The class has returned more fields than it said it contained, it is corrupt.
						fieldList.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Corrupt class returned more fields than it declared"));	//add the corrupted field to the iterator
						break; 
					}
				}
				/* Large lists of fields are likely to be corrupt data so
				 * don't cache them.
				 */
				if( fieldList.size() < 1024 ) {
					declaredFieldsCache.put(j9class, fieldList);
				}
			} catch (Throwable t) {
				fieldList.add(J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t));
			} finally {
				unregister(corruptDataListener);
			}
		}
		return list.iterator();
	}

	@SuppressWarnings("rawtypes")
	public Iterator getDeclaredMethods() {
		ArrayList<Object> methods;
		J9MethodPointer ramMethod;
		long methodCount;
		try {
			ramMethod = j9class.ramMethods();
			methodCount = j9class.romClass().romMethodCount().longValue();
			if( methodCount > MAX_CLASS_METHODS ) {
				CorruptData cd = J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Corrupt class, maximum number of methods exceeded");
				return corruptIterator(cd);
			}
			methods = new ArrayList<Object>((int)methodCount);
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return corruptIterator(cd);
		} 
		for(int i = 0; i < methodCount; i++) {
			try {
				DTFJJavaMethod jmethod = new DTFJJavaMethod(this, ramMethod.add(i));
				methods.add(jmethod);
			} catch (Throwable t) {
				CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
				methods.add(cd);
			}
		}
		return methods.iterator();
	}

	private J9DDRImagePointer id;
	public ImagePointer getID() {
		if (id == null) {
			id = DTFJContext.getImagePointer(j9class.getAddress());
		}
		return id;
	}

	@SuppressWarnings("rawtypes")
	public Iterator getInterfaces() {
		ArrayList<Object> interfaceNames;
		int interfaceCount;
		SelfRelativePointer interfaceName;
		try {
			//a failure in the following calls mean that we cannot find any interfaces
			interfaceCount = j9class.romClass().interfaceCount().intValue();
			if(interfaceCount > 0xFFFF) {
				//class file format limits the number of interfaces to U2 (unsigned 2 bytes)
				String msg = String.format("Invalid number of interfaces for class@0x%s", Long.toHexString(j9class.getAddress()));
				throw new com.ibm.j9ddr.CorruptDataException(msg);
			}
			interfaceNames = new ArrayList<Object>(interfaceCount);
			interfaceName = j9class.romClass().interfaces();
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return corruptIterator(cd);
		}
		
		//a failure in the loop should return what we've found so far + a corrupt data
		try {
			for (int i = 0; i < interfaceCount; i++) {
				VoidPointer namePointer = interfaceName.add(i).get();
				//a failure to get the name means we can still move to the next iface 
				try {
					interfaceNames.add(J9UTF8Helper.stringValue(J9UTF8Pointer.cast(namePointer)));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					interfaceNames.add(cd);
				}
			}
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			interfaceNames.add(cd);
		}
		return interfaceNames.iterator();
	}

	static final Map<J9ClassPointer,Integer> modifiersCache = new HashMap<J9ClassPointer,Integer>();
	
	
	public int getModifiers() throws CorruptDataException {
		Integer cachedModifiers = modifiersCache.get(j9class);
		
		if (cachedModifiers == null) {
		
			try {
				cachedModifiers = J9ClassHelper.getJavaLangClassModifiers(j9class);
				modifiersCache.put(j9class, cachedModifiers);
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		
		return cachedModifiers;
	}


	public String getName() throws CorruptDataException {
		try {
			if (isArray()) {
				return J9ClassHelper.getArrayName(j9class);
			} else {
				return J9ClassHelper.getName(j9class);
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
	boolean isObjectArray() throws CorruptDataException, com.ibm.j9ddr.CorruptDataException
	{
		try {
			if(isObjectArray == null) {
				if (isArray()) {
					J9ArrayClassPointer arrayClass = J9ArrayClassPointer.cast(j9class);
					
					//Any multi-dimensional array is an object array, as is any single dimensional array with an object type (i.e. [Lwhatever;)
					if(arrayClass.arity().gt(1) || getName().charAt(1) == 'L') {
						isObjectArray = true;
					} else {
						isObjectArray = false;
					}
				} else {
					isObjectArray = false;
				}
			}
			return isObjectArray;
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
	public JavaObject getObject() throws CorruptDataException {
		DTFJJavaObject obj = null; 
		if( classObject != null ) {
			obj = classObject.get();
		}
		if( obj == null ) {
			try {
				obj = new DTFJJavaObject(j9class.classObject());
				classObject = new SoftReference<DTFJJavaObject> (obj);
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		return obj;
	}

	private List<Object> references = null;
	@SuppressWarnings("rawtypes")
	public Iterator getReferences() {
		if (references == null) {
			try {
			// 	need to build a list of references from this class.
				references = new ArrayList<Object>();
				//there are error handlers in each method which should handle corrupt data so only add one try..catch block in this method as a back stop
				addConstantPoolReferences(references);
				addStaticFieldReferences(references);
				addSuperclassReference(references);
				addClassLoaderReference(references);
				addClassObjectReference(references);
			} catch (Throwable t) {
				CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
				references.add(cd);
			}
		}
		return references.iterator();
			
	}

	@SuppressWarnings("rawtypes")
	private void addStaticFieldReferences(List<Object> references) {
		JavaReference jRef;
		Iterator declaredFieldIt = getDeclaredFields();
		Object obj = null;		//can get corrupt data returned through the field iterator as it's coming from DTFJ
		while ((declaredFieldIt.hasNext() && (obj = declaredFieldIt.next()) instanceof DTFJJavaField)) {
			DTFJJavaField jField = (DTFJJavaField) obj;
			if (jField instanceof DTFJJavaFieldStatic) {
				JavaObject jObject;
				try {
					char type = jField.getSignature().charAt(0);
					if (type == DTFJConstants.OBJECT_PREFIX_SIGNATURE || type == DTFJConstants.ARRAY_PREFIX_SIGNATURE) {
						jObject = (JavaObject) jField.get(null);
						if (jObject != null) {
							// build a JavaReference type and add the reference to the container.
							String fieldName = jField.getName();
							String description = "Static field";
							if (null != fieldName) {
								description  = description + " [field name:" + fieldName + "]";
							}
							jRef = new DTFJJavaReference(this, jObject, description, JavaReference.REFERENCE_STATIC_FIELD, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
							references.add(jRef);
						}
					}
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}
	}

	private void addSuperclassReference (List<Object> coll) {
		JavaReference jRef = null;

		try {
			JavaClass superClass = this.getSuperclass();
			if (null != superClass) {
				jRef = new DTFJJavaReference(this, superClass, "Superclass", JavaReference.REFERENCE_SUPERCLASS, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
				coll.add(jRef);
			}
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			coll.add(cd);
		}
	}

	private void addClassLoaderReference (List<Object> coll) {
		JavaReference jRef = null;

		try {
			JavaClassLoader classLoader = this.getClassLoader();
			if (null != classLoader) {
				JavaObject classLoaderObject = classLoader.getObject();
				if (null != classLoaderObject) {
					jRef = new DTFJJavaReference(this, classLoaderObject, "Classloader", JavaReference.REFERENCE_CLASS_LOADER, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
					coll.add(jRef);
				}
			}
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			coll.add(cd);
		}
	}
	
	private void addClassObjectReference (List<Object> coll) {
	    JavaReference jRef = null;
	    try {
	        com.ibm.dtfj.java.JavaObject classObject = this.getObject();
	    
	        if(null != classObject) {
	            jRef = new DTFJJavaReference(this,classObject,"Class object",JavaReference.REFERENCE_CLASS_OBJECT,JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
	            coll.add(jRef);
	        }
	    } catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			coll.add(cd);
		}
	}
	
	@SuppressWarnings("rawtypes")
	private JavaReference addConstantPoolReferences(List<Object> references) {
		// get the Constant Pool references from this class.
		JavaReference jRef = null;
		Iterator constantPoolIt = getConstantPoolReferences();
		try {
			while (constantPoolIt.hasNext()) {
				// get each reference in turn, note that the iterator can return JavaClass
				// JavaObject and CorruptData. The CorruptData objects are ignored.
				Object cpObject = constantPoolIt.next();
				if (cpObject instanceof JavaObject) {
					// add the reference to the container.
					jRef = new DTFJJavaReference(this, cpObject, "Constant Pool Object", JavaReference.REFERENCE_CONSTANT_POOL, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
				} else if (cpObject instanceof JavaClass) {
					// add the reference to the container.
					jRef = new DTFJJavaReference(this, cpObject, "Constant Pool Class", JavaReference.REFERENCE_CONSTANT_POOL, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
				}
				if (null != jRef) {
					references.add(jRef);
				}
			}
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			references.add(cd);
		}
		return jRef;
	}

	private J9ClassPointer getSuperClassPointer() throws com.ibm.j9ddr.CorruptDataException {
		if(superclass == null) {
			superclass = J9ClassHelper.superclass(j9class);
		}
		
		return superclass; 
	}
	
	//Data class to allow us to cache null superclasses
	private static class SuperClassCacheEntry
	{
		public SuperClassCacheEntry(JavaClass superClass)
		{
			this.superClass = superClass;
		}
		
		public final JavaClass superClass;
	}
	
	private static final Map<J9ClassPointer, SuperClassCacheEntry> superClassCache = new HashMap<J9ClassPointer,SuperClassCacheEntry>(); 
	
	public JavaClass getSuperclass() throws CorruptDataException {
		SuperClassCacheEntry cachedEntry = superClassCache.get(j9class);
		
		if (null == cachedEntry) {
			try {
				if(isInterface()) {
					cachedEntry = new SuperClassCacheEntry(null); //interfaces return a null superclass
				} else {				
					J9ClassPointer sclass = getSuperClassPointer();
					if(sclass == null) {
						cachedEntry = new SuperClassCacheEntry(null);
					} else {
						cachedEntry = new SuperClassCacheEntry(new DTFJJavaClass(sclass));
					}
				}
				
				superClassCache.put(j9class, cachedEntry);
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		
		if ( (cachedEntry.superClass != null) &&
			(cachedEntry.superClass instanceof DTFJJavaClass) &&
			((DTFJJavaClass)cachedEntry.superClass).getJ9Class().isNull()) {
			return null;
		}
		
		return cachedEntry.superClass;
	}

	private boolean isInterface() throws com.ibm.j9ddr.CorruptDataException {
		if(isInterface == null) {
			J9ROMClassPointer romclass = j9class.romClass();
			if(romclass.modifiers().allBitsIn(J9JavaAccessFlags.J9AccInterface)) {
				isInterface = true;
			} else {
				isInterface = false;
			}
		}
		return isInterface;
	}
	
	public boolean isArray() throws CorruptDataException {
		if (isArray == null) {
			try {
				isArray = J9ClassHelper.isArrayClass(j9class);
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		
		return isArray;
	}

	public boolean equals(Object obj) {
		if (obj != null && obj instanceof DTFJJavaClass) {
			DTFJJavaClass objImpl = (DTFJJavaClass) obj;
			return j9class.getAddress() == objImpl.j9class.getAddress();
		}

		return false;
	}

	public int hashCode() {
		return (int) j9class.getAddress();
	}
	
	public String getFilename() throws CorruptDataException
	{
		try {
			return getSourceFileNameForROMClass(j9class.romClass());
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public String toString()
	{
		try {
			return getName();
		} catch (CorruptDataException e) {
			return "JavaClass 0x" + Long.toHexString(j9class.getAddress()) + " (exception reading class name)";
		}
	}
	
	public long getInstanceSize() throws CorruptDataException
	{
		try {
			return j9class.totalInstanceSize().longValue() + J9Object.SIZEOF;
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
	public JavaObject getProtectionDomain() throws DataUnavailable,	CorruptDataException {
		try {
			return DTFJJavaClassHelper.getProtectionDomain(this);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
	public boolean isPacked() {
		return false; // 29 vm does not support packed
	}
	
	public long getPackedDataSize() throws DataUnavailable, CorruptDataException {
		return 0; // 29 vm does not support packed
	}
	
}
