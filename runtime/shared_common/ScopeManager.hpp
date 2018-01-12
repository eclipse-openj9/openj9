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

/**
 * @file
 * @ingroup Shared_Common
 */

#if !defined(SCOPEMANAGER_HPP_INCLUDED)
#define SCOPEMANAGER_HPP_INCLUDED

/* @ddr_namespace: default */
#include "Manager.hpp"

/**
 * Sub-interface of SH_Manager used for managing Scopes in the cache
 *
 * @see SH_ScopeManagerImpl.hpp
 * @ingroup Shared_Common
 */
class SH_ScopeManager : public SH_Manager
{
public:
	typedef char* BlockPtr;

	virtual const J9UTF8* findScopeForUTF(J9VMThread* currentThread, const J9UTF8* localScope) = 0;

	virtual IDATA validate(J9VMThread* currentThread, const J9UTF8* partition, const J9UTF8* modContext, const ShcItem* item) = 0;
};

#endif /* !defined(SCOPEMANAGER_HPP_INCLUDED) */


