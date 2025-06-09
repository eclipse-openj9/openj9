/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(JFRUTILS_HPP_)
#define JFRUTILS_HPP_

#include "j9.h"
#include "vm_api.h"

#if defined(J9VM_OPT_JFR)

enum JfrBuildResult {
	OK,
	OutOfMemory,
	InternalVMError,
	FileIOError,
	EnvVarsNotSet,
	TimeFailure,
	MetaDataFileNotLoaded,
};

class VM_JFRUtils {
	/*
	 * Data members
	 */
private:

protected:

public:

	/*
	 * Function members
	 */
private:

protected:

public:

	static U_64 getCurrentTimeNanos(J9PortLibrary *privatePortLibrary, JfrBuildResult &buildResult)
	{
		UDATA success = 0;
		U_64 result = (U_64) j9time_current_time_nanos(&success);

		if (success == 0) {
			buildResult = TimeFailure;

		}

		return result;
	}

};

#endif /* defined(J9VM_OPT_JFR) */

#endif /* JFRCHUNKWRITER_HPP_ */
