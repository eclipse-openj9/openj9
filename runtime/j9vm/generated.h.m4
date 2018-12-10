/*******************************************************************************
 * Copyright (c) 2002, 2018 IBM Corp. and others
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
changequote(`[',`]')dnl

#ifndef jvm_generated_h
#define jvm_generated_h

#include "j9cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** WARNING: Automatically Generated File
 *
 * This file contains automatically generated function prototypes
 * for Sun VM Interface (i.e. JVM_) functions.
 *
 * DO NOT ADD PROTOTYPES MANUALLY, instead modify the table in:
 * redirector/forwarders.m4
 *
 * Generated prototypes for all forwarded functions, see
 * redirector/forwarders.m4 for source data.
 */

include([helpers.m4])
define([_IF],
[#if $1
$2
dnl #endif /* $1 */
#endif
])
dnl        (1-name,2-cc, 3-decorate, 4-ret, 5-args..)
define([_X],
[$4 $2
$1(join([, ],mshift(4,$@)));])

include([forwarders.m4])

#ifdef __cplusplus
}
#endif

#endif /* jvm_generated_h */
