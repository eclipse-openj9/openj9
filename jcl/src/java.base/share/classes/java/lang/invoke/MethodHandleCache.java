/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2014, 2014 IBM Corp. and others
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

package java.lang.invoke;

import static java.lang.invoke.MethodType.methodType;

import java.lang.ref.ReferenceQueue;

import com.ibm.oti.util.WeakReferenceNode;
import com.ibm.oti.vm.VMLangAccess;

/**
 * MethodHandleCache is a per-class cache that used to cache:
 * * the Class.cast(Object) handle for the class &
 * * the Constant(null) handle for the class.
 * These handles are commonly created and this saves on creating duplicate handles.
 */
class MethodHandleCache {

	private Class<?> clazz;
	/*[PR JAZZ 58955] Prevent duplicate RBH's of the same class on Class.cast() */
	private MethodHandle classCastHandle;
	private MethodHandle nullConstantObjectHandle;
	private WeakReferenceNode<DirectHandle> directHandlesHead;
	private ReferenceQueue<DirectHandle> referenceQueue = new ReferenceQueue<>();

	private MethodHandleCache(Class<?> clazz) {
		this.clazz = clazz;
	}

	/**
	 * Get the MethodHandle cache for a given class.
	 * If one doesn't exist, it will be created.
	 * 
	 * @param clazz the Class that MHs are being cached on
	 * @return the MethodHandleCache object for that class
	 */
	public static MethodHandleCache getCache(Class<?> clazz) {
		VMLangAccess vma = MethodHandles.Lookup.getVMLangAccess();
		MethodHandleCache cache = (MethodHandleCache)vma.getMethodHandleCache(clazz);
		if (null == cache) {
			cache = new MethodHandleCache(clazz);
			cache = (MethodHandleCache)vma.setMethodHandleCache(clazz, cache);
		}
		return cache;
	}

	/**
	 * @return a BoundMethodHandle that calls {@link Class#cast(Object)} on the passed in class
	 * @throws NoSuchMethodException 
	 * @throws IllegalAccessException 
	 */
	/*[PR JAZZ 58955] Prevent duplicate RBH's of the same class on Class.cast() */
	public MethodHandle getClassCastHandle() throws IllegalAccessException, NoSuchMethodException {
		if (null == classCastHandle) {
			synchronized (this) {
				if (null == classCastHandle) {
					classCastHandle = MethodHandles.Lookup.internalPrivilegedLookup.bind(clazz, "cast", methodType(Object.class, Object.class)); //$NON-NLS-1$
				}
			}
		}
		return classCastHandle;
	}

	/**
	 * @return a MethodHandle that contains a null ConstantHandle for a specific class.
	 */
	public MethodHandle getNullConstantObjectHandle() {
		if (null == nullConstantObjectHandle) {
			nullConstantObjectHandle = new ConstantObjectHandle(methodType(clazz), null);
		}
		return nullConstantObjectHandle;
	}
	
	/**
	 * Add a DirectHandle that needs to be updated upon redefinition of the Class that owns this cache.
	 * The DirectHandle should reference a method in that Class.
	 * 
	 * @param handle A DirectHandle to track for class redefinition
	 */
	public void addDirectHandle(DirectHandle handle) {
		WeakReferenceNode<DirectHandle> ref = new WeakReferenceNode<>(handle, referenceQueue);
		
		synchronized (this) {
			ref.addBefore(directHandlesHead);
			directHandlesHead = ref;
		}
		
		processReferenceQueue();
	}

	/**
	 * MethodHandles may be garbage collected, so we need to remove collected WeakReferences to prevent
	 * the linked list from growing indefinitely.
	 */
	private synchronized void processReferenceQueue() {
		WeakReferenceNode<DirectHandle> forRemoval;
		while (null != (forRemoval = (WeakReferenceNode<DirectHandle>)referenceQueue.poll())) {
			if (directHandlesHead == forRemoval) {
				directHandlesHead = forRemoval.next();
			} else {
				forRemoval.remove();
			}
		}
	}
}
