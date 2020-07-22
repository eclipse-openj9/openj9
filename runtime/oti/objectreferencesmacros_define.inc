/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#if defined(OMR_GC_COMPRESSED_POINTERS)
#if defined(OMR_GC_FULL_POINTERS)
/* Mixed mode - necessarily 64-bit */
#if defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
#define J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES
#define J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm) OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES
#else /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#define J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) (0 != (vmThread)->compressObjectReferences)
#define J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm) J9_ARE_ANY_BITS_SET((vm)->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_COMPRESS_OBJECT_REFERENCES)
#endif /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#else /* OMR_GC_FULL_POINTERS */
/* Compressed only - necessarily 64-bit */
#define J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) TRUE
#define J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm) TRUE
#endif /* OMR_GC_FULL_POINTERS */
#else /* OMR_GC_COMPRESSED_POINTERS */
/* Full only - could be 32 or 64-bit */
#define J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread) FALSE
#define J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm) FALSE
#endif /* OMR_GC_COMPRESSED_POINTERS */
