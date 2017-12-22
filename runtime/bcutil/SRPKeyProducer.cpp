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
 * SRPKeyProducer.cpp
 */

#include "SRPKeyProducer.hpp"
#include "ClassFileOracle.hpp"

#include "ut_j9bcu.h"

SRPKeyProducer::SRPKeyProducer(ClassFileOracle *classFileOracle) :
	_cfrConstantPoolCount(classFileOracle->getConstantPoolCount()),
	_methodCount(classFileOracle->getMethodsCount()),
	_startStackMapKeys(0),
	_startMethodDebugInfoKeys(0),
	_startVariableInfoKeys(0),
	_maxKey(0),
	_getMaxKeyWasCalled(false)
{
	_startStackMapKeys = _cfrConstantPoolCount;
	_startMethodDebugInfoKeys = _startStackMapKeys + UDATA(_methodCount);
	_startVariableInfoKeys = _startMethodDebugInfoKeys + UDATA(_methodCount);
	_maxKey = _startVariableInfoKeys + UDATA(_methodCount);
}

UDATA
SRPKeyProducer::generateKey()
{
	Trc_BCU_Assert_Equals(false, _getMaxKeyWasCalled);

	return ++_maxKey;
}

UDATA
SRPKeyProducer::getMaxKey()
{
	_getMaxKeyWasCalled = true;
	return _maxKey;
}
