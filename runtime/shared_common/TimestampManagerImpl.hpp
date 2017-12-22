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

#if !defined(TIMESTAMPMANAGERIMPL_HPP_INCLUDED)
#define TIMESTAMPMANAGERIMPL_HPP_INCLUDED

/* @ddr_namespace: default */
#include "TimestampManager.hpp"
#include "Managers.hpp"

/* 
 * Implementation of SH_TimestampManager interface
 */
class SH_TimestampManagerImpl : public SH_TimestampManager
{
protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

public:
	/*
	 * Create a new SH_TimestampManagerImpl.
	 * 
	 * Parameters:
	 *   vm				A Java VM
	 *   memForConstructor		Memory to build this instance into
	 */
	static SH_TimestampManagerImpl* newInstance(J9JavaVM* vm, SH_TimestampManagerImpl* memForConstructor, J9SharedClassConfig* sharedClassConfig);

	/*
	 * Returns memory bytes the constructor requires to build what it needs
	 */
	static UDATA getRequiredConstrBytes(void);

	/* @see TimestampManager.hpp */
	virtual I_64 checkCPEITimeStamp(J9VMThread* currentThread, ClasspathEntryItem* cpei);

	/* @see TimestampManager.hpp */
	virtual I_64 checkROMClassTimeStamp(J9VMThread* currentThread, const char* className, UDATA classNameLen, ClasspathEntryItem* cpei, ROMClassWrapper* rcWrapper);

private:

	I_64 localCheckTimeStamp(J9VMThread* currentThread, ClasspathEntryItem* cpei, const char* className, UDATA classNameLen, ROMClassWrapper* rcWrapper);
	J9SharedClassConfig* _sharedClassConfig;
};

#endif /* !defined(TIMESTAMPMANAGERIMPL_HPP_INCLUDED) */

