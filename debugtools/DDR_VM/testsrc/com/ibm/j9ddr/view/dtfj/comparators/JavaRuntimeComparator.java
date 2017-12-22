/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.comparators;

import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaRuntimeComparator extends DTFJComparator {

	public static final int COMPILED_METHODS = 1;
	public static final int HEAP_ROOTS = 2;
	public static final int HEAPS = 4;
	public static final int JAVA_CLASS_LOADERS = 8;
	public static final int JAVA_VM = 16;
	public static final int JAVA_VM_INIT_ARGS = 32;
	public static final int MONITORS = 64;
	public static final int THREADS = 128;
	
	// getCompiledMethods()
	// getHeapRoots()
	// getHeaps()
	// getJavaClassLoaders()
	// getJavaVM()
	// getJavaVMInitArgs()
	// getMonitors()
	// getThreads()
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaRuntime ddrJavaRuntime = (JavaRuntime) ddrObject;
		JavaRuntime jextractJavaRuntime = (JavaRuntime) jextractObject;

		// getCompiledMethods()
		if ((COMPILED_METHODS & members) != 0)
			new JavaMethodComparator().testComparatorIteratorEquals(ddrJavaRuntime, jextractJavaRuntime, "getCompiledMethods", JavaMethod.class);
		
		// TODO: Can't handle multiple types in this iterator yet.  Probably its own comparator for JavaReferenceOrJavaStackFrameComparator
		// getHeapRoots()

		// getHeaps()
		if ((HEAPS & members) != 0)
			new JavaHeapComparator().testComparatorIteratorEquals(ddrJavaRuntime, jextractJavaRuntime, "getHeaps", JavaHeap.class);
		
		// getJavaClassLoaders()
		if ((JAVA_CLASS_LOADERS & members) != 0)
			new JavaClassLoaderComparator().testComparatorIteratorEquals(ddrJavaRuntime, jextractJavaRuntime, "getJavaClassLoaders", JavaClassLoader.class);
		
		// getJavaVM()
		if ((JAVA_VM & members) != 0)
			new ImagePointerComparator().testComparatorEquals(ddrJavaRuntime, jextractJavaRuntime, "getJavaVM");
		
		// getJavaVMInitArgs()
		if ((JAVA_VM_INIT_ARGS & members) != 0)
			new JavaVMInitArgsComparator().testComparatorEquals(ddrJavaRuntime, jextractJavaRuntime, "getJavaVMInitArgs");
		
		// getMonitors()
		if ((MONITORS & members) != 0)
			new JavaMonitorComparator().testComparatorIteratorEquals(ddrJavaRuntime, jextractJavaRuntime, "getMonitors", JavaMonitor.class);
		
		// getThreads()
		if ((THREADS & members) != 0)
			new JavaThreadComparator().testComparatorIteratorEquals(ddrJavaRuntime, jextractJavaRuntime, "getThreads", JavaThread.class);
	}
}
