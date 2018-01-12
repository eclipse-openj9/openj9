/*******************************************************************************
 * Copyright (c) 2013, 2016 IBM Corp. and others
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

#ifndef _Included_com_ibm_cuda_CudaCommon
#define _Included_com_ibm_cuda_CudaCommon

#include <jni.h>
#include "j9.h"
#include "omrcuda.h"
#include "ut_cuda4j.h"

#define J9CUDA_ALLOCATE_MEMORY(byteCount) j9mem_allocate_memory(byteCount, J9MEM_CATEGORY_CUDA4J)

#define J9CUDA_FREE_MEMORY(buffer)        j9mem_free_memory(buffer)

/**
 * Throw a new CudaException with the given error code.
 *
 * @param[in] env the JNI environment handle
 * @param[in] error the CUDA runtime API error code
 */
void
throwCudaException(JNIEnv * env, int32_t error);

#endif // _Included_com_ibm_cuda_CudaCommon
