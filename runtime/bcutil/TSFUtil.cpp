/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
/*
 * TSFUtil.cpp
 */

#include "TSFUtil.hpp"
#include "j9.h"
#include "bcutil_api.h"
#include "pool_api.h"

#define AS_U_8(str) ((const U_8*)str)

#ifndef NULL
#define NULL 0
#endif

const U_8* TSFUtil::METHOD_TCLINIT=AS_U_8("tclinit");
const U_8* TSFUtil::DESC_TCLINIT=AS_U_8("()V");
const U_8* TSFUtil::METHOD_CLINIT=AS_U_8("<clinit>");
const U_8* TSFUtil::DESC_CLINIT=AS_U_8("()V");

TSFUtil::TSFUtil()
{
    //ctor
}

TSFUtil::~TSFUtil()
{
    //dtor
}

UDATA
TSFUtil::U_8_len(const U_8 *str){
	UDATA i = 0;
	for(const U_8* iter = str; *iter != 0; ++iter, ++i){}
	return i;
}

TSFUtil::CompareResult
TSFUtil::compare(const UDATA strLen, const U_8 *str, const U_8 *target){
	const U_8 *iter1 = str;
	const U_8 *iter2 = target;
	UDATA i = 0;
	for(; i < strLen && *iter2 != 0; ++iter1, ++iter2, ++i){
		if(*iter1 != *iter2){
			return missMatch;
		}
	}

	if(i == strLen && 0 == *iter2){
		return equal;
	}else if(i == strLen){
		return contained;
	}else{
		return header;
	}
}


bool
TSFUtil::isFilterClass(J9TenantGlobals* tenantGlobals, const UDATA nameLength, const U_8 *className)
{
	pool_state state;
	J9TenantIsolationFilter* filter = NULL;

	filter = (J9TenantIsolationFilter*)pool_startDo(tenantGlobals->isolationControlPool, &state);
	while (NULL != filter) {
		if (filter->startsWith) {
			if(isHeader(nameLength, className, filter->filter)){
				return true;
			}
		} else {
			if(isEqual(nameLength, className, filter->filter)){
				return true;
			}
		}
		filter = (J9TenantIsolationFilter*)pool_nextDo(&state);
	}
	return false;
}


bool
TSFUtil::isTclinit(const UDATA nameLen, const U_8 *name, const UDATA descLen, const U_8* desc){
	return isEqual(nameLen, name, METHOD_TCLINIT) && isEqual(descLen, desc, DESC_TCLINIT);
}


bool
TSFUtil::isClinit(const UDATA nameLen, const U_8 *name, const UDATA descLen, const U_8* desc){
	return isEqual(nameLen, name, METHOD_CLINIT) && isEqual(descLen, desc, DESC_CLINIT);
}

