/*******************************************************************************
 * Copyright (c) 2002, 2017 IBM Corp. and others
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

#ifndef jvm_generated_h
#define jvm_generated_h

#ifdef __cplusplus
extern "C" {
#endif

<#-- Include the data table which defines forwarded functions -->
<#include "../redirector/forwarders.ftl" >

<#-- Determine if JNIEXPORT prefix is required -->
<#function exportPrefix callconv>
	<#if callconv == "JNICALL">
		<#return "JNIEXPORT" />
	</#if>
	<#return "" />
</#function>

/** WARNING: Automatically Generated File
 *
 * This file contains automatically generated function prototypes
 * for Sun VM Interface (i.e. JVM_) functions. It is created during
 * makefile generation.
 *
 * DO NOT ADD PROTOTYPES MANUALLY, instead modify the table in:
 * VM_Redirector\redirector\forwarders.ftl
 *
 * Generated prototypes for all forwarded functions, see
 * redirector/forwarders.ftl for source data.
 */

<#list functions as function>
<#if function.if??>
#if ${function.if}
<#elseif function.ifdef??>
#ifdef ${function.ifdef}
<#elseif function.ifndef??>
#ifndef ${function.ifndef}
</#if>
${function.return} ${function.cc}
${function.name}(${function.args});
<#if function.if?? | function.ifdef?? | function.ifndef??>
#endif
</#if>
</#list>

#ifdef __cplusplus
}
#endif

#endif /* jvm_generated_h */
