<?xml version="1.0" encoding="UTF-8"?>

<!--
  Copyright (c) 2006, 2017 IBM Corp. and others
 
  This program and the accompanying materials are made available under
  the terms of the Eclipse Public License 2.0 which accompanies this
  distribution and is available at https://www.eclipse.org/legal/epl-2.0/
  or the Apache License, Version 2.0 which accompanies this distribution and
  is available at https://www.apache.org/licenses/LICENSE-2.0.
 
  This Source Code may also be made available under the following
  Secondary Licenses when the conditions for such availability set
  forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
  General Public License, version 2 with the GNU Classpath
  Exception [1] and GNU General Public License, version 2 with the
  OpenJDK Assembly Exception [2].
 
  [1] https://www.gnu.org/software/classpath/license.html
  [2] http://openjdk.java.net/legal/assembly-exception.html

  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

<feature xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://www.ibm.com/j9/builder/feature" xsi:schemaLocation="http://www.ibm.com/j9/builder/feature feature-v2.xsd" id="combogc">
	<name>Standard+Balanced+SRT Garbage Collector</name>
	<description></description>
	<properties/>
	<source/>
	<flags>
		<flag id="gc_adaptiveTenuring" value="true"/>
		<flag id="gc_allocationTax" value="true"/>
		<flag id="gc_arraylets" value="true"/>
		<flag id="gc_combinationSpec" value="true"/>
		<flag id="gc_concurrentSweep" value="true"/>
		<flag id="gc_dynamicClassUnloading" value="true"/>
		<flag id="gc_dynamicNewSpaceSizing" value="true"/>
		<flag id="gc_finalization" value="true"/>
		<flag id="gc_fragmentedHeap" value="true"/>
		<flag id="gc_generational" value="true"/>
		<flag id="gc_heapCardTable" value="true"/>
		<flag id="gc_hybridArraylets" value="true"/>
		<flag id="gc_jniArrayCache" value="true"/>
		<flag id="gc_largeObjectArea" value="true"/>
		<flag id="gc_leafBits" value="true"/>
		<flag id="gc_modronCompaction" value="true"/>
		<flag id="gc_modronConcurrentMark" value="true"/>
		<flag id="gc_modronGC" value="true"/>
		<flag id="gc_modronScavenger" value="true"/>
		<flag id="gc_modronStandard" value="true"/>
		<flag id="gc_modronTrace" value="true"/>
		<flag id="gc_nonZeroTLH" value="true"/>
		<flag id="gc_objectAccessBarrier" value="true"/>
		<flag id="gc_realtime" value="true"/>
		<flag id="gc_segregatedHeap" value="true"/>
		<flag id="gc_staccato" value="true"/>
		<flag id="gc_threadLocalHeap" value="true"/>
		<flag id="gc_tiltedNewSpace" value="true"/>
		<flag id="gc_useInlineAllocate" value="true"/>
		<flag id="gc_vlhgc" value="true"/>
		<flag id="interp_jniSupport" value="true"/>
		<flag id="interp_verbose" value="true"/>
		<flag id="ive_memorySpaceHelpers" value="false"/>
		<flag id="module_gc_check" value="true"/>
		<flag id="module_gc_modron_standard" value="true"/>
		<flag id="module_gc_realtime" value="true"/>
		<flag id="module_gc_staccato" value="true"/>
		<flag id="module_gc_trace" value="true"/>
		<flag id="module_gcchk" value="true"/>
		<flag id="opt_noClassloaders" value="false"/>
	</flags>
</feature>
