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

#if !defined(CLASSPATHMANAGER_HPP_INCLUDED)
#define CLASSPATHMANAGER_HPP_INCLUDED

/* @ddr_namespace: default */
#include "Manager.hpp"
#include "ClasspathItem.hpp"

#define MAX_CLASS_PATH_ENTRIES 0x7fff

/**
 * Sub-interface of SH_Manager used for managing Classpaths in the cache
 *
 * @see SH_ClasspathManagerImpl.hpp
 * @ingroup Shared_Common
 */
class SH_ClasspathManager : public SH_Manager
{
public:
	virtual IDATA update(J9VMThread* currentThread, ClasspathItem* cp, I_16 cpeIndex, ClasspathWrapper** foundCP) = 0;

	virtual IDATA validate(J9VMThread* currentThread, ROMClassWrapper* foundROMClass, ClasspathItem* compareTo, IDATA confirmedEntries, I_16* foundAtIndex, ClasspathEntryItem** staleItem) = 0;
	
	virtual void notifyClasspathEntryStateChange(J9VMThread* currentThread, const J9UTF8* path, UDATA newState) = 0;

	virtual void markClasspathsStale(J9VMThread* currentThread, ClasspathEntryItem* cpei) = 0;

	virtual void setTimestamps(J9VMThread* currentThread, ClasspathWrapper* cpw) = 0;
 
	virtual bool isStale(ClasspathWrapper* cpw) = 0;

	virtual bool touchForClassFiles(J9VMThread* currentThread, const char* className, UDATA classNameLen, ClasspathItem* cp, I_16 toIndex) = 0;

	virtual void getNumItemsByType(UDATA* numClasspaths, UDATA* numURLs, UDATA* numTokens) = 0;
};

#endif /* !defined(CLASSPATHMANAGER_HPP_INCLUDED) */


