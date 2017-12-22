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
 * SuppliedBufferAllocationStrategy.hpp
 */

#ifndef SUPPLIEDBUFFERALLOCATIONSTRATEGY_HPP_
#define SUPPLIEDBUFFERALLOCATIONSTRATEGY_HPP_

/* @ddr_namespace: default */
#include "AllocationStrategy.hpp"

class SuppliedBufferAllocationStrategy: public AllocationStrategy
{
public:
	SuppliedBufferAllocationStrategy(U_8 * buffer, UDATA bufferSize) :
		_buffer(buffer),
		_bufferSize(bufferSize),
		_lineNumberBuffer(NULL),
		_lineNumberBufferSize(0),
		_variableInfoBuffer(NULL),
		_variableInfoBufferSize(0)
	{
	}

	SuppliedBufferAllocationStrategy(U_8 * buffer, UDATA bufferSize,
			U_8 *lineNumberBuffer, UDATA lineNumberBufferSize,
			U_8 * variableInfoBuffer, UDATA variableInfoBufferSize) :
		_buffer(buffer),
		_bufferSize(bufferSize),
		_lineNumberBuffer(lineNumberBuffer),
		_lineNumberBufferSize(lineNumberBufferSize),
		_variableInfoBuffer(variableInfoBuffer),
		_variableInfoBufferSize(variableInfoBufferSize)
	{
	}

	virtual U_8* allocate(UDATA byteAmount) {
		if (byteAmount <= _bufferSize) {
			return _buffer;
		}
		return NULL;
	}

	virtual bool allocateWithOutOfLineData(AllocatedBuffers *allocatedBuffers, UDATA byteAmount, UDATA lineNumberByteAmount, UDATA variableInfoByteAmount)
	{
		if (byteAmount > _bufferSize) {
			return false;
		}
		if (lineNumberByteAmount > _lineNumberBufferSize) {
			return false;
		}
		if (variableInfoByteAmount > _variableInfoBufferSize) {
			return false;
		}
		allocatedBuffers->romClassBuffer = _buffer;
		allocatedBuffers->lineNumberBuffer = _lineNumberBuffer;
		allocatedBuffers->variableInfoBuffer = _variableInfoBuffer;
		return true;
	}


	void updateFinalROMSize(UDATA finalSize) { /* do nothing */ }

	virtual bool canStoreDebugDataOutOfLine() { return true; }

private:
	U_8 * _buffer;
	UDATA _bufferSize;
	U_8 * _lineNumberBuffer;
	UDATA _lineNumberBufferSize;
	U_8 * _variableInfoBuffer;
	UDATA _variableInfoBufferSize;
};

#endif /* SUPPLIEDBUFFERALLOCATIONSTRATEGY_HPP_ */
