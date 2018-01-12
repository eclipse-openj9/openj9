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
 * SRPKeyProducer.hpp
 */

#ifndef SRPKEYPRODUCER_HPP_
#define SRPKEYPRODUCER_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"
#include "ut_j9bcu.h"

class ClassFileOracle;

class SRPKeyProducer
{
public:
	SRPKeyProducer(ClassFileOracle *classFileOracle);

	/* generateKey can no-longer be called after getMaxKey has been called */
	UDATA generateKey();
	UDATA getMaxKey();

	UDATA mapCfrConstantPoolIndexToKey(U_16 index)
	{
		Trc_BCU_Assert_LessThan(index, _cfrConstantPoolCount);
		return index;
	}

	U_16 mapKeyToCfrConstantPoolIndex(UDATA key)
	{
		Trc_BCU_Assert_LessThan(key, _cfrConstantPoolCount);
		return U_16(key);
	}

	UDATA mapMethodIndexToStackMapKey(U_16 index)
	{
		Trc_BCU_Assert_LessThan(index, _methodCount);
		return UDATA(index) + _startStackMapKeys;
	}

	UDATA mapMethodIndexToMethodDebugInfoKey(U_16 index)
	{
		Trc_BCU_Assert_LessThan(index, _methodCount);
		return UDATA(index) + _startMethodDebugInfoKeys;
	}

	UDATA mapMethodIndexToVariableInfoKey(U_16 index)
	{
		Trc_BCU_Assert_LessThan(index, _methodCount);
		return UDATA(index) + _startVariableInfoKeys;
	}

	bool isKeyToCfrConstantPoolItem(UDATA key) { return key < _cfrConstantPoolCount;}

private:
	U_16 _cfrConstantPoolCount;
	U_16 _methodCount;
	UDATA _startStackMapKeys;
	UDATA _startMethodDebugInfoKeys;
	UDATA _startVariableInfoKeys;
	UDATA _maxKey;
	bool _getMaxKeyWasCalled;
};

#endif /* SRPKEYPRODUCER_HPP_ */
