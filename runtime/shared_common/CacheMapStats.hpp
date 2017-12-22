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

#if !defined(CACHEMAPSTATS_H_INCLUDED)
#define CACHEMAPSTATS_H_INCLUDED

/* @ddr_namespace: default */
#include "j9comp.h"
#include "j9.h"
#include "OSCache.hpp"
#include "ut_j9shr.h"

/**
 * Interface to shared cache subsystem for reading cache statistics
 */
class SH_CacheMapStats
{
public:
	virtual IDATA startupForStats(J9VMThread* currentThread, SH_OSCache * oscache, U_64 * runtimeflags) = 0;

	virtual IDATA shutdownForStats(J9VMThread* currentThread) = 0;

	virtual UDATA getJavacoreData(J9JavaVM *vm, J9SharedClassJavacoreDataDescriptor* descriptor) = 0;
};

#endif /*CACHEMAPSTATS_H_INCLUDED*/
