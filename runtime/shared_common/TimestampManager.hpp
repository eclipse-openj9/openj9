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

#if !defined(TIMESTAMPMANAGER_HPP_INCLUDED)
#define TIMESTAMPMANAGER_HPP_INCLUDED

/* @ddr_namespace: default */
#include "j9.h"
#include "ClasspathItem.hpp"
#include "sharedconsts.h"
#include "ut_j9shr.h"

/*
 * Utility class which provides timestamping functions for 
 * classpaths, packages and ROMClasses.
 * (Note: This class is not an SH_Manager as it does not store data)
 */
class SH_TimestampManager
{
public:
	/*
	 * Checks the timestamp of a classpath entry for a given package.
	 * If the classpath entry is not a filesystem directory or if the package
	 * is the default package, this function timestamps the classpath entry itself.
	 * Otherwise, it timestamps the correct package directory within the
	 * classpath entry given.
	 * 
	 * If the actual timestamp is different to the timestamp stored in cpei
	 * or pItem, a new timestamp is returned and it is the responsibility
	 * of the caller to take appropriate action (mark stale).
	 * If the classpath entry does not exist, -1 is always returned.
	 * Otherwise, if there is no change in timestamp, 0 is returned.
	 * (If a new timestamp is detected, pItem is updated as packageItems do
	 *  not become stale. However, cpei is not updated
	 * 
	 * Parameters:
	 *   cpei					Classpath entry to be timestamp checked
	 */
	virtual I_64 checkCPEITimeStamp(J9VMThread* currentThread, ClasspathEntryItem* cpei) = 0;

	/*
	 * Checks the timestamp of an individual ROMClass.
	 * This function is only used for ROMClasses loaded from filesystem directories.
	 * 
	 * If the timestamp of the ROMClass has changed, a new timestamp is returned
	 * and it is the responsibility of the caller to take appropriate action (mark stale).
	 * If the romclass doesn't exist, -1 is always returned.
	 * Otherwise, if there is no change, 0 is returned.
	 * 
	 * Parameters:
	 *   className		Fully-qualified name of class to timestamp check
	 *   cpei			Classpath entry that the class was found in
	 *   rcWrapper		The ROMClass wrapper of the ROMClass to check
	 * 					(Contains the current timestamp)
	 */
	virtual I_64 checkROMClassTimeStamp(J9VMThread* currentThread, const char* className, UDATA classNameLen, ClasspathEntryItem* cpei, ROMClassWrapper* rcWrapper) = 0;
protected:
	/* - Virtual destructor has been added to avoid compile warnings. 
	 * - Delete operator added to avoid linkage with C++ runtime libs 
	 */
	void operator delete(void *) { return; }
	virtual ~SH_TimestampManager() { Trc_SHR_Assert_ShouldNeverHappen(); };
};

#endif /* !defined(TIMESTAMPMANAGER_HPP_INCLUDED) */


