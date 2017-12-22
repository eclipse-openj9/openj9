/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef DBG_H
#define DBG_H

/* @ddr_namespace: default */
#ifdef __cplusplus
extern "C" {
#endif

#include "j9comp.h"

#define ClassTagClass 1
#define ClassTagInterface 2
#define ClassTagArray 3
#define J9WP_MAJOR_VERSION 2
#define J9WP_MINOR_VERSION 1
#define JDWP_BYTECODE_PC_SIZE 8
#define JDWP_FIELD_ID_SIZE 8
#define JDWP_FRAME_ID_SIZE 8
#define JDWP_METHOD_ID_SIZE 4
#define JDWP_OBJECT_ID_SIZE 4
#define JDWP_REF_TYPE_ID_SIZE 4
#define JVMDI_CLASS_STATUS_ERROR 8
#define JVMDI_CLASS_STATUS_INITIALIZED 4
#define JVMDI_CLASS_STATUS_PREPARED 2
#define JVMDI_CLASS_STATUS_VERIFIED 1
#define JVMDI_DISABLE 0
#define JVMDI_ENABLE 1
#define JVMDI_ERROR_ABSENT_INFORMATION 101
#define JVMDI_ERROR_ACCESS_DENIED 111
#define JVMDI_ERROR_ADD_METHOD_NOT_IMPLEMENTED 63
#define JVMDI_ERROR_ALREADY_INVOKING 502
#define JVMDI_ERROR_BUFFER_GROW 65535
#define JVMDI_ERROR_CIRCULAR_CLASS_DEFINITION 61
#define JVMDI_ERROR_CLASS_MODIFIERS_CHANGE_NOT_IMPLEMENTED 70
#define JVMDI_ERROR_CLASS_NOT_PREPARED 22
#define JVMDI_ERROR_DELETE_METHOD_NOT_IMPLEMENTED 67
#define JVMDI_ERROR_DUPLICATE 40
#define JVMDI_ERROR_FAILS_VERIFICATION 62
#define JVMDI_ERROR_HIERARCHY_CHANGE_NOT_IMPLEMENTED 66
#define JVMDI_ERROR_HOT_SWAP_OPERATION_IGNORED 901
#define JVMDI_ERROR_HOT_SWAP_OPERATION_REFUSED 900
#define JVMDI_ERROR_ILLEGAL_ARGUMENT 103
#define JVMDI_ERROR_INTERNAL 113
#define JVMDI_ERROR_INTERRUPT 52
#define JVMDI_ERROR_INVALID_ARRAY 508
#define JVMDI_ERROR_INVALID_CLASS 21
#define JVMDI_ERROR_INVALID_CLASS_FORMAT 60
#define JVMDI_ERROR_INVALID_CLASS_LOADER 507
#define JVMDI_ERROR_INVALID_EVENT_TYPE 102
#define JVMDI_ERROR_INVALID_FIELDID 25
#define JVMDI_ERROR_INVALID_FRAMEID 30
#define JVMDI_ERROR_INVALID_INDEX 503
#define JVMDI_ERROR_INVALID_LENGTH 504
#define JVMDI_ERROR_INVALID_LOCATION 24
#define JVMDI_ERROR_INVALID_METHODID 23
#define JVMDI_ERROR_INVALID_MONITOR 50
#define JVMDI_ERROR_INVALID_OBJECT 20
#define JVMDI_ERROR_INVALID_PRIORITY 12
#define JVMDI_ERROR_INVALID_SLOT 35
#define JVMDI_ERROR_INVALID_STRING 506
#define JVMDI_ERROR_INVALID_TAG 500
#define JVMDI_ERROR_INVALID_THREAD 10
#define JVMDI_ERROR_INVALID_THREAD_GROUP 11
#define JVMDI_ERROR_INVALID_TYPESTATE 65
#define JVMDI_ERROR_METHOD_MODIFIERS_CHANGE_NOT_IMPLEMENTED 71
#define JVMDI_ERROR_NAMES_DONT_MATCH 69
#define JVMDI_ERROR_NO_AVAILABLE_THREAD 501
#define JVMDI_ERROR_NO_MORE_FRAMES 31
#define JVMDI_ERROR_NONE 0
#define JVMDI_ERROR_NOT_CURRENT_FRAME 33
#define JVMDI_ERROR_NOT_FOUND 41
#define JVMDI_ERROR_NOT_IMPLEMENTED 99
#define JVMDI_ERROR_NOT_MONITOR_OWNER 51
#define JVMDI_ERROR_NULL_POINTER 100
#define JVMDI_ERROR_OPAQUE_FRAME 32
#define JVMDI_ERROR_OUT_OF_MEMORY 110
#define JVMDI_ERROR_SCHEMA_CHANGE_NOT_IMPLEMENTED 64
#define JVMDI_ERROR_THREAD_NOT_SUSPENDED 13
#define JVMDI_ERROR_THREAD_SUSPENDED 14
#define JVMDI_ERROR_TRANSPORT_INIT 510
#define JVMDI_ERROR_TRANSPORT_LOAD 509
#define JVMDI_ERROR_TYPE_MISMATCH 34
#define JVMDI_ERROR_UNATTACHED_THREAD 115
#define JVMDI_ERROR_VM_DEAD 112
#define JVMDI_ERROR_VM_NOT_INTERRUPTED 505
#define JVMDI_EVENT_BREAKPOINT 2
#define JVMDI_EVENT_CLASS_LOAD 10
#define JVMDI_EVENT_CLASS_PREPARE 8
#define JVMDI_EVENT_CLASS_UNLOAD 9
#define JVMDI_EVENT_COMMAND_SET 64
#define JVMDI_EVENT_DEBUG_ATTRIBUTES 1
#define JVMDI_EVENT_EXCEPTION 4
#define JVMDI_EVENT_EXCEPTION_CATCH 30
#define JVMDI_EVENT_FIELD_ACCESS 20
#define JVMDI_EVENT_FIELD_MODIFICATION 21
#define JVMDI_EVENT_FRAME_POP 3
#define JVMDI_EVENT_METHOD_ENTRY 40
#define JVMDI_EVENT_METHOD_EXIT 41
#define JVMDI_EVENT_RAM_CLASS_CREATE 2
#define JVMDI_EVENT_RAM_CLASS_CREATE_WITH_ID 3
#define JVMDI_EVENT_SINGLE_STEP 1
#define JVMDI_EVENT_THREAD_END 7
#define JVMDI_EVENT_THREAD_START 6
#define JVMDI_EVENT_USER_DEFINED 5
#define JVMDI_EVENT_VM_DEATH 99
#define JVMDI_EVENT_VM_DISCONNECTED 100
#define JVMDI_EVENT_VM_INIT 90
#define JVMDI_EXTENDED_EVENT_COMMAND_SET 192
#define JVMDI_INVOKE_NONVIRTUAL 2
#define JVMDI_INVOKE_SINGLE_THREADED 1
#define JVMDI_MAX_EVENT_TYPE_VAL 99
#define JVMDI_MOD_KIND_CLASS_EXCLUDE 6
#define JVMDI_MOD_KIND_CLASS_MATCH 5
#define JVMDI_MOD_KIND_CLASS_ONLY 4
#define JVMDI_MOD_KIND_CONDITIONAL 2
#define JVMDI_MOD_KIND_COUNT 1
#define JVMDI_MOD_KIND_EXCEPTION_ONLY 8
#define JVMDI_MOD_KIND_FIELD_ONLY 9
#define JVMDI_MOD_KIND_INSTANCE_ONLY 11
#define JVMDI_MOD_KIND_LOCATION_ONLY 7
#define JVMDI_MOD_KIND_LOCATION_RANGE 21
#define JVMDI_MOD_KIND_STEP 10
#define JVMDI_MOD_KIND_THREAD_ONLY 3
#define JVMDI_MONITOR_WAIT_FOREVER -1
#define JVMDI_STEP_DEPTH_INTO 0
#define JVMDI_STEP_DEPTH_OUT 2
#define JVMDI_STEP_DEPTH_OVER 1
#define JVMDI_STEP_DEPTH_REENTER 3
#define JVMDI_STEP_SIZE_LINE 1
#define JVMDI_STEP_SIZE_MIN 0
#define JVMDI_SUSPEND_POLICY_ALL 2
#define JVMDI_SUSPEND_POLICY_EVENT_THREAD 1
#define JVMDI_SUSPEND_POLICY_NONE 0
#define JVMDI_SUSPEND_STATUS_BREAK 2
#define JVMDI_SUSPEND_STATUS_NOT_SUSPENDED 0
#define JVMDI_SUSPEND_STATUS_SUSPENDED 1
#define JVMDI_THREAD_MAX_PRIORITY 10
#define JVMDI_THREAD_MIN_PRIORITY 1
#define JVMDI_THREAD_NORM_PRIORITY 5
#define JVMDI_THREAD_STATUS_MONITOR 3
#define JVMDI_THREAD_STATUS_RUNNING 1
#define JVMDI_THREAD_STATUS_SLEEPING 2
#define JVMDI_THREAD_STATUS_UNKNOWN -1
#define JVMDI_THREAD_STATUS_WAIT 4
#define JVMDI_THREAD_STATUS_ZOMBIE 0
#define PacketFlagFlushRemaining 2
#define PacketFlagHighPriority 1
#define PacketFlagSentByVM 128
#define PacketNoFlags 0

#define J9_LINE_TABLE_CACHE_SIZE 64

typedef struct J9SourceDebugExtension {
    U_32 size;
} J9SourceDebugExtension;

typedef struct J9MethodDebugInfo {
    J9SRP srpToVarInfo;
    U_32 lineNumberCount;
    U_32 varInfoCount;
} J9MethodDebugInfo;

#define J9METHODDEBUGINFO_SRPTOVARINFO(base) SRP_GET((base)->srpToVarInfo, U_8*)

typedef struct J9LineNumber {
    U_16 location;
    U_16 lineNumber;
} J9LineNumber;

typedef struct J9VariableInfoValues {
    struct J9UTF8* name;
    struct J9UTF8* signature;
    struct J9UTF8* genericSignature;
    U_32 startVisibility;
    U_32 visibilityLength;
    U_32 slotNumber;
} J9VariableInfoValues;

typedef struct J9VariableInfoWalkState {
    U_8* variableTablePtr;
    struct J9VariableInfoValues values;
    U_32 variablesLeft;
} J9VariableInfoWalkState;

#ifdef __cplusplus
}
#endif

#endif /* DBG_H */
