/*******************************************************************************
 * Copyright (c) 2001, 2008 IBM Corp. and others
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
/* generated.c */

#include <stdio.h>
#include "j9comp.h"
#include "dfix.h"

<#-- Include the data table which defines forwarded functions -->
<#include "dynamic.ftl" >

/* Declare a static variable to hold each dynamically resolved function pointer. */
<#list functions as function>
typedef ${function.return} (${function.cc} *${function.name}_func_t)(${function.args});
${function.name}_func_t ${function.name}_impl;
</#list>

<#-- Utility function to compute argument names -->
<#function extractNames args>
	<#if args == "void">
		<#return "" />
	</#if>
	<#assign result = "" >
	<#list args?split(",") as x>
		<#assign filtered = x?replace("*","") >
		<#assign chunks = filtered?word_list >
		<#assign result = result + chunks?last + ", " >
	</#list>
	
	<#-- Chop off the trailing comma and space -->
	<#return result?substring(0, (result?length) - 2) />
</#function>


<#function invokePrefix returnType>
	<#if returnType?trim == "void">
		<#return "" />
	<#else>
		<#return "return" />
	</#if>
</#function>


/* Prototype the intrinsic: http://msdn.microsoft.com/en-us/library/64ez38eh%28vs.71%29.aspx */
void * _ReturnAddress(void);

/* Prototype the fixup function */
void fixup(void* returnAddress, void* resolved);

<#list functions as function>
${function.return} ${function.cc}
${function.name}(${function.args})
{
	fixup(_ReturnAddress(),${function.name}_impl);
	${invokePrefix(function.return)} (*${function.name}_impl)( ${extractNames( function.args )} );
}
</#list>
