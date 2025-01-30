/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(J9ACCESS_H)
#define J9ACCESS_H

/* The following definitions are required by j9.h and are provided via OpenJ9
 * jni.h or the OpenJ9 build system, but needed when building outside of OpenJ9
 * with j9.h and OpenJDK jni.h is used.
 */
struct GCStatus;
typedef struct GCStatus GCStatus;
struct JavaVMQuery;
typedef struct JavaVMQuery JavaVMQuery;
struct JVMExtensionInterface_;
typedef const struct JVMExtensionInterface_ *JVMExt;

#if defined(AIX)
#define AIXPPC
#define RS6000
#elif defined(MACOSX) /* defined(AIX) */
#define OSX
#elif defined(WIN32) /* defined(MACOSX) */
#define OMR_OS_WINDOWS
#endif /* defined(WIN32) */

#include "j9.h"

#endif /* !defined(J9ACCESS_H) */
