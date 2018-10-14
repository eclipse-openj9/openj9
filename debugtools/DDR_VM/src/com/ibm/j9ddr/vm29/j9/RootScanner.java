/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.gc.GCClassHeapIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCFinalizableObjectIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCJNIGlobalReferenceIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCJNIWeakGlobalReferenceIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCJVMTIObjectTagTableIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCJVMTIObjectTagTableListIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCMonitorReferenceIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectHeapIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCOwnableSynchronizerObjectListIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCRememberedSetIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCRememberedSetSlotIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCSegmentIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCStringCacheTableIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCStringTableIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCUnfinalizedObjectListIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCVMClassSlotIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadJNISlotIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadListIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadMonitorRecordSlotIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadSlotIterator;
import com.ibm.j9ddr.vm29.j9.stackwalker.FrameCallbackResult;
import com.ibm.j9ddr.vm29.j9.stackwalker.IStackWalkerCallbacks;
import com.ibm.j9ddr.vm29.j9.stackwalker.WalkState;
import com.ibm.j9ddr.vm29.pointer.ObjectMonitorReferencePointer;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JVMTIDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JVMTIEnvPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_SublistPuddlePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.structure.J9MemorySegment;
import com.ibm.j9ddr.vm29.structure.J9VMThread;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.structure.MM_GCExtensions$DynamicClassUnloading;
import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;

public abstract class RootScanner
{
	private Reachability _reachability = Reachability.STRONG;
	
	public static enum Reachability {STRONG, WEAK};

	private J9JavaVMPointer _vm;
	private MM_GCExtensionsPointer _extensions;
	
	private boolean _classDataAsRoots = true; /**< Should all classes (and class loaders) be treated as roots. Default true, should set to false when class unloading */
	private boolean _includeJVMTIObjectTagTables = true; /**< Should the iterator include the JVMTIObjectTagTables. Default true, should set to false when doing JVMTI object walks */
	private boolean _includeStackFrameClassReferences = true; /**< Should the iterator include Classes which have a method running on the stack */
	private boolean _trackVisibleStackFrameDepth = false; /**< Should the stack walker be told to track the visible frame depth. Default false, should set to true when doing JVMTI walks that report stack slots */
	private boolean _scanStackSlots = true; /**< Should we scan stacks. */
	private boolean _stringTableAsRoot = true; /**< Treat the string table as a hard root */
	private boolean _nurseryReferencesOnly = false;  /**< Should the iterator only scan structures that currently contain nursery references */
	private boolean _nurseryReferencesPossibly = false;  /**< Should the iterator only scan structures that may contain nursery references */
	private boolean _includeRememberedSetReferences; /**< Should the iterator include references in the Remembered Set (if applicable) */
	private RootScanner.RootScannerStackWalkerCallbacks _stackWalkerCallbacks = new RootScannerStackWalkerCallbacks();
	
	protected RootScanner() throws CorruptDataException 
	{
		_vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		_extensions = GCExtensions.getGCExtensionsPointer();
		_includeRememberedSetReferences = _extensions.scavengerEnabled() ? true : false;
		/* Doesn't match up with C code exactly, but this seems to make sensible defaults as most GC cycles set things up this way */
		_stringTableAsRoot = !_extensions.collectStringConstants();
		if (J9BuildFlags.gc_dynamicClassUnloading) {
			_classDataAsRoots = MM_GCExtensions$DynamicClassUnloading.DYNAMIC_CLASS_UNLOADING_NEVER == _extensions.dynamicClassUnloading();
		} else {
			_classDataAsRoots = true;
		}
	}
	
	private class RootScannerStackWalkerCallbacks implements IStackWalkerCallbacks
	{
		public FrameCallbackResult frameWalkFunction(J9VMThreadPointer walkThread, WalkState walkState)
		{
			return FrameCallbackResult.KEEP_ITERATING;
		}
	
		public void objectSlotWalkFunction(J9VMThreadPointer walkThread, WalkState walkState, PointerPointer objectSlot, VoidPointer stackLocation)
		{	
			try {
				J9ObjectPointer object = J9ObjectPointer.cast(objectSlot.at(0));
				if (object.notNull()) {
					RootScanner.this.doStackSlot(object, walkState, stackLocation);
				}
			} catch (CorruptDataException e) {
				EventManager.raiseCorruptDataEvent("Corrupt stack object slot detected", e, false);
			}
		}

		public void fieldSlotWalkFunction(J9VMThreadPointer walkThread,
				WalkState walkState, ObjectReferencePointer objectSlot,
				VoidPointer stackLocation)
		{
			try {
				J9ObjectPointer object = objectSlot.at(0);
				if (object.notNull()) {
					RootScanner.this.doStackSlot(object, walkState, stackLocation);
				}
			} catch (CorruptDataException e) {
				EventManager.raiseCorruptDataEvent("Corrupt stack object slot detected", e, false);
			}
		}
	}
		
	public void setStringTableAsRoot(boolean stringTableAsRoot) 
	{
		_stringTableAsRoot = stringTableAsRoot;
	}
	
	public void setNurseryReferencesOnly(boolean nurseryReferencesOnly)
	{
		if (J9BuildFlags.gc_modronScavenger) {
			throw new UnsupportedOperationException("Not supported with non-scavenger based garbage collection");
		}
		_nurseryReferencesOnly = nurseryReferencesOnly;
	}
	
	public void setNurseryReferencesPossibly(boolean nurseryReferencesPossibly)
	{
		if (J9BuildFlags.gc_modronScavenger) {
			throw new UnsupportedOperationException("Not supported with non-scavenger based garbage collection");
		}
		_nurseryReferencesPossibly = nurseryReferencesPossibly;
	}
	
	public void setIncludeRememberedSetReferences(boolean includeRememberedSetReferences)
	{
		if (J9BuildFlags.gc_modronScavenger) {
			throw new UnsupportedOperationException("Not supported with non-scavenger based garbage collection");
		}
		_includeRememberedSetReferences = includeRememberedSetReferences;
	}
	
	public void setIncludeStackFrameClassReferences(boolean includeStackFrameClassReferences)
	{
		_includeStackFrameClassReferences = includeStackFrameClassReferences;
	}
	
	public void setClassDataAsRoots(boolean classDataAsRoots)
	{
		if (J9BuildFlags.gc_dynamicClassUnloading) {
			throw new UnsupportedOperationException("Not supoprted without dynamic class unloading");
		}
		_classDataAsRoots = classDataAsRoots;
	}
	
	public void setTrackVisibleStackFrameDepth(boolean trackVisibleStackFrameDepth) 
	{
		_trackVisibleStackFrameDepth = trackVisibleStackFrameDepth;
	}
	
	public void setScanStackSlots(boolean scanStackSlots)
	{
		_scanStackSlots = scanStackSlots;
	}
	
	/* In MM_RootScanner all the doXXXSlot methods have a default implementation
	 * that simply calls doSlot 
	 */
	/* General object slot handler to be reimplemented by specializing class. 
	 * This handler is called for every reference to a J9Object. */
	//protected abstract void doSlot(J9ObjectPointer slot);

	/* General class slot handler to be reimplemented by specializing class. 
	 * This handler is called for every reference to a J9Class. */
	protected abstract void doClassSlot(J9ClassPointer slot);

	/* General class handler to be reimplemented by specializing class. 
	 * This handler is called once per class. */
	protected abstract void doClass(J9ClassPointer clazz);
	
	protected abstract void doClassLoader(J9ClassLoaderPointer slot);

	protected abstract void doWeakReferenceSlot(J9ObjectPointer slot);
	protected abstract void doSoftReferenceSlot(J9ObjectPointer slot);
	protected abstract void doPhantomReferenceSlot(J9ObjectPointer slot);
	
	protected abstract void doFinalizableObject(J9ObjectPointer slot);
	protected abstract void doUnfinalizedObject(J9ObjectPointer slot);
	protected abstract void doOwnableSynchronizerObject(J9ObjectPointer slot);
	
	protected abstract void doMonitorReference(J9ObjectMonitorPointer objectMonitor);
	protected abstract void doMonitorLookupCacheSlot(J9ObjectMonitorPointer slot);
	
	protected abstract void doJNIWeakGlobalReference(J9ObjectPointer slot);
	protected abstract void doJNIGlobalReferenceSlot(J9ObjectPointer slot);

	protected abstract void doRememberedSlot(J9ObjectPointer slot);

	protected abstract void doJVMTIObjectTagSlot(J9ObjectPointer slot);

	protected abstract void doStringTableSlot(J9ObjectPointer slot);
	protected abstract void doStringCacheTableSlot(J9ObjectPointer slot);
	
	protected abstract void doVMClassSlot(J9ClassPointer slot);
	protected abstract void doVMThreadSlot(J9ObjectPointer slot);
	protected abstract void doVMThreadJNISlot(J9ObjectPointer slot);
	protected abstract void doVMThreadMonitorRecordSlot(J9ObjectPointer slot);
	protected abstract void doNonCollectableObjectSlot(J9ObjectPointer slot);
	protected abstract void doMemorySpaceSlot(J9ObjectPointer slot);
	protected abstract void doStackSlot(J9ObjectPointer slot);

	
	protected void doClassSlot(J9ClassPointer slot, VoidPointer address)
	{
		doClassSlot(slot);
	}
	protected void doClass(J9ClassPointer clazz, VoidPointer address)
	{
		doClass(clazz);
	}
	protected void doClassLoader(J9ClassLoaderPointer slot, VoidPointer address)
	{
		doClassLoader(slot);
	}
	protected void doWeakReferenceSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doWeakReferenceSlot(slot);
	}
	protected void doSoftReferenceSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSoftReferenceSlot(slot);
	}
	protected void doPhantomReferenceSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doPhantomReferenceSlot(slot);
	}
	protected void doMonitorLookupCacheSlot(J9ObjectMonitorPointer objectMonitor, ObjectMonitorReferencePointer slotAddress)
	{
		doMonitorLookupCacheSlot(objectMonitor);
	}
	protected void doJNIWeakGlobalReference(J9ObjectPointer slot, VoidPointer address)
	{
		doJNIWeakGlobalReference(slot);
	}
	protected void doJNIGlobalReferenceSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doJNIGlobalReferenceSlot(slot);
	}
	protected void doRememberedSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doRememberedSlot(slot);
	}
	protected void doJVMTIObjectTagSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doJVMTIObjectTagSlot(slot);
	}
	protected void doStringTableSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doStringTableSlot(slot);
	}
	protected void doStringCacheTableSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doStringCacheTableSlot(slot);
	}
	protected void doVMClassSlot(J9ClassPointer slot, VoidPointer address)
	{
		doVMClassSlot(slot);
	}
	protected void doVMThreadSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doVMThreadSlot(slot);
	}
	protected void doVMThreadJNISlot(J9ObjectPointer slot, VoidPointer address)
	{
		doVMThreadJNISlot(slot);
	}
	protected void doVMThreadMonitorRecordSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doVMThreadMonitorRecordSlot(slot);
	}
	
	protected void doNonCollectableObjectSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doNonCollectableObjectSlot(slot);
	}
	
	protected void doMemoryAreaSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doMemorySpaceSlot(slot);
	}
	
	/* TODO: lpnguyen add J9ObjectReferencePointer, J9ObjectPointerPointer versions of this in case anyone finds it useful to distinguish between the two */
	protected void doStackSlot(J9ObjectPointer slot, WalkState walkState, VoidPointer stackLocation)
	{
		doStackSlot(slot);
	}
	
	protected void scanPermanentClasses() throws CorruptDataException
	{
		J9ClassLoaderPointer sysClassLoader = _vm.systemClassLoader();
		J9ClassLoaderPointer appClassLoader = J9ClassLoaderPointer.cast(_vm.applicationClassLoader());
		
		GCSegmentIterator segmentIterator = GCSegmentIterator.fromJ9MemorySegmentList(_vm.classMemorySegments(), J9MemorySegment.MEMORY_TYPE_RAM_CLASS);
		setReachability(Reachability.STRONG);
		
		while (segmentIterator.hasNext()) {
			J9MemorySegmentPointer segment = segmentIterator.next();
			if (segment.classLoader().equals(sysClassLoader) || segment.classLoader().equals(appClassLoader)) {
				GCClassHeapIterator classHeapIterator = GCClassHeapIterator.fromJ9MemorySegment(segment);
				while (classHeapIterator.hasNext()) {
					doClass(classHeapIterator.next());
				}
			}
		}
	}
	
	/**
	 * Scan all slots which contain references into the heap.
	 * @note this includes class references.
	 * @throws CorruptDataException 
	 */
	public void scanAllSlots() throws CorruptDataException
	{
		scanClasses();
		scanVMClassSlots();
		
		scanClassLoaders();

		scanThreads();
		
		if(J9BuildFlags.gc_finalization) {
			scanFinalizableObjects();
		}

		scanJNIGlobalReferences();
		
		scanStringTable();

		scanWeakReferenceObjects();

		scanSoftReferenceObjects();
		scanPhantomReferenceObjects();

		if(J9BuildFlags.gc_finalization) {
			scanUnfinalizedObjects();
		}

		scanMonitorReferences();
		
		scanJNIWeakGlobalReferences();

		if(J9BuildFlags.gc_modronScavenger) {
			if(_extensions.scavengerEnabled()) {
				scanRememberedSet();
			}
		}

		if(J9BuildFlags.opt_jvmti) {
			scanJVMTIObjectTagTables();
		}
		
		scanOwnableSynchronizerObjects();
	}
	
	/**
	 * Scan all root set references from the VM into the heap.
	 * For all slots that are hard root references into the heap, the appropriate slot handler will be called.
	 * @note This includes all references to classes.
	 * @throws CorruptDataException 
	 */
	public void scanRoots() throws CorruptDataException
	{
		if (_classDataAsRoots || _nurseryReferencesOnly || _nurseryReferencesPossibly) {
			/* The classLoaderObject of a class loader might be in the nursery, but a class loader
			 * can never be in the remembered set, so include class loaders here.
			 */
			scanClassLoaders();
		}

		if (!_nurseryReferencesOnly && !_nurseryReferencesPossibly) {
			if (J9BuildFlags.gc_dynamicClassUnloading) {
				if (_classDataAsRoots) {
					scanClasses();
					/* We are scanning all classes, no need to include stack frame references */
					setIncludeStackFrameClassReferences(false);
				} else {
					scanPermanentClasses();
					setIncludeStackFrameClassReferences(true);
				}	
			} else {
				scanClasses();
				setIncludeStackFrameClassReferences(false);
			}
		}
		
		scanThreads();
		if (J9BuildFlags.gc_finalization) {
			scanFinalizableObjects();
		}
		scanJNIGlobalReferences();
		
		if (_stringTableAsRoot && (!_nurseryReferencesOnly && !_nurseryReferencesPossibly)) {
			scanStringTable();
		}
	}
	
	/**
	 * Scan all clearable root set references from the VM into the heap.
	 * For all slots that are clearable root references into the heap, the appropriate slot handler will be
	 * called.
	 * @note This includes all references to classes.
	 */
	public void scanClearable() throws CorruptDataException
	{
		scanSoftReferenceObjects();
		
		scanWeakReferenceObjects();
		
		if (J9BuildFlags.gc_finalization) {
			scanUnfinalizedObjects();
		}
		
		scanJNIWeakGlobalReferences();
		
		scanPhantomReferenceObjects();
		
		scanMonitorLookupCaches();

		scanMonitorReferences();
		
		if (!_stringTableAsRoot && (!_nurseryReferencesOnly && !_nurseryReferencesPossibly)) {
			scanStringTable();
		}
		
		scanOwnableSynchronizerObjects();
		
		if (J9BuildFlags.gc_modronScavenger) {
			/* Remembered set is clearable in a generational system -- if an object in old
			 * space dies, and it pointed to an object in new space, it needs to be removed
			 * from the remembered set.
			 * This must after any other marking might occur, e.g. phantom references.
			 */
			if (_includeRememberedSetReferences && !_nurseryReferencesOnly && !_nurseryReferencesPossibly) {
				scanRememberedSet();
			}
		}
		
		if (J9BuildFlags.opt_jvmti) {
			if (_includeJVMTIObjectTagTables) {
				scanJVMTIObjectTagTables();
			}
		}
	}

	protected void scanJVMTIObjectTagTables() throws CorruptDataException
	{
		if(!J9BuildFlags.opt_jvmti) return;
		setReachability(Reachability.WEAK);
		
		J9JVMTIDataPointer jvmtiData = J9JVMTIDataPointer.cast(_vm.jvmtiData());
		if(jvmtiData.notNull()) {
			GCJVMTIObjectTagTableListIterator objectTagTableList = GCJVMTIObjectTagTableListIterator.fromJ9JVMTIData(jvmtiData);
			while(objectTagTableList.hasNext()) {
				J9JVMTIEnvPointer list = objectTagTableList.next();
				GCJVMTIObjectTagTableIterator objectTagTableIterator = GCJVMTIObjectTagTableIterator.fromJ9JVMTIEnv(list);
				GCJVMTIObjectTagTableIterator addressIterator = GCJVMTIObjectTagTableIterator.fromJ9JVMTIEnv(list);
				while(objectTagTableIterator.hasNext()) {
					doJVMTIObjectTagSlot(objectTagTableIterator.next(), addressIterator.nextAddress());
				}
			}
		}
	}
	
	protected void scanNonCollectableObjects() throws CorruptDataException 
	{
		scanNonCollectableObjectsInternal(J9MemorySegment.MEMORY_TYPE_IMMORTAL);
		scanNonCollectableObjectsInternal(J9MemorySegment.MEMORY_TYPE_SCOPED);
	}
	
	private void scanNonCollectableObjectsInternal(long memoryType) throws CorruptDataException
	{
		GCHeapRegionIterator regionIterator = GCHeapRegionIterator.from();
		while(regionIterator.hasNext()) {
			GCHeapRegionDescriptor region = regionIterator.next();
			
			if(new UDATA(region.getTypeFlags()).allBitsIn(memoryType)) {
				GCObjectHeapIterator objectIterator = GCObjectHeapIterator.fromHeapRegionDescriptor(region, true, false);

				while(objectIterator.hasNext()) {
					J9ObjectPointer object = objectIterator.next();
					doClassSlot(J9ObjectHelper.clazz(object));
					GCObjectIterator fieldIterator = GCObjectIterator.fromJ9Object(object, true);
					GCObjectIterator fieldAddressIterator = GCObjectIterator.fromJ9Object(object, true);
					while(fieldIterator.hasNext()) {
						doNonCollectableObjectSlot(fieldIterator.next(), fieldAddressIterator.nextAddress());
					}
				}
			}
		}
	}
	
	protected void scanRememberedSet() throws CorruptDataException
	{
		if(!J9BuildFlags.gc_modronScavenger) return;
		setReachability(Reachability.WEAK);
		
		GCRememberedSetIterator rememberedSetIterator = GCRememberedSetIterator.from();
		while (rememberedSetIterator.hasNext()) {
			MM_SublistPuddlePointer puddle = rememberedSetIterator.next();
			GCRememberedSetSlotIterator rememberedSetSlotIterator = GCRememberedSetSlotIterator.fromSublistPuddle(puddle);
			GCRememberedSetSlotIterator addressIterator = GCRememberedSetSlotIterator.fromSublistPuddle(puddle);
			while (rememberedSetSlotIterator.hasNext()) {
				doRememberedSlot(rememberedSetSlotIterator.next(), addressIterator.nextAddress());
			}		
		}
	}

	protected void scanJNIWeakGlobalReferences() throws CorruptDataException
	{
		setReachability(Reachability.WEAK);
		GCJNIWeakGlobalReferenceIterator jniWeakGlobalReferenceIterator = GCJNIWeakGlobalReferenceIterator.from();
		GCJNIWeakGlobalReferenceIterator addressIterator = GCJNIWeakGlobalReferenceIterator.from();
		while(jniWeakGlobalReferenceIterator.hasNext()) {
			doJNIWeakGlobalReference(jniWeakGlobalReferenceIterator.next(), addressIterator.nextAddress());
		}
	}

	protected void scanMonitorReferences() throws CorruptDataException
	{
		setReachability(Reachability.WEAK);
		GCMonitorReferenceIterator monitorReferenceIterator = GCMonitorReferenceIterator.from();
		while(monitorReferenceIterator.hasNext()) {
			doMonitorReference(monitorReferenceIterator.next());
		}
	}

	protected void scanUnfinalizedObjects() throws CorruptDataException
	{
		if(!J9BuildFlags.gc_finalization) return;
		setReachability(Reachability.WEAK);
		
		GCUnfinalizedObjectListIterator unfinalizedObjectListIterator = GCUnfinalizedObjectListIterator.from();
		while(unfinalizedObjectListIterator.hasNext()) {
			doUnfinalizedObject(unfinalizedObjectListIterator.next());
		}
	}
	
	protected void scanOwnableSynchronizerObjects() throws CorruptDataException
	{
		setReachability(Reachability.WEAK);
		
		GCOwnableSynchronizerObjectListIterator ownableSynchronizerObjectListIterator = GCOwnableSynchronizerObjectListIterator .from();

		while (ownableSynchronizerObjectListIterator.hasNext()) {
			J9ObjectPointer ownableSynchronizerObject = ownableSynchronizerObjectListIterator.next();
			doOwnableSynchronizerObject(ownableSynchronizerObject);
		}
	}
	
	protected void scanMonitorLookupCaches() throws CorruptDataException
	{
		setReachability(Reachability.WEAK);
		
		GCVMThreadListIterator vmThreadListIterator = GCVMThreadListIterator.from();
		while (vmThreadListIterator.hasNext()) {
			J9VMThreadPointer walkThread = vmThreadListIterator.next();
			
			if (J9BuildFlags.thr_lockNursery) {
				ObjectMonitorReferencePointer objectMonitorLookupCache = walkThread.objectMonitorLookupCacheEA();
				for (long cacheIndex = 0; cacheIndex < J9VMThread.J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE; cacheIndex++) {
					ObjectMonitorReferencePointer slotAddress = objectMonitorLookupCache.add(cacheIndex);
					doMonitorLookupCacheSlot(slotAddress.at(0), slotAddress);
				}
			} else {
				
				/* Can't implement this because walkThread.cachedMonitor field does not exist as we've never had a vm29 spec yet with the condition: 
				 * (!J9BuildFlags.thr_lockNursery && !J9BuildFlags.opt_realTimeLockingSupport)
				 * 
				 * // This cast is ugly but is technically what the c-code is doing 
				 * ObjectMonitorReferencePointer cachedMonitorSlot = ObjectMonitorReferencePointer.cast(walkThread.cachedMonitorEA());
				 * doMonitorLookupCacheSlot(walkThread.cachedMonitor(), cachedMonitorSlot);
				 */
				throw new UnsupportedOperationException("Not implemented");
			}
		}
	}
	
	protected void scanPhantomReferenceObjects() throws CorruptDataException
	{
//		if(!J9BuildFlags.gc_referenceObjects) return;
//		setReachability(Reachability.WEAK);
	}

	protected void scanSoftReferenceObjects() throws CorruptDataException
	{
//		if(!J9BuildFlags.gc_referenceObjects) return;
//		setReachability(Reachability.WEAK);
	}

	protected void scanWeakReferenceObjects() throws CorruptDataException
	{
//		if(!J9BuildFlags.gc_weakReferenceObjects) return;
//		setReachability(Reachability.WEAK);
	}

	protected void scanStringTable() throws CorruptDataException
	{
		if (_extensions.collectStringConstants()) {
			setReachability(Reachability.WEAK);
		} else {
			setReachability(Reachability.STRONG);
		}
		
		GCStringTableIterator stringTableIterator = GCStringTableIterator.from();
		GCStringTableIterator stringTableAddressIterator = GCStringTableIterator.from();
		while(stringTableIterator.hasNext()) {
			doStringTableSlot(stringTableIterator.next(), stringTableAddressIterator.nextAddress());
		}
		
		GCStringCacheTableIterator cacheIterator = GCStringCacheTableIterator.from();
		GCStringCacheTableIterator cacheAddressIterator = GCStringCacheTableIterator.from();
		while(cacheIterator.hasNext()) {
			doStringCacheTableSlot(cacheIterator.next(), cacheAddressIterator.nextAddress());
		}
		
	}

	protected void scanJNIGlobalReferences() throws CorruptDataException
	{
		setReachability(Reachability.STRONG);
	
		GCJNIGlobalReferenceIterator jniGlobalReferenceIterator = GCJNIGlobalReferenceIterator.from();
		GCJNIGlobalReferenceIterator addressIterator = GCJNIGlobalReferenceIterator.from();
		while(jniGlobalReferenceIterator.hasNext()) {
			doJNIGlobalReferenceSlot(jniGlobalReferenceIterator.next(), addressIterator.nextAddress());
		}	
	}

	protected void scanFinalizableObjects() throws CorruptDataException
	{
		if(!J9BuildFlags.gc_finalization) return;
		setReachability(Reachability.STRONG);
		
		/* New style finalizable object list */
		GCFinalizableObjectIterator finalizableObjectIterator = GCFinalizableObjectIterator.from();
		while(finalizableObjectIterator.hasNext()) {
			doFinalizableObject(finalizableObjectIterator.next());	
		}
	}

	protected void scanThreads() throws CorruptDataException
	{
		setReachability(Reachability.STRONG);
		GCVMThreadListIterator vmThreadListIterator = GCVMThreadListIterator.from();
		while (vmThreadListIterator.hasNext()) {
			J9VMThreadPointer walkThread = vmThreadListIterator.next();

			/* "Inline" the behaviour of GC_VMThreadIterator to distinguish between the types of roots */
			GCVMThreadSlotIterator threadSlotIterator = GCVMThreadSlotIterator.fromJ9VMThread(walkThread);
			GCVMThreadSlotIterator threadSlotAddressIterator = GCVMThreadSlotIterator.fromJ9VMThread(walkThread);
			while (threadSlotIterator.hasNext()) {
				doVMThreadSlot(threadSlotIterator.next(), threadSlotAddressIterator.nextAddress());
			}
			
			GCVMThreadJNISlotIterator jniSlotIterator = GCVMThreadJNISlotIterator.fromJ9VMThread(walkThread);
			GCVMThreadJNISlotIterator jniSlotAddressIterator = GCVMThreadJNISlotIterator.fromJ9VMThread(walkThread);
			while (jniSlotIterator.hasNext()) {
				doVMThreadJNISlot(jniSlotIterator.next(), jniSlotAddressIterator.nextAddress());
			}

			if (J9BuildFlags.interp_hotCodeReplacement) {
				GCVMThreadMonitorRecordSlotIterator monitorRecordSlotIterator = GCVMThreadMonitorRecordSlotIterator.fromJ9VMThread(walkThread);
				GCVMThreadMonitorRecordSlotIterator addressIterator = GCVMThreadMonitorRecordSlotIterator.fromJ9VMThread(walkThread);
				while (monitorRecordSlotIterator.hasNext()) {
					doVMThreadMonitorRecordSlot(monitorRecordSlotIterator.next(), addressIterator.nextAddress());
				}
			}
			
			if (_scanStackSlots) {
				GCVMThreadStackSlotIterator.scanSlots(walkThread, _stackWalkerCallbacks, _includeStackFrameClassReferences, _trackVisibleStackFrameDepth);
			}
		}
	}

	protected void scanClassLoaders() throws CorruptDataException
	{
		J9ClassLoaderPointer sysClassLoader = _vm.systemClassLoader();
		J9ClassLoaderPointer appClassLoader = J9ClassLoaderPointer.cast(_vm.applicationClassLoader());
		
		GCClassLoaderIterator classLoaderIterator = GCClassLoaderIterator.from();
		while (classLoaderIterator.hasNext()) {
			J9ClassLoaderPointer loader = classLoaderIterator.next();
			
			Reachability reachability;
			if (J9BuildFlags.gc_dynamicClassUnloading) {
				long dynamicClassUnloadingFlag = _extensions.dynamicClassUnloading();
				
				if (MM_GCExtensions$DynamicClassUnloading.DYNAMIC_CLASS_UNLOADING_NEVER == dynamicClassUnloadingFlag) {
					reachability = Reachability.STRONG;
				} else {
					if (loader.eq(sysClassLoader) || loader.eq(appClassLoader)) {
						reachability = Reachability.STRONG;
					} else {
						reachability = Reachability.WEAK;
					}
				}
			} else {
				reachability = Reachability.STRONG;
			}
			
			setReachability(reachability);

			doClassLoader(loader);
		}
	}

	protected void scanVMClassSlots() throws CorruptDataException
	{
		GCVMClassSlotIterator classSlotIterator = GCVMClassSlotIterator.from();
		GCVMClassSlotIterator addressIterator = GCVMClassSlotIterator.from();
		setReachability(Reachability.STRONG);
		while (classSlotIterator.hasNext()) {
			doVMClassSlot(classSlotIterator.next(), addressIterator.nextAddress());
		}
	}

	protected void scanClasses() throws CorruptDataException
	{
		J9ClassLoaderPointer sysClassLoader = _vm.systemClassLoader();
		J9ClassLoaderPointer appClassLoader = J9ClassLoaderPointer.cast(_vm.applicationClassLoader());
		
		GCSegmentIterator segmentIterator = GCSegmentIterator.fromJ9MemorySegmentList(_vm.classMemorySegments(), J9MemorySegment.MEMORY_TYPE_RAM_CLASS);
		
		
		while(segmentIterator.hasNext()) {
			J9MemorySegmentPointer segment = segmentIterator.next();
			GCClassHeapIterator classHeapIterator = GCClassHeapIterator.fromJ9MemorySegment(segment);
			while (classHeapIterator.hasNext()) {
				J9ClassPointer clazz = classHeapIterator.next();
				Reachability reachability;
				if (J9BuildFlags.gc_dynamicClassUnloading) {
					long dynamicClassUnloadingFlag = _extensions.dynamicClassUnloading();
					
					if (MM_GCExtensions$DynamicClassUnloading.DYNAMIC_CLASS_UNLOADING_NEVER == dynamicClassUnloadingFlag) {
						reachability = Reachability.STRONG;
					} else {
						if (clazz.classLoader().eq(sysClassLoader) || clazz.classLoader().eq(appClassLoader)) {
							reachability = Reachability.STRONG;
						} else {
							reachability = Reachability.WEAK;
						}
					}
				} else {
					reachability = Reachability.STRONG;
				}
				
				setReachability(reachability);
				
				doClass(clazz);
			}
		}
	}
	
	private void setReachability(Reachability r)
	{
		_reachability = r;
	}
	
	protected Reachability getReachability()
	{
		return _reachability;
	}
}
