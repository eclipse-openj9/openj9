/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
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
 * SRPOffsetTable.cpp
 */

#include "SRPOffsetTable.hpp"

#include "BufferManager.hpp"
#include "ROMClassCreationContext.hpp"
#include "ROMClassVerbosePhase.hpp"
#include "SRPKeyProducer.hpp"
#include "ut_j9bcu.h"

SRPOffsetTable::SRPOffsetTable(SRPKeyProducer *srpKeyProducer, BufferManager *bufferManager, UDATA maxTag, ROMClassCreationContext *context) :
	_maxKey( srpKeyProducer->getMaxKey() ),
	_maxTag( maxTag ),
	_table( NULL ),
	_baseAddresses( NULL ),
	_bufferManager( bufferManager ),
	_buildResult( OutOfMemory )
{
	ROMClassVerbosePhase v(context, SRPOffsetTableCreation, &_buildResult);

	_table = (Entry *) _bufferManager->alloc( sizeof(Entry) * (_maxKey + 1) );
	if ( NULL == _table ) {
		return;
	}

	_baseAddresses = (U_8 **) _bufferManager->alloc( sizeof(U_8 *) * (_maxTag + 1) );
	if ( NULL == _baseAddresses ) {
		_bufferManager->free(_table);
		_table = NULL;
		return;
	}

	memset( _table, 0, sizeof(Entry) * (_maxKey + 1) );
	memset( _baseAddresses, 0, sizeof(U_8 *) * (_maxTag + 1) );

	_buildResult = OK;
}

SRPOffsetTable::~SRPOffsetTable()
{
	_bufferManager->free( _table );
	_bufferManager->free( _baseAddresses );
}

void
SRPOffsetTable::insert(UDATA key, UDATA offset, UDATA tag)
{
	Trc_BCU_Assert_NotGreaterThan( key, _maxKey );
	Trc_BCU_Assert_NotGreaterThan( tag, _maxTag );

	if (0 == key) {
		Trc_BCU_Assert_ShouldNeverHappen();
	}

	Trc_BCU_Assert_Equals(false, _table[ key ].interned);

	_table[ key ].offset = offset;
	_table[ key ].tag = tag;
	_table[ key ].marked = true;
}

UDATA
SRPOffsetTable::get(UDATA key)
{
	Trc_BCU_Assert_NotGreaterThan( key, _maxKey );

	return _table[ key ].offset;
}

J9SRP
SRPOffsetTable::computeSRP(UDATA key, J9SRP *srpAddr)
{
	Trc_BCU_Assert_NotGreaterThan( key, _maxKey );

	if ( _table[ key ].marked ) {
		return J9SRP(_baseAddresses[ _table[ key ].tag ] + _table[key].offset - (U_8 *)srpAddr);
	} else if (_table[ key ].interned) {
		return J9SRP(IDATA(_table[ key ].offset) - IDATA(srpAddr));
	}
	return 0;
}

J9WSRP
SRPOffsetTable::computeWSRP(UDATA key, J9WSRP *wsrpAddr)
{
	Trc_BCU_Assert_NotGreaterThan( key, _maxKey );

	if ( _table[ key ].marked ) {
		return J9WSRP(_baseAddresses[ _table[ key ].tag ] + _table[key].offset - (U_8 *)wsrpAddr);
	} else if (_table[ key ].interned) {
		return J9WSRP(IDATA(_table[ key ].offset) - IDATA(wsrpAddr));
	}
	return 0;
}

bool
SRPOffsetTable::isNotNull(UDATA key)
{
	return _table[ key ].marked || _table[ key ].interned;
}

bool
SRPOffsetTable::isInterned(UDATA key)
{
	return _table[ key ].interned;
}

void
SRPOffsetTable::setInternedAt(UDATA key, U_8 *address)
{
	_table[key].interned = true;
	_table[key].marked = false;
	_table[key].offset = UDATA(address);
}

void
SRPOffsetTable::setBaseAddressForTag(UDATA tag, U_8 *address)
{
	Trc_BCU_Assert_NotGreaterThan( tag, _maxTag );

	_baseAddresses[ tag ] = address;
}

U_8 *
SRPOffsetTable::getBaseAddressForTag(UDATA tag)
{
	Trc_BCU_Assert_NotGreaterThan( tag, _maxTag );

	return _baseAddresses[ tag ];
}

void
SRPOffsetTable::clear()
{
	memset( _table, 0, sizeof(Entry) * (_maxKey + 1) );
}
