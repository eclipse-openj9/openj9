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
 * SRPOffsetTable.hpp
 */

#ifndef SRPOFFSETTABLE_HPP_
#define SRPOFFSETTABLE_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"
#include "j9.h"
#include "BuildResult.hpp"

class BufferManager;
class SRPKeyProducer;
class ROMClassCreationContext;

class SRPOffsetTable
{
public:
	SRPOffsetTable(SRPKeyProducer *srpKeyProducer, BufferManager *bufferManager, UDATA maxTag, ROMClassCreationContext *context);
	~SRPOffsetTable();

	bool isOK() { return OK == _buildResult; }
	BuildResult getBuildResult() { return _buildResult; }

	void insert(UDATA key, UDATA offset, UDATA tag);
	UDATA get(UDATA key);
	J9SRP computeSRP(UDATA key, J9SRP *srpAddr);
	J9WSRP computeWSRP(UDATA key, J9WSRP *wsrpAddr);

	bool isNotNull(UDATA key);

	bool isInterned(UDATA key);
	void setInternedAt(UDATA key, U_8 *address);

	void setBaseAddressForTag(UDATA tag, U_8 *address);
	U_8 *getBaseAddressForTag(UDATA tag);

	void clear();

private:
	struct Entry
	{
		UDATA tag;
		UDATA offset;
		bool marked;
		bool interned;
	};

	UDATA _maxKey;
	UDATA _maxTag;
	Entry *_table;
	U_8 **_baseAddresses;
	BufferManager *_bufferManager;
	BuildResult _buildResult;
};

#endif /* SRPOFFSETTABLE_HPP_ */
