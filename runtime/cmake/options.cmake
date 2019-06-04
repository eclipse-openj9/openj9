################################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
################################################################################

# This replicates the options from j9.flags that I felt were important. Blank lines
# mark where I skipped options which seemed unimportant.
# TODO: some options still need to be ported over
# TODO: at some point we should go through and clean out options which no longer work / make sense

option(J9VM_INTERP_AOT_COMPILE_SUPPORT "Controls if the AOT compilation support is included in the VM")
option(J9VM_INTERP_AOT_RUNTIME_SUPPORT "Controls if the AOT runtime support is included in the VM")
option(J9VM_INTERP_ATOMIC_FREE_JNI "Use the new atomic-free JNI support")
option(J9VM_INTERP_BYTECODE_PREVERIFICATION "Does this VM support 1st pass bytecode verification (able to dynamically generate pre-verify data)")
option(J9VM_INTERP_BYTECODE_VERIFICATION "Does this VM support 2nd pass bytecode verification (pre-verify data in .jxe only)")


option(J9VM_INTERP_DEBUG_SUPPORT "Controls if a debugging support is included in the VM.")
option(J9VM_INTERP_ENABLE_JIT_ON_DESKTOP "TEMPORARY FLAG for arm test environment")


option(J9VM_INTERP_FLOAT_SUPPORT "Determines if float and double data types are supported by the VM.")


option(J9VM_INTERP_GP_HANDLER "Determines if protection faults are caught by the VM and handled cleanly.")
option(J9VM_INTERP_GP_GROWABLE_STACKS "Enables dynamic java stack growing")


option(J9VM_INTERP_JIT_ON_BY_DEFAULT "Turns JIT on by default")
option(J9VM_INTERP_JNI_SUPPORT "Determines if JNI native support is available.")



option(J9VM_INTERP_NATIVE_SUPPORT "Controls if the native translator is included in the VM.")
option(J9VM_INTERP_NEW_HEADER_SHAPE "Temporary flag for experimentation with the object header shape.")
option(J9VM_INTERP_PROFILING_BYTECODES "If enabled, profiling versions of branch and call bytecodes are available. These gather additional info for the JIT for optimal code generation")
option(J9VM_INTERP_ROMABLE_AOT_SUPPORT "ROMable AOT Support for TJ Watson")


option(J9VM_INTERP_TRACING "DEBUGGING FEATURE.  Determines if the interpreter produces debug information at every bytecode.")


option(J9VM_INTERP_USE_SPLIT_SIDE_TABLES "Use split side tables for handling constant pool entry shared between multiple invoke bytecodes")
option(J9VM_INTERP_USE_UNSAFE_HELPER "If set, use helper functions in UnsafeAPI to access native memory")
option(J9VM_INTERP_VERBOSE "Determines if verbose reporting is available. (ie. -verbose)")

option(J9VM_MODULE_A2E "Enables compilation of the a2e module.")
option(J9VM_MODULE_ALGORITHM_TEST "Enables compilation of the algorithm_test module.")
option(J9VM_MODULE_AOTRT_COMMON "Enables compilation of the aotrt_common module.")

option(J9VM_MODULE_BCUTIL "Enables compilation of the bcutil module.")
option(J9VM_MODULE_BCVERIFY "Enables compilation of the bcverify module.")



option(J9VM_MODULE_CASSUME "Enables compilation of the cassume module.")
option(J9VM_MODULE_CFDUMPER "Enables compilation of the cfdumper module.")


option(J9VM_MODULE_DBGEXT "Enables compilation of the dbgext module.")



option(J9VM_MODULE_EXELIB "Enables compilation of the exelib module.")


option(J9VM_MODULE_VM "Enables compilation of the vm module.")
option(J9VM_MODULE_WINDBG "Enables compilation of the windbg module.")
option(J9VM_MODULE_ZIP "Enables compilation of the zip module.")
option(J9VM_MODULE_ZLIB "Enables compilation of the zlib module.")



option(J9VM_OPT_CUDA "Add support for CUDA")


option(J9VM_OPT_DYNAMIC_LOAD_SUPPORT "Determines if the dynamic loader is included.")
option(J9VM_OPT_FIPS "Add supports for FIPs")
option(J9VM_OPT_FRAGMENT_RAM_CLASSES "Transitional flag for the the GC during the switch to fragmented RAM class allocation")
option(J9VM_OPT_HARMONY "Support the Open Source Harmony project")


option(J9VM_OPT_JVMTI "Support for the JVMTI interface")
option(J9VM_OPT_JXE_LOAD_SUPPORT "Controls if main will allow -jxe: and relocate the disk image for you.")


option(J9VM_OPT_METHOD_HANDLE "Turns on methodhandle support")
option(J9VM_OPT_MODULE "Turns on module support")
option(J9VM_OPT_MULTI_VM "Decides if multiple VMs can be created in the same address space")


option(J9VM_OPT_PANAMA "Enables support for Project Panama features such as native method handles")
option(J9VM_OPT_VALHALLA_MVT "Enables support for Project Valhalla Minimal Value Types")
option(J9VM_OPT_VALHALLA_NESTMATES "Enables support for Project Valhalla Nest Mates")
option(J9VM_OPT_VALHALLA_VALUE_TYPES "Enables support for Project Valhalla Value Types")

option(J9VM_OPT_ROM_IMAGE_SUPPORT "Controls if the VM includes basic support for linked rom images")
option(J9VM_OPT_SHARED_CLASSES "Support for class sharing")

option(J9VM_OPT_ZIP_SUPPORT "Controls if the VM includes zip reading and caching support. (implies dynamic loading)")
option(J9VM_OPT_ZLIB_COMPRESSION "Controls if the compression routines in zlib are included.")
option(J9VM_OPT_ZLIB_SUPPORT "Controls if the VM includes the zlib compression library.")

option(J9VM_PORT_RUNTIME_INSTRUMENTATION "Controls whether runtime instrumentation support exists on this platform.")



option(J9VM_RAS_DUMP_AGENTS "Support multiple dump agents, instead of just console dump.")
option(J9VM_RAS_EYE_CATCHERS "Add eyecatcher blocks to key structures")
