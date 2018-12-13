<?xml version="1.0" encoding="UTF-8"?>
<!--
Copyright (c) 2006, 2018 IBM Corp. and others

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

<feature xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://www.ibm.com/j9/builder/feature" xsi:schemaLocation="http://www.ibm.com/j9/builder/feature feature-v2.xsd" id="core">
	<name>J9 Core</name>
	<description>Contains settings common to all specs.</description>
	<properties>
		<requiredProperty name="directoryDelimiter"/>
		<requiredProperty name="j9dt.compileTarget"/>
		<requiredProperty name="j9dt.make"/>
		<requiredProperty name="j9dt.toolsTarget"/>
		<requiredProperty name="jclMemoryMax"/>
		<requiredProperty name="jclOSStackSizeMax"/>
		<requiredProperty name="jitTestingOptLevel"/>
		<requiredProperty name="localRootPath"/>
		<requiredProperty name="longLimitCmd"/>
		<requiredProperty name="os.lineDelimiter"/>
		<requiredProperty name="uma_processor"/>
	</properties>
	<source>
		<project id="omr"/>
		<project id="buildspecs"/>
		<project id="VM_Build-Tools"/>
		<project id="runtime"/>
	</source>
	<flags>
		<flag id="build_stage_ottawa_vmlab" value="true"/>
		<flag id="build_uma" value="true"/>
		<flag id="compiler_promotion" value="true"/>
		<flag id="gc_classesOnHeap" value="true"/>
		<flag id="gc_compressedPointerBarrier" value="false"/>
		<flag id="gc_fragmentedHeap" value="true"/>
		<flag id="gc_modronGC" value="true"/>
		<flag id="gc_modronVerbose" value="true"/>
		<flag id="graph_common_jobs" value="false"/>
		<flag id="interp_bytecodePreverification" value="true"/>
		<flag id="interp_bytecodeVerification" value="true"/>
		<flag id="interp_customSpinOptions" value="true"/>
		<flag id="interp_floatSupport" value="true"/>
		<flag id="interp_floatmathTracing" value="false"/>
		<flag id="interp_floatmathlibTracing" value="false"/>
		<flag id="interp_jitOnByDefault" value="true"/>
		<flag id="interp_jniSupport" value="true"/>
		<flag id="interp_newHeaderShape" value="true"/>
		<flag id="interp_romableAotSupport" value="false"/>
		<flag id="interp_tracing" value="false"/>
		<flag id="interp_updateVMCTracing" value="false"/>
		<flag id="jit_ia32FixedFrame" value="false"/>
		<flag id="jit_cHelpers" value="true"/>
		<flag id="jit_nathelpUsesClassObjects" value="false"/>
		<flag id="module_aotrt_common" value="false"/>
		<flag id="module_avl" value="true"/>
		<flag id="module_buildtools" value="true"/>
		<flag id="module_codegen" value="false"/>
		<flag id="module_codegen_common_aot" value="false"/>
		<flag id="module_codegen_wcode" value="false"/>
		<flag id="module_exelib" value="true"/>
		<flag id="module_gc" value="true"/>
		<flag id="module_gc_api" value="true"/>
		<flag id="module_gc_base" value="true"/>
		<flag id="module_gc_doc" value="true"/>
		<flag id="module_gc_include" value="true"/>
		<flag id="module_gc_modron_base" value="true"/>
		<flag id="module_gc_modron_startup" value="true"/>
		<flag id="module_gc_stats" value="true"/>
		<flag id="module_gc_structs" value="true"/>
		<flag id="module_hashtable" value="true"/>
		<flag id="module_hookable" value="true"/>
		<flag id="module_include" value="true"/>
		<flag id="module_jcl" value="true"/>
		<flag id="module_jit_armaot" value="false"/>
		<flag id="module_jit_common_aot" value="false"/>
		<flag id="module_jit_ia32aot" value="false"/>
		<flag id="module_jit_ppcaot" value="false"/>
		<flag id="module_jit_s390aot" value="false"/>
		<flag id="module_omr" value="true"/>
		<flag id="module_oti" value="true"/>
		<flag id="module_pool" value="true"/>
		<flag id="module_port" value="true"/>
		<flag id="module_redirect" value="false"/>
		<flag id="module_simplepool" value="true"/>
		<flag id="module_srphashtable" value="true"/>
		<flag id="module_stackmap" value="true"/>
		<flag id="module_thread" value="true"/>
		<flag id="module_util" value="true"/>
		<flag id="module_util_core" value="true"/>
		<flag id="module_verutil" value="true"/>
		<flag id="module_vm" value="true"/>
		<flag id="opt_fragmentRamClasses" value="true"/>
		<flag id="opt_inlineJsrs" value="true"/>
		<flag id="opt_module" value="true"/>
		<flag id="opt_newObjectHash" value="true"/>
		<flag id="opt_newRomClassBuilder" value="true"/>
		<flag id="opt_phpSupport" value="false"/>
		<flag id="opt_romImageSupport" value="true"/>
		<flag id="opt_useOmrDdr" value="false"/>
		<flag id="opt_veeSupport" value="false"/>
		<flag id="opt_vmLocalStorage" value="true"/>
		<flag id="prof_countArgsTemps" value="false"/>
		<flag id="prof_jvmti" value="false"/>
		<flag id="thr_extraChecks" value="false"/>
		<flag id="thr_lockNurseryFatArrays" value="false"/>
		<flag id="thr_preemptive" value="true"/>
		<flag id="uma_tracegenc" value="false"/>
	</flags>
</feature>
