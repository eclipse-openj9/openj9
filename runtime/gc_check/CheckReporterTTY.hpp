
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
 * @ingroup GC_Check
 */

#if !defined(CHECKREPORTERTTY_HPP_)
#define CHECKREPORTERTTY_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"

#include "CheckReporter.hpp"

class GC_CheckError;

/**
 * Output reports to a terminal.
 * @ingroup GC_Check
 */
class GC_CheckReporterTTY : public GC_CheckReporter
{
private:

public:
	static GC_CheckReporterTTY *newInstance(J9JavaVM *javaVM);
	virtual void kill();
	virtual void report(GC_CheckError *error);
	virtual void reportObjectHeader(GC_CheckError *error, J9Object *objectPtr, const char *prefix);
	virtual void reportClass(GC_CheckError *error, J9Class *clazz, const char *prefix);
	virtual void reportFatalError(GC_CheckError *error);
	virtual void reportHeapWalkError(GC_CheckError *error, GC_CheckElement previousObjectPtr1, GC_CheckElement previousObjectPtr2, GC_CheckElement previousObjectPtr3);

	/**
	 * Create a new CheckReporterTTY object
	 */
	GC_CheckReporterTTY(J9JavaVM *javaVM) :
		GC_CheckReporter(javaVM)
	{}
};

#endif /* CHECKREPORTERTTY_HPP_ */
