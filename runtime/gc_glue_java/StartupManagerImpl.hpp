/*******************************************************************************
 * Copyright (c) 2014, 2017 IBM Corp. and others
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

#if !defined(MM_STARTUPMANAGERIMPL_HPP_)
#define MM_STARTUPMANAGERIMPL_HPP_

#include "StartupManager.hpp"

class MM_CollectorLanguageInterface;
class MM_VerboseManagerBase;

struct OMR_VM;

class MM_StartupManagerImpl : public MM_StartupManager
{
	/*
	 * Data members
	 */
private:

protected:
public:
	static const uintptr_t defaultMinimumHeapSize = (uintptr_t) 8*1024*1024;
	static const uintptr_t defaultMaximumHeapSize = (uintptr_t) 1*1024*1024*1024;

	/*
	 * Function members
	 */

private:

protected:
	virtual bool handleOption(MM_GCExtensionsBase *extensions, char *option);
	virtual char * getOptions(void);

public:
	virtual MM_Configuration *createConfiguration(MM_EnvironmentBase *env);
	virtual MM_CollectorLanguageInterface * createCollectorLanguageInterface(MM_EnvironmentBase *env);
	virtual MM_VerboseManagerBase * createVerboseManager(MM_EnvironmentBase* env);

	MM_StartupManagerImpl(OMR_VM *omrVM)
		: MM_StartupManager(omrVM, defaultMinimumHeapSize, defaultMaximumHeapSize)
	{
	}
};

#endif /* MM_STARTUPMANAGERIMPL_HPP_ */
