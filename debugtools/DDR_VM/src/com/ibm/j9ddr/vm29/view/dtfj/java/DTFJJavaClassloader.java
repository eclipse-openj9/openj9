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
package com.ibm.j9ddr.vm29.view.dtfj.java;

import static com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils.corruptIterator;
import static com.ibm.j9ddr.vm29.events.EventManager.register;
import static com.ibm.j9ddr.vm29.events.EventManager.unregister;

import java.lang.ref.SoftReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.image.J9DDRCorruptData;
import com.ibm.j9ddr.vm29.j9.walkers.ClassIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;
import com.ibm.j9ddr.vm29.view.dtfj.java.j9.SlidingIterator;

public class DTFJJavaClassloader implements JavaClassLoader {

	private J9ClassLoaderPointer j9ClassLoader;
	private SoftReference<ClassCache> softReferenceToClassCache = new SoftReference<ClassCache>(null) ;  
	private Logger log = DTFJContext.getLogger();
	
	public DTFJJavaClassloader(J9ClassLoaderPointer pointer) {
		this.j9ClassLoader = pointer;
		log.fine(String.format("Created JavaClassloader for 0x%016x", pointer.getAddress()));
	}

	public JavaClass findClass(String name) throws CorruptDataException {
		ClassCache cache = getPopulatedClassCache(); // the scope of this strong reference to the cache is just this method, only the soft reference will persist
		return cache.findClass(name);
	}

	@SuppressWarnings("rawtypes")
	public Iterator getCachedClasses() {
		ClassCache cache = getPopulatedClassCache(); // the scope of this strong reference to the cache is just this method, only the soft reference will persist
		return cache.getCachedClasses();
	}

	@SuppressWarnings("rawtypes")
	public Iterator getDefinedClasses() {
		ClassCache cache = getPopulatedClassCache(); // the scope of this strong reference to the cache is just this method, only the soft reference will persist
		return cache.getDefinedClasses();
	}

	public JavaObject getObject() throws CorruptDataException {
		try {
			return new DTFJJavaObject(j9ClassLoader.classLoaderObject());
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public boolean equals(Object obj) {
		if (obj != null && obj instanceof DTFJJavaClassloader) {
			DTFJJavaClassloader objImpl = (DTFJJavaClassloader) obj;
			return j9ClassLoader.getAddress() == objImpl.j9ClassLoader.getAddress();
		}

		return false;
	}

	public int hashCode() {
		return (int) j9ClassLoader.getAddress();
	}

	/**
	 *  
	 * @return a populated class cache. If it can, it will retrieve the class cache using the soft reference but it is prepared to 
	 * cope with either finding the soft reference is null (as it will be on the very first time through) or with finding that the
	 * soft reference has been cleared. In all cases, on exit, the soft reference will exist and will be referencing a populated cache.  
	 */
	private ClassCache getPopulatedClassCache() {
		ClassCache cache;
		cache = softReferenceToClassCache.get();
		/* 
		 * On the first time through, cache will be null because we created the soft reference that 
		 * way so that we only initialize the class cache if needed.
		 * On subsequent occasions, the cache may be null if memory pressure caused GC
		 * to clear the soft reference.
		 */
		if (cache == null) {
			cache = new ClassCache();
			cache.populateCache();
			log.fine(String.format("Populated class cache for JavaClassLoader 0x%016x", j9ClassLoader.getAddress()));
			softReferenceToClassCache = new SoftReference<ClassCache>(cache); // this soft reference is what persists
		}		
		return cache;		
	}
	
	private class ClassCache implements IEventListener { 
		private ArrayList<Object> cache = new ArrayList<Object>();					//cache of the objects
		private HashMap<String, Integer> names = new HashMap<String, Integer>();	//name lookup index
		@SuppressWarnings("rawtypes")
		private Iterator corruptCache;
		private int definedClassCount = 0;
		
		void populateCache() {
			Iterator<J9ClassPointer> classes;
			try {
				classes = ClassIterator.fromJ9Classloader(j9ClassLoader);
			} catch (Throwable t) {		//if we are corrupt here, then we can't return any classes
				CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
				corruptCache = corruptIterator(cd);
				return;				//abort cache population
			}

			int index = 0;
			index += storeClasses(classes, index);
			try {
				ClassSegmentIterator definedClasses = new ClassSegmentIterator(j9ClassLoader.classSegments());
				definedClassCount = storeClasses(definedClasses, index);
				index += definedClassCount;
			} catch (Throwable t) {
				CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
				cache.add(cd);
			}
			if(log.isLoggable(Level.FINE)) {
				if(corruptCache == null) {
					log.fine(String.format("The class loader cache for 0x%016x is not corrupt", j9ClassLoader.getAddress()));
					log.fine(String.format("Stored %d classes", cache.size()));
					log.fine(String.format("Found %d defined classes", definedClassCount));
				} else {
					log.fine("The class loader cache is corrupt");
				}
			}
		}
		
		@SuppressWarnings("rawtypes")
		private int storeClasses(Iterator classes, int start) {
			int initialSize = cache.size();
			try {
				register(this);		//register this class for notifications	
				while(classes.hasNext()) {
					J9ClassPointer ptr = (J9ClassPointer)classes.next();
					log.fine(String.format("Found JavaClass at 0x%016x", ptr.getAddress()));
					DTFJJavaClass clazz = new DTFJJavaClass(ptr);
					try {
						//store the DTFJ class name rather than the underlying J9 name as API users will supply the DTFJ name
						names.put(clazz.getName(), start + (cache.size() - initialSize));
						cache.add(clazz);
					} catch (CorruptDataException e) {
						cache.add(e.getCorruptData());
					}
				}
			} finally {
				unregister(this);	//remove notifier registration
			}
			return cache.size() - initialSize;		//return how many items were placed in the cache as a result of executing this function
		}
		
		
		
		/* Corrupt data is added to the cache
		 * (non-Javadoc)
		 * @see com.ibm.j9ddr.events.IEventListener#corruptData(java.lang.String, com.ibm.j9ddr.CorruptDataException, boolean)
		 */
		public void corruptData(String message,	com.ibm.j9ddr.CorruptDataException e, boolean fatal) {
			J9DDRCorruptData cd = J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e);
			cache.add(cd);
		}

		@SuppressWarnings("rawtypes")
		Iterator getCachedClasses() {
			if(corruptCache == null) {
				return new SlidingIterator(cache, 0, cache.size() - definedClassCount);
			} else {
				return corruptCache;
			}
		}
		
		@SuppressWarnings("rawtypes")
		Iterator getDefinedClasses() {
			if(corruptCache == null) {
				if(definedClassCount == 0) {
					return J9DDRDTFJUtils.emptyIterator();
				}
				return new SlidingIterator(cache, cache.size() - definedClassCount, cache.size());
			} else {
				return corruptCache;
			}
		}
		
		JavaClass findClass(String name) {
			Object index = names.get(name);
			if(index == null) {
				return null;
			} else {
				if(index instanceof Integer) {
					return (JavaClass) cache.get(((Integer)index));		//only valid JavaClass objects are put in the name lookup, so cast is safe
				} else {
					throw new IllegalArgumentException(String.format("The name lookup cache contained an instance of %s", index.getClass().getName()));
				}
			}
		}
	}
	
}
