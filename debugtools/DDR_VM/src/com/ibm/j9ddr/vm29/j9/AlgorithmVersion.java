/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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

import java.lang.reflect.Field;
import java.util.HashMap;

import com.ibm.j9ddr.vm29.structure.DDRAlgorithmVersions;

public class AlgorithmVersion {
	// VM Version constants
	private static final String VM_MAJOR_VERSION = "VM_MAJOR_VERSION";
	private static final String VM_MINOR_VERSION = "VM_MINOR_VERSION";
	
	// ALgorithm version constants
	
	public static final String AVL_TREE_VERSION = "ALG_AVL_TREE_VERSION";
	public static final String DEBUG_LOCAL_MAP_VERSION = "ALG_DEBUG_LOCAL_MAP_VERSION";
	public static final String HASH_TABLE_VERSION = "VM_HASHTABLE_VERSION";
	public static final String JIT_STACK_WALKER_VERSION = "ALG_JIT_STACK_WALKER_VERSION";
	public static final String LOCAL_MAP_VERSION = "ALG_LOCAL_MAP_VERSION";
	public static final String METHOD_META_DATA_VERSION = "ALG_METHOD_META_DATA_VERSION";
	public static final String MONITOR_HASH_FUNCTION_VERSION = "ALG_MONITOR_HASH_FUNCTION_VERSION";
	public static final String MONITOR_EQUAL_FUNCTION_VERSION = "ALG_MONITOR_EQUAL_FUNCTION_VERSION";
	public static final String OBJECT_FIELD_OFFSET = "ALG_OBJECT_FIELD_OFFSET_VERSION";
	public static final String OBJECT_MONITOR_VERSION = "OBJECT_MONITOR_VERSION";
	public static final String OPT_INFO_VERSION = "OPT_INFO_VERSION";
	public static final String POOL_VERSION = "ALG_POOL_VERSION";
	public static final String ROM_HELP_VERSION = "ALG_ROM_HELP_VERSION";
	public static final String STACK_MAP_VERSION = "ALG_STACK_MAP_VERSION";
	public static final String STACK_WALKER_VERSION = "ALG_STACKWALKER_VERSION";
	public static final String FOUR_BYTE_OFFSETS_VERSION = "FOUR_BYTE_OFFSETS_VERSION";
	public static final String VTABLE_VERSION = "ALG_VM_VTABLE_VERSION";
	public static final String ITABLE_VERSION = "ALG_VM_ITABLE_VERSION";
	public static final String BYTECODE_VERSION = "ALG_VM_BYTECODE_VERSION";
	
	public static final String GC_ARRAYLET_OBJECT_MODEL_VERSION = "ALG_GC_ARRAYLET_OBJECT_MODEL_VERSION";
	public static final String GC_CLASS_MODEL_VERSION = "ALG_GC_CLASS_MODEL_VERSION";
	public static final String GC_CONTIGUOUS_ARRAY_OBJECT_MODEL_VERSION = "ALG_GC_CONTIGUOUS_ARRAY_OBJECT_MODEL_VERSION";
	public static final String GC_EMPTY_OBJECT_ITERATOR_VERSION = "ALG_GC_EMPTY_OBJECT_ITERATOR_VERSION";
	public static final String GC_HEAP_LINKED_FREE_HEADER_VERSION = "ALG_GC_HEAP_LINKED_FREE_HEADER_VERSION";
	public static final String GC_HEAP_REGION_DESCRIPTOR_VERSION = "ALG_GC_HEAP_REGION_DESCRIPTOR_VERSION";
	public static final String GC_MIXED_OBJECT_ITERATOR_VERSION = "ALG_GC_MIXED_OBJECT_ITERATOR_VERSION";
	public static final String GC_MIXED_OBJECT_MODEL_VERSION = "ALG_GC_MIXED_OBJECT_MODEL_VERSION";
	public static final String GC_OBJECT_HEAP_ITERATOR_ADDRESS_ORDERED_LIST_VERSION = "ALG_GC_OBJECT_HEAP_ITERATOR_ADDRESS_ORDERED_LIST_VERSION";
	public static final String GC_OBJECT_HEAP_ITERATOR_MARK_MAP_VERSION = "ALG_GC_OBJECT_HEAP_ITERATOR_MARK_MAP_VERSION";
	public static final String GC_OBJECT_HEAP_ITERATOR_SEGREGATED_VERSION = "ALG_GC_OBJECT_HEAP_ITERATOR_SEGREGATED_ORDERED_LIST_VERSION";
	public static final String GC_OBJECT_MODEL_VERSION = "ALG_GC_OBJECT_MODEL_VERSION";
	public static final String GC_POINTER_ARRAY_ITERATOR_VERSION = "ALG_GC_POINTER_ARRAY_ITERATOR_VERSION";
	public static final String GC_POINTER_ARRAYLET_ITERATOR_VERSION = "ALG_GC_POINTER_ARRAYLET_ITERATOR_VERSION";
	public static final String GC_SCAVENGER_FORWARDED_HEADER_VERSION = "ALG_GC_SCAVENGER_FORWARDED_HEADER_VERSION";

	public static final String MM_OBJECT_ACCESS_BARRIER_VERSION = "ALG_MM_OBJECT_ACCESS_BARRIER_VERSION";

	// Fields
	private static AlgorithmVersion DEFAULT_VERSION;
	private static int vmMajorVersion;
	private static int vmMinorVersion;
	private static HashMap<String, AlgorithmVersion> versionCache;
	private int algVersion;
	
	// Nobody instantiates this Class.  Used by getVersionOf(String)
	private AlgorithmVersion(int version) {
		super();
		this.algVersion = version;
	}
	
	public static AlgorithmVersion getVersionOf(String algorithmID) {
		if (versionCache == null) {
			init();
		}

		AlgorithmVersion result = versionCache.get(algorithmID);
		if (result == null) {
			return DEFAULT_VERSION;
		}
		return result;
	}
	
	// Read the blob constants and cache
	private static void init() {
		versionCache = new HashMap<String, AlgorithmVersion>();
		try {
			Field[] fields = DDRAlgorithmVersions.class.getFields();
			for (Field field : fields) {
				if (field.getType().equals(Long.TYPE)) {
					String fieldName = field.getName();
					long value = field.getLong(null);
					if (fieldName.equals(VM_MAJOR_VERSION)) {
						vmMajorVersion = (int) value;
					} else if (fieldName.equals(VM_MINOR_VERSION)) {
						vmMinorVersion = (int) value;
					} else {
						AlgorithmVersion version = new AlgorithmVersion((int) value);
						versionCache.put(field.getName(), version);
					}
				}
			}
		} catch (IllegalArgumentException e) {
			// Ignore
		} catch (IllegalAccessException e) {
			// Ignore
		} catch (NoClassDefFoundError e) {
			vmMajorVersion = 2;
			vmMinorVersion = 30;
		}
		
		// Create a default version to return
		DEFAULT_VERSION = new AlgorithmVersion(0);
	}

	// VM Versions are constant for all algorithms in a particular VM
	public static int getVMMajorVersion() {
		if (versionCache == null) {
			init();
		}
		
		return vmMajorVersion;
	}

	// VM Versions are constant for all algorithms in a particular VM
	public static int getVMMinorVersion() {
		if (versionCache == null) {
			init();
		}
		
		return vmMinorVersion;
	}
	
	public int getAlgorithmVersion() {
		return algVersion;
	}

	@Override
	public String toString()
	{
		return "AlgorithmVersion. VM: " + getVMMajorVersion() + "." + getVMMinorVersion() + " algorithm version: " + getAlgorithmVersion();
	}
	
	
}
