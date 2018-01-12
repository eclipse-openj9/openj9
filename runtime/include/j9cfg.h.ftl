/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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

#ifndef J9CFG_H
#define J9CFG_H

/*
 * @ddr_namespace: map_to_type=J9BuildFlags
 * @ddr_options: valuesandbuildflags
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "omrcfg.h"

#define J9_COPYRIGHT_STRING "(c) Copyright 1991, ${uma.year} IBM Corp. and others."

/* undef these flags temporarily so they do not conflict with the version defined in omrcfg.h */
#undef EsVersionMajor
#define EsVersionMajor ${uma.buildinfo.version.major}
#undef EsVersionMinor
#define EsVersionMinor ${uma.buildinfo.version.minor}0

#define EsVersionString "${uma.buildinfo.version.major}.${uma.buildinfo.version.minor}"
#define EsExtraVersionString ""

/*  Note: The following defines record flags used to build VM.  */
/*  Changing them here does not remove the feature and may cause linking problems. */

<#list uma.spec.flags as flag>
<#if flag.enabled && flag.cname_defined>
#define ${flag.cname}
</#if>
</#list>

/* flags NOT used by this VM.  */
<#list uma.spec.flags as flag>
<#if !flag.enabled && flag.cname_defined>
#undef ${flag.cname}
</#if>
</#list>

#ifdef __cplusplus
}
#endif

#endif /* J9CFG_H */
