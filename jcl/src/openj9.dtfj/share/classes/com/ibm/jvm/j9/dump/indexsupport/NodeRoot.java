/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.j9.dump.indexsupport;

import org.xml.sax.Attributes;

import com.ibm.dtfj.java.j9.JavaReference;
import com.ibm.dtfj.java.j9.JavaRuntime;

/**
 * @author nhardman
 * 
 */
public class NodeRoot extends NodeAbstract {
	
	long address;
	int reachability = JavaReference.REACHABILITY_UNKNOWN;
	int roottype = JavaReference.HEAP_ROOT_OTHER;
	String description;

	public NodeRoot(JavaRuntime runtime, Attributes attributes)
	{
		String typeAttr = attributes.getValue("type");
		String reachabilityAttr = attributes.getValue("reachability");

		address = _longFromString(attributes.getValue("id"));
		description = typeAttr;
		
		 // do the mapping of the reachability description to a reachability value.
		if(reachabilityAttr.equals("strong")) {
			reachability = JavaReference.REACHABILITY_STRONG;
		} else if(reachabilityAttr.equals("weak")) {
			reachability = JavaReference.REACHABILITY_WEAK;
		} else if(reachabilityAttr.equals("soft")) {
			reachability = JavaReference.REACHABILITY_SOFT;
		} else if(reachabilityAttr.equals("phantom")) {
			reachability = JavaReference.REACHABILITY_PHANTOM;
		}
		
		 // do the mapping of type names to root type.
		if (typeAttr.equals("None")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		} else if (typeAttr.equals("Class")) {
			roottype = JavaReference.HEAP_ROOT_SYSTEM_CLASS;
		} else if (typeAttr.equals("VMClassSlot")) {
			roottype = JavaReference.HEAP_ROOT_SYSTEM_CLASS;
		} else if (typeAttr.equals("PermanentClass")) {
			roottype = JavaReference.HEAP_ROOT_SYSTEM_CLASS;
		} else if (typeAttr.equals("ClassLoader")) {
			roottype = JavaReference.HEAP_ROOT_CLASSLOADER;
		} else if (typeAttr.equals("Thread")) {
			roottype = JavaReference.HEAP_ROOT_THREAD;
		} else if (typeAttr.equals("FinalizableObject")) {
			roottype = JavaReference.HEAP_ROOT_FINALIZABLE_OBJ;
		} else if (typeAttr.equals("UnfinalizedObject")) {
			roottype = JavaReference.HEAP_ROOT_UNFINALIZED_OBJ;
		} else if (typeAttr.equals("StringTable")) {
			roottype = JavaReference.HEAP_ROOT_STRINGTABLE;
		} else if (typeAttr.equals("JNIGlobalReference")) {
			roottype = JavaReference.HEAP_ROOT_JNI_GLOBAL;
		} else if (typeAttr.equals("JNIWeakGlobalReference")) {
			roottype = JavaReference.HEAP_ROOT_JNI_GLOBAL;
		} else if (typeAttr.equals("DebuggerReference")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		} else if (typeAttr.equals("DebuggerClassReference")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		} else if (typeAttr.equals("MonitorReference")) {
			roottype = JavaReference.HEAP_ROOT_MONITOR;
		} else if (typeAttr.equals("WeakReferenceObject")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		} else if (typeAttr.equals("SoftReferenceObject")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		} else if (typeAttr.equals("PhantomReferenceObject")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		} else if (typeAttr.equals("JVMTIObjectTagTable")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		} else if (typeAttr.equals("NonCollectableObject")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		} else if (typeAttr.equals("RememberedSet")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		} else if (typeAttr.equals("MemoryAreaObject")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		} else if (typeAttr.equals("MetronomeRememberedSet")) {
			roottype = JavaReference.HEAP_ROOT_OTHER;
		}
	}
}
