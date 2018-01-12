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
#if !defined(COMPILED_METHOD_MANAGER_HPP_INCLUDED)
#define COMPILED_METHOD_MANAGER_HPP_INCLUDED

#include "ROMClassResourceManager.hpp"

class SH_CompiledMethodManager : public SH_ROMClassResourceManager 
{
public:

class SH_CompiledMethodResourceDescriptor : public SH_ResourceDescriptor
{
	public:
		typedef char* BlockPtr;

		SH_CompiledMethodResourceDescriptor() :
			_dataStart(0), _codeStart(0), _dataSize(0), _codeSize(0) 
		{
		}

		SH_CompiledMethodResourceDescriptor(const U_8* dataStart, U_32 dataSize, const U_8* codeStart, U_32 codeSize) :
			_dataStart(dataStart), 
			_codeStart(codeStart), 
			_dataSize(dataSize),
			_codeSize(codeSize) 
		{
		}

		~SH_CompiledMethodResourceDescriptor() 
		{
		}
		
		virtual U_32 getResourceLength() 
		{
			return _dataSize + _codeSize;
		}

		virtual U_32 getWrapperLength() 
		{
			return sizeof(CompiledMethodWrapper);
		}

		virtual U_16 getResourceType() 
		{
			return TYPE_COMPILED_METHOD;
		}

		virtual U_32 getAlign() 
		{
			return SHC_WORDALIGN;
		}

		virtual const ShcItem* wrapperToItem(const void* wrapper) 
		{
			return (const ShcItem*)CMWITEM(wrapper);
		}

		virtual UDATA resourceLengthFromWrapper(const void* wrapper) 
		{
			return ((CompiledMethodWrapper*)wrapper)->dataLength + ((CompiledMethodWrapper*)wrapper)->codeLength;
		}

		virtual const void* unWrap(const void* wrapper) 
		{
			return (const void*)CMWDATA(wrapper);
		}

		virtual void writeDataToCache(const ShcItem* newCacheItem, const void* resourceAddress) 
		{
			CompiledMethodWrapper* cmwInCache = (CompiledMethodWrapper*)ITEMDATA(newCacheItem);

			cmwInCache->dataLength = _dataSize;
			cmwInCache->codeLength = _codeSize;
			cmwInCache->romMethodOffset = (J9SRP)((BlockPtr)resourceAddress - (BlockPtr)(cmwInCache));
			memcpy(CMWDATA(cmwInCache), (void *)_dataStart, _dataSize);
			memcpy(CMWCODE(cmwInCache), (void *)_codeStart, _codeSize);
		}

		virtual UDATA generateKey(const void *resourceAddress)
		{
			return (UDATA)resourceAddress;
		}
private:
		/* Placement operator new (<new> is not included) */
		void* operator new(size_t size, void* memoryPtr) 
		{
			return memoryPtr;
		}

		const U_8* _dataStart;
		const U_8* _codeStart;
		const U_32 _dataSize;
		const U_32 _codeSize;
	};

};

#endif

