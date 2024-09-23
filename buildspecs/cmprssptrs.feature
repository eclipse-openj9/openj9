<?xml version="1.0" encoding="UTF-8"?>

<!--
  Copyright IBM Corp. and others 2006
 
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
  [2] https://openjdk.org/legal/assembly-exception.html

  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->

<feature xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://www.ibm.com/j9/builder/feature" xsi:schemaLocation="http://www.ibm.com/j9/builder/feature feature-v2.xsd" id="cmprssptrs">
	<name>Compressed References</name>
	<description></description>
	<properties/>
	<source/>
	<flags>
		<flag id="env_data64" value="true"/>
		<flag id="gc_classesOnHeap" value="true"/>
		<flag id="gc_compressedPointers" value="true"/>
		<flag id="gc_enableSparseHeapAllocation" value="true"/>
		<flag id="gc_objectAccessBarrier" value="true"/>
		<flag id="interp_compressedObjectHeader" value="true"/>
		<flag id="interp_smallMonitorSlot" value="true"/>
	</flags>
</feature>
