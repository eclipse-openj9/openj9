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
import com.ibm.j9ddr.vm29.j9.ObjectModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.NoSuchElementException;
import java.util.Properties;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaVMInitArgs;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.util.IteratorHelpers;
import com.ibm.j9ddr.view.dtfj.DTFJCorruptDataException;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.image.J9DDRCorruptData;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageSection;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.RootScanner;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadListIterator;
import com.ibm.j9ddr.vm29.j9.walkers.J9MemTagIterator;
import com.ibm.j9ddr.vm29.j9.walkers.MemoryCategoryIterator;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITConfigPointer;
import com.ibm.j9ddr.vm29.pointer.generated.OMRMemCategoryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadAbstractMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemorySpacePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9JavaVMHelper;
import com.ibm.j9ddr.vm29.structure.J9JITConfig;
import com.ibm.j9ddr.vm29.structure.J9JavaVM;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;
import com.ibm.j9ddr.vm29.view.dtfj.java.corrupt.AddCorruptionToListListener;
import com.ibm.j9ddr.vm29.view.dtfj.java.j9.DTFJMonitorIterator;

public class DTFJJavaRuntime implements JavaRuntime {

	private final DTFJJavaVMInitArgs vminitargs;
	private List<Object> references;
	private Properties systemProperties;
	private LinkedList<Object> classLoaders = null;  // use linked list for predictable ordering; polite to clients and debug-friendly
	private List<ImageSection> mergedHeapSections = null;
	
	public DTFJJavaRuntime() {
		vminitargs = new DTFJJavaVMInitArgs();
	}
	
	@Override
	public boolean equals(Object other) {
		
		if ((other instanceof JavaRuntime) == false)
			return false;
		
		try {
			return getJavaVM().equals(((JavaRuntime)other).getJavaVM());
		} catch (CorruptDataException e) {
			return false;
		}
	}
	
	@Override
	public int hashCode() {
		try {
			return getJavaVM().hashCode();
		} catch (CorruptDataException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		return 0;
	}
	
	private List<Object> compiledMethods = null;
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public Iterator getCompiledMethods() {
		if (compiledMethods == null) {
			compiledMethods = new ArrayList<Object>();
			Iterator<Object> classLoaderIterators = getJavaClassLoaders();
			while (classLoaderIterators.hasNext()) {
				Object classLoaderObj = classLoaderIterators.next();
				if (classLoaderObj instanceof CorruptData) {
					compiledMethods.add(classLoaderObj);
				} else {
					Iterator classesIterator = ((JavaClassLoader) classLoaderObj).getDefinedClasses();
					while (classesIterator.hasNext()) {
						Object classObject = classesIterator.next();
						if (classObject instanceof CorruptData) {
							compiledMethods.add(classObject);
						} else {
							Iterator methodsIterator = ((JavaClass) classObject).getDeclaredMethods();
							while (methodsIterator.hasNext()) {
								Object methodObj = methodsIterator.next();
								if (methodObj instanceof CorruptData) {
									compiledMethods.add(methodObj);
								}
								if (((JavaMethod) methodObj).getCompiledSections().hasNext()) {
									compiledMethods.add(methodObj);
								}
							}
						}
					}
				}
			}
		}
		
		return compiledMethods.iterator();
	}

	@SuppressWarnings("rawtypes")
	public Iterator getHeapRoots() {
		if (null == references) {
			scanReferences();
		}
		
		return references.iterator();
	}
	
	public boolean isJITEnabled() throws CorruptDataException {
		try {
			J9JITConfigPointer jitConfig = DTFJContext.getVm().jitConfig();
			return jitConfig.notNull() && jitConfig.runtimeFlags().allBitsIn(J9JITConfig.J9JIT_JIT_ATTACHED);
			
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
	public Properties getJITProperties() throws DataUnavailable, CorruptDataException {
		Properties properties = new Properties();
		try {
			J9JITConfigPointer jitConfig = DTFJContext.getVm().jitConfig();
			
			if (jitConfig.notNull()) {
				if (jitConfig.runtimeFlags().allBitsIn(J9JITConfig.J9JIT_JIT_ATTACHED)) {
					properties.setProperty("JIT", "enabled");
				} else {
					properties.setProperty("JIT", "disabled");
				}
				
				if (jitConfig.runtimeFlags().allBitsIn(J9JITConfig.J9JIT_AOT_ATTACHED)) {
					properties.setProperty("AOT", "enabled");
				} else {
					properties.setProperty("AOT", "disabled");
				}
				
				if (!jitConfig.fsdEnabled().eq(new UDATA(0))) {
					properties.setProperty("FSD", "enabled");
				} else {
					properties.setProperty("FSD", "disabled");
				}
				
				if (DTFJContext.getVm().requiredDebugAttributes().allBitsIn(J9JavaVM.J9VM_DEBUG_ATTRIBUTE_CAN_REDEFINE_CLASSES)) {
					properties.setProperty("HCR", "enabled");
				} else {
					properties.setProperty("HCR", "disabled");
				}
			} else {
				throw new DataUnavailable("JIT not enabled");
			}
			return properties;
			
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
	private void scanReferences()
	{
		references = new LinkedList<Object>();
		AddCorruptionToListListener corruptionListener = new AddCorruptionToListListener(references);
		register(corruptionListener);
		
		try {
			RootScanner scanner = new DTFJRootScanner();
			/* We don't report stack slots here */
			scanner.setScanStackSlots(false);
		
			scanner.scanAllSlots();
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			references.add(cd);
		}
		unregister(corruptionListener);
	}

	public class DTFJRootScanner extends RootScanner
	{
		DTFJRootScanner() throws com.ibm.j9ddr.CorruptDataException 
		{
			super();
		}

		private int getReachabilityCode()
		{
			switch (this.getReachability()) {
			case STRONG:
				return JavaReference.REACHABILITY_STRONG;
			case WEAK:
				return JavaReference.REACHABILITY_WEAK;
			default:
				throw new UnsupportedOperationException("Unknown reachability: " + this.getReachability() + " found");
			}
		}
		
		@Override
		protected void doClassLoader(J9ClassLoaderPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
						new DTFJJavaClassloader(slot).getObject(),
						"ClassLoader",
						JavaReference.REFERENCE_UNKNOWN,
						JavaReference.HEAP_ROOT_CLASSLOADER, 
						getReachabilityCode()));
				} catch (CorruptDataException e) {
					references.add(e.getCorruptData());
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doClassSlot(J9ClassPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
						new DTFJJavaClass(slot),
						"Class",
						JavaReference.REFERENCE_UNKNOWN,
						JavaReference.HEAP_ROOT_SYSTEM_CLASS, 
						getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}
		
		@Override
		protected void doFinalizableObject(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"FinalizableObject",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_FINALIZABLE_OBJ, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doJNIGlobalReferenceSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
						new DTFJJavaObject(slot),
						"JNIGlobalReference",
						JavaReference.REFERENCE_UNKNOWN,
						JavaReference.HEAP_ROOT_JNI_GLOBAL, 
						getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doJNIWeakGlobalReference(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
						new DTFJJavaObject(slot),
						"JNIWeakGlobalReference",
						JavaReference.REFERENCE_UNKNOWN,
						JavaReference.HEAP_ROOT_JNI_GLOBAL, 
						getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doJVMTIObjectTagSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
						new DTFJJavaObject(slot),
						"JVMTIObjectTagTable",
						JavaReference.REFERENCE_UNKNOWN,
						JavaReference.HEAP_ROOT_UNKNOWN, 
						getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doMonitorReference(J9ObjectMonitorPointer slot)
		{
			if (slot.notNull()) {
				try {
					J9ThreadAbstractMonitorPointer monitor = J9ThreadAbstractMonitorPointer.cast(slot.monitor());
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(J9ObjectPointer.cast(monitor.userData())),
							"MonitorReference",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_MONITOR, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doPhantomReferenceSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"PhantomReferenceObject",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_OTHER, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doRememberedSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"RememberedSlot",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_OTHER, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doSoftReferenceSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"SoftReferenceObject",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_OTHER, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doStringTableSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
				references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
						new DTFJJavaObject(slot),
						"StringTable",
						JavaReference.REFERENCE_UNKNOWN,
						JavaReference.HEAP_ROOT_STRINGTABLE, 
						getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}
		
		@Override
		protected void doStringCacheTableSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
						new DTFJJavaObject(slot),
						"StringCacheTable",
						JavaReference.REFERENCE_UNKNOWN,
						JavaReference.HEAP_ROOT_STRINGTABLE, 
						getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doUnfinalizedObject(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"UnfinalizedObject",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_UNFINALIZED_OBJ, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doVMClassSlot(J9ClassPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaClass(slot),
							"VMClassSlot",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_SYSTEM_CLASS, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doVMThreadJNISlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"Thread",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_THREAD, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doVMThreadMonitorRecordSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"Thread",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_THREAD, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}
		protected void doNonCollectableObjectSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"Reference from an immortal/scoped region",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_OTHER, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}
		
		@Override
		protected void doMemorySpaceSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"MemorySpace reference to Java MemoryArea object",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_OTHER, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}
		

		@Override
		protected void doVMThreadSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"Thread",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_THREAD, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doWeakReferenceSlot(J9ObjectPointer slot)
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
						new DTFJJavaObject(slot),
						"WeakReferenceObject",
						JavaReference.REFERENCE_UNKNOWN,
						JavaReference.HEAP_ROOT_OTHER, 
						getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doClass(J9ClassPointer clazz)
		{
			doClassSlot(clazz);
		}
		
		@Override
		protected void doStackSlot(J9ObjectPointer slot)
		{
			/* Do nothing, we don't report stack slots here */
		}

		@Override
		protected void doOwnableSynchronizerObject(J9ObjectPointer slot) 
		{
			if (slot.notNull()) {
				try {
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(slot),
							"OwnableSynchronizerObject",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_OTHER, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}

		@Override
		protected void doMonitorLookupCacheSlot(J9ObjectMonitorPointer slot) 
		{
			if (slot.notNull()) {
				try {
					J9ThreadAbstractMonitorPointer monitor = J9ThreadAbstractMonitorPointer.cast(slot.monitor());
					references.add(new DTFJJavaReference(DTFJJavaRuntime.this,
							new DTFJJavaObject(J9ObjectPointer.cast(monitor.userData())),
							"MonitorReference",
							JavaReference.REFERENCE_UNKNOWN,
							JavaReference.HEAP_ROOT_MONITOR, 
							getReachabilityCode()));
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					references.add(cd);
				}
			}
		}
	}

	@SuppressWarnings("rawtypes")
	public Iterator getHeaps() throws UnsupportedOperationException {
		try {
			LinkedList<Object> heaps = new LinkedList<Object>();

			VoidPointer memorySpace = DTFJContext.getVm().defaultMemorySpace();
			MM_MemorySpacePointer defaultMemorySpace = MM_MemorySpacePointer.cast(memorySpace);
			U8Pointer namePtr = defaultMemorySpace._name();
			String name = "No name"; //MEMORY_SPACE_NAME_UNDEFINED
			if (namePtr != null && !namePtr.isNull()) {
				try {
					name = namePtr.getCStringAtOffset(0);
				} catch (com.ibm.j9ddr.CorruptDataException e) {
					name = "<<corrupt heap name>>";
				}
			}
			heaps.add(new DTFJJavaHeap(defaultMemorySpace, name, DTFJContext.getImagePointer(memorySpace.getAddress())));

			return heaps.iterator();
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return corruptIterator(cd);
		}
	}

	@SuppressWarnings("rawtypes")
	public Iterator getJavaClassLoaders() {
		if (classLoaders != null) {
			return classLoaders.iterator(); // return cached set of class loaders
		} 
		classLoaders = new LinkedList<Object>();
		GCClassLoaderIterator classLoaderIterator;
		try {
			classLoaderIterator = GCClassLoaderIterator.from();
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			classLoaders.add(cd);
			return classLoaders.iterator();
		}
		
		AddCorruptionToListListener corruptionListener = new AddCorruptionToListListener(classLoaders);
		register(corruptionListener);
		try {
			while(!corruptionListener.fatalCorruption() && classLoaderIterator.hasNext() ) {
				J9ClassLoaderPointer next =  classLoaderIterator.next();
				if( next != null ) {
					classLoaders.add(new DTFJJavaClassloader(next));
				}
			}
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			classLoaders.add(cd);
		}	
		unregister(corruptionListener);
		return classLoaders.iterator();
	}

	public ImagePointer getJavaVM() throws CorruptDataException {
		try {
			return DTFJContext.getImagePointer(DTFJContext.getVm().getAddress());
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public JavaVMInitArgs getJavaVMInitArgs() throws DataUnavailable, CorruptDataException {
		return vminitargs;
	}

	@SuppressWarnings("rawtypes")
	public Iterator getMonitors() {
		try {
			return new DTFJMonitorIterator();
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return corruptIterator(cd);
		}
	}

	public JavaObject getObjectAtAddress(ImagePointer address) throws CorruptDataException, IllegalArgumentException, MemoryAccessException, DataUnavailable {
		try {
			J9ObjectPointer object = J9ObjectPointer.cast(address.getAddress());
			
			//Check the address is within a known segment and is aligned properly
			if(object.anyBitsIn(ObjectModel.getObjectAlignmentInBytes() - 1)) {
				throw new IllegalArgumentException("Invalid alignment for JavaObject. Address = " + address);
			}
			
			if( validHeapAddress(address)) {
				// DTFJJavaObject will lazily initialize its heap parameter if DTFJJavaObject.getHeap()
				// is called.
				return new DTFJJavaObject(null, object);
			} else {
				throw new IllegalArgumentException("Object address " + address + " is not in any heap");			
			}
		} catch (Throwable t) {
			Class<?>[] whitelist = new Class<?>[]{IllegalArgumentException.class}; // white list can only contain RTEs
			throw J9DDRDTFJUtils.handleAllButMemAccExAndDataUnavailAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}
	
	/*
	 * Merge all the sections of the heap together to simplify range checks for 
	 * getObjectAtAddress. (This is very important with balanced heaps where there
	 * are thousands of heap sections but they are actually contiguous. See PR 103197)
	 */
	private synchronized void mergeSections() {
		// Get the list of all the heap sections.
		if( mergedHeapSections != null ) {
			return;
		}
		
		Iterator heaps = getHeaps();
		List<ImageSection> heapSections = new LinkedList<ImageSection>();

		// If there are no heap sections this can't be valid.
		if (!heaps.hasNext()) {
			return;
		}
		while (heaps.hasNext()) {
			DTFJJavaHeap heap = (DTFJJavaHeap) heaps.next();
			Iterator sections = heap.getSections();
			while (sections.hasNext()) {
				heapSections.add((ImageSection) sections.next());
			}
		}
		// Sort them.
		Collections.sort(heapSections, new Comparator<ImageSection>() {
			public int compare(ImageSection arg0, ImageSection arg1) {
				U64 ptr0 = new U64(arg0.getBaseAddress().getAddress());
				U64 ptr1 = new U64(arg1.getBaseAddress().getAddress());
				// Addresses are actually unsigned so use the unsigned 64 bit comparison
				// not a java long comparison.
				if (ptr0.lt(ptr1)) {
					return -1;
				} else if (ptr0.gt(ptr1)) {
					return 1;
				} else {
					return 0;
				}
			}
		});

		mergedHeapSections = new LinkedList<ImageSection>();
		Iterator<ImageSection> itr = heapSections.iterator();
		// We know we have at least one section.
		ImageSection currentSection = itr.next();
		while (itr.hasNext()) {
			ImageSection nextSection = itr.next();
			// If the sections are contiguous, merge them.
			if (currentSection.getBaseAddress().getAddress()
					+ currentSection.getSize() == nextSection.getBaseAddress()
					.getAddress()) {
				currentSection = new J9DDRImageSection(
						DTFJContext.getProcess(),
						currentSection.getBaseAddress().getAddress(),
						currentSection.getSize() + nextSection.getSize(), null);
				
				
			} else {
				mergedHeapSections.add(currentSection);
				currentSection = nextSection;
			}
		}
		mergedHeapSections.add(currentSection);
	}
	
	/*
	 * Quickly check if an object address is within a memory range that we
	 * know is part of the heap.
	 * (Heap sections are usually contiguous so we can merge them down to
	 * just a few ranges. This is important in balanced mode where there
	 * may be thousands. See See PR 103197)
	 */
	private boolean validHeapAddress(ImagePointer address) {

		if( mergedHeapSections == null ) {
			mergeSections();
		}
		
		U64 addr = new U64(address.getAddress());
		
		for( ImageSection i: mergedHeapSections ) {
			U64 baseAddress = new U64( i.getBaseAddress().getAddress() );
			if( baseAddress.gt(addr) ) {
				// Sections are sorted by base address so this implies we haven't
				// found the pointer in a heap.
				return false;
			} 
			U64 endAddress = new U64( i.getBaseAddress().getAddress() + i.getSize() );
			if( endAddress.gt( addr ) ) {
				return true;
			}
		}
		return false;
	}
	
	/* 
	 * Returns the JavaHeap which contains the passed-in address
	 * Returns null if no containing JavaHeap is found
	 */
	@SuppressWarnings("rawtypes")
	public DTFJJavaHeap getHeapFromAddress(ImagePointer address)
	{
		try {
			VoidPointer pointer = VoidPointer.cast(address.getAddress());
			
			Iterator heapsIterator = getHeaps();
			while(heapsIterator.hasNext()) {
				DTFJJavaHeap heap = (DTFJJavaHeap) heapsIterator.next();
				Iterator sectionsIterator = heap.getSections();
				while(sectionsIterator.hasNext()) {
					ImageSection section = (ImageSection) sectionsIterator.next();
					VoidPointer base = VoidPointer.cast(section.getBaseAddress().getAddress());
					VoidPointer top = base.addOffset(section.getSize());
					if(pointer.gte(base) && pointer.lt(top)) {
						// Object is within this section
						return heap;
					}
				}
			}
		} catch (Throwable t) {
			//just log exceptions and allow it to fall through
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
		
		return null;
	}

	@SuppressWarnings("rawtypes")
	public Iterator getThreads() {
		GCVMThreadListIterator threadIterator;
		try {
			threadIterator = GCVMThreadListIterator.from();
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return corruptIterator(cd);
		}
		
		List<Object> toIterate = new LinkedList<Object>();
		AddCorruptionToListListener listener = new AddCorruptionToListListener(toIterate);
		register(listener);
		try {
			while (threadIterator.hasNext() && !listener.fatalCorruption()) {
				J9VMThreadPointer next = threadIterator.next();
				if( next != null ) {
					toIterate.add(new DTFJJavaThread(next));
				}
			}
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			toIterate.add(cd);
		}
		unregister(listener);
		
		return toIterate.iterator();
	}

	public Object getTraceBuffer(String arg0, boolean arg1) throws CorruptDataException {
		return new J9DDRCorruptData(DTFJContext.getProcess(),"Trace buffers are not available");
	}

	public String getFullVersion() throws CorruptDataException {
		return getVersion();
	}

	public String getVersion() throws CorruptDataException {
		try {
			//starting with 26 stream, RAS structure contains the JRE version which avoids the potential pitfall
			//of using system properties which may have their name and/or value stored in native libraries
			return DTFJContext.getVm().j9ras().serviceLevel().getCStringAtOffset(0);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	/*
	 * This method is no longer used for getting the version but the requirement to be able to get
	 * system properties is an omission from the DTFJ API which will be rectified in the future, at
	 * which point this method will form the core of that functionality.
	 */
	@SuppressWarnings("unused")
	private String getRequiredSystemProperty(String name) throws CorruptDataException
	{
		String value = null;
		SystemPropertiesEventListener listener = new SystemPropertiesEventListener();
		register(listener);
		try {
			value = getSystemProperties().getProperty(name);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		} finally {
			unregister(listener);
		}
		
		if (value != null) {
			return value;
		} else {
			if( listener.corruption ) {
				throw new DTFJCorruptDataException(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), listener.exception));
			} else {
				throw new DTFJCorruptDataException(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Couldn't find required system property " + name));
			}
		}
	}
	
	private class SystemPropertiesEventListener implements IEventListener {

		protected boolean corruption = false;
		protected com.ibm.j9ddr.CorruptDataException exception;
		
		public void corruptData(String message,
				com.ibm.j9ddr.CorruptDataException e, boolean fatal) {
			corruption = true;
			this.exception = e;
		}
		
	}
	
	private Properties getSystemProperties() throws com.ibm.j9ddr.CorruptDataException
	{
		if (systemProperties == null) {
			systemProperties = J9JavaVMHelper.getSystemProperties(DTFJContext.getVm());
		}
		
		return systemProperties;
	}

	private String getServiceLevel() {
		try {
			U8Pointer serviceLevelPointer = DataType.getJ9RASPointer().serviceLevel();
			String serviceLevel = serviceLevelPointer.getCStringAtOffset(0);
			if( serviceLevel != null && serviceLevel.length() > 0 ) {
				return serviceLevel;
			}
		} catch (Throwable t) {
			// If we can't access the memory then we'll have to go with "unknown".
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
		
		return "unknown";
	}
	
	@Override
	public String toString() {
		return "Java Runtime 0x" + Long.toHexString(DTFJContext.getVm().getAddress());
	}

	public Iterator<?> getMemoryCategories() throws DataUnavailable
	{
		final List<Object> returnList = new LinkedList<Object>();
		
		IEventListener eventListener = new IEventListener(){

			public void corruptData(String message,
					com.ibm.j9ddr.CorruptDataException e, boolean fatal)
			{
				returnList.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e));
			}};
		
		EventManager.register(eventListener);
		try {
			Iterator<? extends OMRMemCategoryPointer> rootCategories = MemoryCategoryIterator.iterateCategoryRootSet(DTFJContext.getVm().portLibrary());
			
			while (rootCategories.hasNext()) {
				returnList.add(new DTFJJavaRuntimeMemoryCategory(this, rootCategories.next()));
			}
		
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			returnList.add(cd);
		} finally {
			EventManager.unregister(eventListener);
		}
		
		return Collections.unmodifiableList(returnList).iterator();
	}

	@SuppressWarnings("unchecked")
	public Iterator<?> getMemorySections(boolean includeFreed)
			throws DataUnavailable
	{
		if (includeFreed) {
			return IteratorHelpers.combineIterators(
					new MemTagMemorySectionIterator(J9MemTagIterator.iterateAllocatedHeaders()),
					new MemTagMemorySectionIterator(J9MemTagIterator.iterateFreedHeaders()),
					getNonMallocMemorySections()
					);
		} else {
			return IteratorHelpers.combineIterators(
					new MemTagMemorySectionIterator(J9MemTagIterator.iterateAllocatedHeaders()),
					getNonMallocMemorySections()
					);
		}
	}

	/**
	 * 
	 * @return Iterator of VM non-malloc memory sections
	 */
	private static Iterator<Object> getNonMallocMemorySections()
	{
		List<Object> list = new LinkedList<Object>();
		
		//TODO Fill in heap sections
		
		return list.iterator();
	}
	
	/**
	 * Iterator of JavaRuntimeMemorySections seeded with a J9MemTagIterator
	 *
	 */
	private class MemTagMemorySectionIterator implements Iterator<Object>
	{
		private final J9MemTagIterator memTagIt;
		
		private final List<Object> buffer = new ArrayList<Object>(2);
		
		private final IEventListener eventListener = new IEventListener(){

			public void corruptData(String message,
					com.ibm.j9ddr.CorruptDataException e, boolean fatal)
			{
				buffer.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e));
			}};
		
		MemTagMemorySectionIterator(J9MemTagIterator it)
		{
			memTagIt = it;
		}
		
		public boolean hasNext()
		{
			if (buffer.size() > 0) {
				return true;
			}
			
			EventManager.register(eventListener);
			try {
				if (memTagIt.hasNext()) {
					buffer.add(new DTFJMemoryTagRuntimeMemorySection(DTFJJavaRuntime.this, memTagIt.next()));
					return true;
				} else {
					return false;
				}
			} catch (Throwable t) {
				CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
				buffer.add(cd);
				return true;
			} finally {
				EventManager.unregister(eventListener);
			}
		}

		public Object next()
		{
			if (hasNext()) {
				return buffer.remove(0);
			} else {
				throw new NoSuchElementException();
			}
		}

		public void remove()
		{
			throw new UnsupportedOperationException();
		}
		
	}
	
	public JavaObject getNestedPackedObject(JavaClass jc, ImagePointer packedDataAddress) 
			throws DataUnavailable {
		throw new DataUnavailable("Not implemented");
	}

	public JavaObject getNestedPackedArrayObject(JavaClass jc, ImagePointer i, int arrayLength) 
			throws DataUnavailable {
		throw new DataUnavailable("Not implemented");
	}

	/**
	 * Return the JVM start time.
	 * 
	 * @return long - JVM start time (milliseconds since 1970)
	 */
	public long getStartTime() throws DataUnavailable, CorruptDataException {
		try {
			return DTFJContext.getVm().j9ras().startTimeMillis().longValue();
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
	/**
	 * Return the value of the system nanotime (high resolution timer) at JVM start.
	 * 
	 * @return long - system nanotime at JVM start
	 */
	public long getStartTimeNanos() throws DataUnavailable, CorruptDataException {
		try {
			return DTFJContext.getVm().j9ras().startTimeNanos().longValue();
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
}
