/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

/* This file provides a means to supply vendor specific version info such as 
 * short name, SHA, and version string.
 * These vendor version info can be defined either in this file or other places.
 * 
 * Example usage for inclusion of a vendor name and repository sha.  These values
 * will be inserted into the java.fullversion and java.vm.info system properties
 * and in a generated javacore file.
 *
 * Note: The default Java full version string buffer size is 512, if the combined
 * length of VENDOR_SHORT_NAME and VENDOR_SHA (including white space and control
 * characters) is longer than 350 characters, please increase the buffer size at
 *     openj9/runtime/jcl/common/jclcinit.c - Line 74
 *
 * #define VENDOR_SHORT_NAME "ABC"
 * #define VENDOR_SHA "1a2b3c4"
 *
 * Example usage for inclusion of a vendor version string.
 * This value will be stored in the system property java.vm.version.
 * #define J9JVM_VERSION_STRING "0.8.1"
 */

#ifndef vendor_version_h
#define vendor_version_h

#include "openj9_version_info.h"

#define VENDOR_SHORT_NAME "OpenJ9"

#define JAVA_VM_VENDOR "Eclipse OpenJ9"
#define JAVA_VM_NAME "Eclipse OpenJ9 VM"

#if JAVA_SPEC_VERSION < 12
/* Pre-JDK12 versions use following defines to set system properties
 * java.vendor and java.vendor.url within vmprop.c:initializeSystemProperties(vm).
 * JDK12 (assuming future versions as well) sets these properties via java.lang.VersionProps.init(systemProperties) 
 * and following settings within System.ensureProperties().
 */
#define JAVA_VENDOR "Eclipse OpenJ9"
#define JAVA_VENDOR_URL "http://www.eclipse.org/openj9"
#endif /* JAVA_SPEC_VERSION < 12 */

#endif     /* vendor_version_h */
