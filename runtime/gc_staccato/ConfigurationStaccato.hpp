
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * @ingroup GC_Modron_Metronome
 */

#if !defined(CONFIGURATIONSTACCATO_HPP_)
#define CONFIGURATIONSTACCATO_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "ConfigurationRealtime.hpp"

class MM_EnvironmentBase;
class MM_GlobalCollector;
class MM_Heap;

class MM_ConfigurationStaccato : public MM_ConfigurationRealtime
{
/* Data members / Types */
public:
protected:
private:

/* Methods */
public:
	static MM_Configuration *newInstance(MM_EnvironmentBase *env);
	
	virtual MM_GlobalCollector *createGlobalCollector(MM_EnvironmentBase *env);
	
	MM_ConfigurationStaccato(MM_EnvironmentBase *env)
		: MM_ConfigurationRealtime(env)
	{
		_typeId = __FUNCTION__;
	};
	
protected:
	
private:
};


#endif /* CONFIGURATIONSTACCATO_HPP_ */
