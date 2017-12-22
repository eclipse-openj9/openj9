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
#if !defined(ATTACHED_DATA_MANAGER_HPP_INCLUDED)
#define ATTACHED_DATA_MANAGER_HPP_INCLUDED

/* @ddr_namespace: default */
#include "ROMClassResourceManager.hpp"
#include "AtomicSupport.hpp"
#include "j9protos.h"
#include "ut_j9shr.h"

#include "UnitTest.hpp"

class SH_AttachedDataManager : public SH_ROMClassResourceManager
{
public:
	virtual UDATA getNumOfType(UDATA type) = 0;

	virtual UDATA getDataBytesForType(UDATA type) = 0;

class SH_AttachedDataResourceDescriptor : public SH_ResourceDescriptor
{
	public:
		typedef char* BlockPtr;

		SH_AttachedDataResourceDescriptor() :
			_dataStart(NULL),
			_dataSize(0),
			_type(J9SHR_ATTACHED_DATA_TYPE_UNKNOWN)
		{
		}

		SH_AttachedDataResourceDescriptor(U_8 *dataStart, U_32 dataSize, U_16 type) :
			_dataStart(dataStart),
			_dataSize(dataSize),
			_type(type)
		{
		}

	    ~SH_AttachedDataResourceDescriptor()
		{
		}

		virtual U_32 getResourceLength()
		{
			return _dataSize;
		}

		virtual U_16 getResourceDataSubType()
		{
			return _type;
		}

		virtual U_32 getWrapperLength()
		{
			return sizeof(AttachedDataWrapper);
		}

		virtual U_16 getResourceType()
		{
			return TYPE_ATTACHED_DATA;
		}

		virtual U_32 getAlign()
		{
			return SHC_DOUBLEALIGN;
		}

		virtual const ShcItem* wrapperToItem(const void* wrapper)
		{
			return (const ShcItem*)ADWITEM(wrapper);
		}

		virtual UDATA resourceLengthFromWrapper(const void* wrapper)
		{
			return (UDATA)ADWLEN((AttachedDataWrapper *)wrapper);
		}

		virtual const void* unWrap(const void* wrapper)
		{
			return (const void*)ADWDATA(wrapper);
		}

		virtual void writeDataToCache(const ShcItem* newCacheItem, const void* resourceAddress)
		{
			AttachedDataWrapper* adwInCache = (AttachedDataWrapper*)ITEMDATA(newCacheItem);

			adwInCache->cacheOffset = (J9SRP)((BlockPtr)resourceAddress - (BlockPtr)(adwInCache));
			adwInCache->dataLength = _dataSize;
			adwInCache->type = _type;
			/* cannot use 0 to indicate non-corrupt data; 0 is a valid cache offset */
			adwInCache->corrupt = -1;
			/* assert condition to ensure data is UDATA aligned */
			Trc_SHR_Assert_False((UDATA)ADWDATA(adwInCache) % sizeof(UDATA));
			memcpy(ADWDATA(adwInCache), (void *)_dataStart, _dataSize);
		}

		virtual void updateDataInCache(const ShcItem *cacheItem, I_32 updateAtOffset, const J9SharedDataDescriptor* data)
		{
			AttachedDataWrapper* adwInCache = (AttachedDataWrapper*)ITEMDATA(cacheItem);
			U_8 *dest;

			dest = ADWDATA(adwInCache) + updateAtOffset;
			adwInCache->corrupt = updateAtOffset;
			/* Hack for testing corrupt data in unit test */
			if ((UnitTest::ATTACHED_DATA_TEST == UnitTest::unitTest) || (UnitTest::ATTACHED_DATA_CORRUPT_COUNT_TEST == UnitTest::unitTest)) {
				return;
			}
			/* issue a write barrier */
			VM_AtomicSupport::writeBarrier();
			memcpy(dest, data->address, data->length);
			/* increment update count */
			++(adwInCache->updateCount);
			/* issue a write barrier */
			VM_AtomicSupport::writeBarrier();
			adwInCache->corrupt = -1;
		}

		virtual void updateUDATAInCache(const ShcItem *cacheItem, I_32 updateAtOffset, UDATA value)
		{
			AttachedDataWrapper* adwInCache = (AttachedDataWrapper*)ITEMDATA(cacheItem);
			UDATA *dest;

			dest = (UDATA *)(ADWDATA(adwInCache) + updateAtOffset);
			*dest = value;
		}

		virtual UDATA generateKey(const void *resourceAddress)
		{
			return (UDATA)((UDATA)resourceAddress + _type);
		}

	private:
		const U_8 *_dataStart;
		const U_32 _dataSize;
		const U_16 _type;
	};

};

#endif /* ATTACHED_DATA_MANAGER_HPP_INCLUDED */
