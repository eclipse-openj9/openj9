/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * ROMClassStringInternManager.hpp
 *
 */

#ifndef ROMCLASSSTRINGINTERNMANAGER_HPP_
#define ROMCLASSSTRINGINTERNMANAGER_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"
#include "ClassFileOracle.hpp"
#include "ROMClassCreationContext.hpp"
#include "ROMClassBuilder.hpp"

struct J9Pool;
struct J9ClassLoader;
struct J9UTF8;

class StringInternTable;
class SRPKeyProducer;
class SRPOffsetTable;

class ROMClassStringInternManager
{
public:
	ROMClassStringInternManager(
			ROMClassCreationContext *context,
			StringInternTable *stringInternTable,
			SRPOffsetTable *srpOffsetTable,
			SRPKeyProducer *srpKeyProducer,
			U_8 *baseAddress,
			U_8 *endAddress,
			bool isSharedROMClass,
			bool hasStringTableLock);

	void visitUTF8(U_16 cpIndex, U_16 utf8Length, U_8 *utf8Data, SharedCacheRangeInfo sharedCacheSRPRangeInfo);
	void internString(J9UTF8 *string);
	bool isInterningEnabled() const { return _context->isInterningEnabled(); }
	bool isSharedROMClass() const { return _isSharedROMClass; }

private:
	ROMClassCreationContext *_context;
	StringInternTable *_stringInternTable;
	SRPOffsetTable *_srpOffsetTable;
	SRPKeyProducer *_srpKeyProducer;
	IDATA _baseAddress;
	IDATA _endAddress;
	bool _hasStringTableLock;
	bool _isSharedROMClass;
};

#endif /* ROMCLASSSTRINGINTERNMANAGER_HPP_ */
