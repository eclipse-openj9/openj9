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

#ifndef VERBOSEHANDLERJAVA_HPP_
#define VERBOSEHANDLERJAVA_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"
#include "Base.hpp"

class MM_VerboseManager;
class MM_EnvironmentBase;

class MM_VerboseHandlerJava : public MM_Base
{
public:
	/**
	 * Output finalizable list summary.
	 * @param manager
	 * @param env GC thread used for output.
	 * @param indent base level of indentation for the summary.
	 */
	static void outputFinalizableInfo(MM_VerboseManager *manager, MM_EnvironmentBase *env, UDATA indent);

	/**
	 * Output the name of the thread into the buffer.
	 * @return Whether the thread name was truncated.
	 */
	static bool getThreadName(char *buf, UDATA bufLen, OMR_VMThread *omrThread);

	/**
	 * Output Java VM arguments.
	 */
	static void writeVmArgs(MM_VerboseManager *manager, MM_EnvironmentBase* env, J9JavaVM *vm);
};

#endif /* VERBOSEHANDLERJAVA_HPP_ */
