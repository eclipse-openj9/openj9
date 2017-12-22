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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
/*
 * ROMClassStringInternManager.cpp
 *
 */

#include "ROMClassStringInternManager.hpp"
#include "ROMClassBuilder.hpp"
#include "SRPKeyProducer.hpp"
#include "SRPOffsetTable.hpp"

#include "ut_j9bcu.h"
#include "bcutil_api.h"

ROMClassStringInternManager::ROMClassStringInternManager(
		ROMClassCreationContext *context,
		StringInternTable *stringInternTable,
		SRPOffsetTable *srpOffsetTable,
		SRPKeyProducer *srpKeyProducer,
		U_8 *baseAddress,
		U_8 *endAddress,
		bool isSharedROMClass,
		bool hasStringTableLock) :
		_context(context),
		_stringInternTable(stringInternTable),
		_srpOffsetTable(srpOffsetTable),
		_srpKeyProducer(srpKeyProducer),
		_baseAddress(IDATA(baseAddress)),
		_endAddress(IDATA(endAddress)),
		_hasStringTableLock(hasStringTableLock),
		_isSharedROMClass(isSharedROMClass)
{
}

/**
 * This function visits the passed UTF8 if it is in any string intern table.
 * By visiting meaning that it marks the node as used and 
 * marks the corresponding index SRP Offset table as interned 
 * if it is found in shared or local string intern table.
 * 
 * @param cpIndex Index in SRP Offset table
 * @param utfLength The length of the UTF8 string
 * @param utf8Data A pointer to the start of the UTF8 string.
 * @param sharedCacheSRPRangeInfo Tells whether shared cache(s) are accessible by rom class. Possible values are:
		 *  1: (SC_COMPLETELY_OUT_OF_THE_SRP_RANGE) Shared cache is out of the SRP range
		 *  2: (SC_COMPLETELY_IN_THE_SRP_RANGE) Shared cache is in the SRP range
		 *  3: (SC_PARTIALLY_IN_THE_SRP_RANGE)	Part of shared cache is in the SRP range, part of it is not.
 * @return void
 *
 */
void
ROMClassStringInternManager::visitUTF8(U_16 cpIndex, U_16 utf8Length, U_8 *utf8Data, SharedCacheRangeInfo sharedCacheSRPRangeInfo)
{
	if ( !isInterningEnabled() ) {
		return;
	}

	J9SharedInvariantInternTable *sharedTable = NULL;

	/* If we don't have the cross-process mutex, it's not safe to search the shared tree */
	if (_hasStringTableLock) {
		sharedTable = _context->sharedStringInternTable();
	}

	J9InternSearchInfo searchInfo;

	searchInfo.stringData = utf8Data;
	searchInfo.stringLength = utf8Length;
	searchInfo.classloader = (_isSharedROMClass ? _context->javaVM()->systemClassLoader : _context->classLoader());
	searchInfo.romClassBaseAddr = (U_8 *)_baseAddress;
	searchInfo.romClassEndAddr = (U_8 *)_endAddress;
	searchInfo.sharedCacheSRPRangeInfo = sharedCacheSRPRangeInfo;

	J9InternSearchResult result;
	if (_stringInternTable->findUtf8(&searchInfo, sharedTable, _isSharedROMClass, &result)) {
		IDATA internedString = IDATA(result.utf8);
		_stringInternTable->markNodeAsUsed(&result, sharedTable);
		_srpOffsetTable->setInternedAt(_srpKeyProducer->mapCfrConstantPoolIndexToKey(cpIndex), (U_8 *)internedString);
	}
}

/**
 * This function is used to add the given UTF8 into one of the string intern tables.
 * It is added into shared string intern table if it is in the shared cache and 
 * if there is an empty spot in shared string intern table.
 * In all other cases, it is added to local string intern table.
 * @param string Given UTF8
 * @return void
 *
 */
void
ROMClassStringInternManager::internString(J9UTF8 *string)
{
	if ( !isInterningEnabled() ) {
		return;
	}

	J9SharedInvariantInternTable *sharedTable = NULL;

	/* If we don't have the cross-process mutex, it's not safe to search the shared tree */
	if (_hasStringTableLock && _isSharedROMClass) {
		sharedTable = _context->sharedStringInternTable();
	}

	/* Local tree nodes that point to shared string data must
	 * have their 'classloader' field set to the system classloader. This
	 * is to allow various asserts to pass when running
	 * -Xshareclasses:verifyInternTree, and also to maintain the same
	 * intern tree sorting as in the past.
	 */
	J9ClassLoader *classLoader = (_isSharedROMClass ? _context->javaVM()->systemClassLoader : _context->classLoader());
	_stringInternTable->internUtf8(string, classLoader, _isSharedROMClass, sharedTable);
}

