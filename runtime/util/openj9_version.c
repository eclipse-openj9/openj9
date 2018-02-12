/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

/*The file prints out required version information for Xinternalversion option */
#include "openj9_version.h"
#if defined(OPENJ9_BUILD)
/*The file collects version information in OpenJ9 build */
#include "openj9_version_info.h"
#endif /* OPENJ9_BUILD */

void
j9print_internal_version(J9PortLibrary *portLib) {
	PORT_ACCESS_FROM_PORT(portLib);

#if defined(OPENJ9_BUILD)
#if defined(J9JDK_EXT_VERSION) && defined(J9JDK_EXT_NAME)
	j9tty_err_printf(PORTLIB, "Eclipse OpenJ9 %s %s-bit Server VM (%s) from %s-%s JRE with %s %s, built on %s %s by %s with %s\n",
		J9PRODUCT_NAME, J9TARGET_CPU_BITS, J9VERSION_STRING, J9TARGET_OS, J9TARGET_CPU_OSARCH,
		J9JDK_EXT_NAME, J9JDK_EXT_VERSION,__DATE__, __TIME__, J9USERNAME, J9COMPILER_VERSION_STRING);
#else
        j9tty_err_printf(PORTLIB, "Eclipse OpenJ9 %s %s-bit Server VM (%s) from %s-%s JRE, built on %s %s by %s with %s\n",
                J9PRODUCT_NAME, J9TARGET_CPU_BITS, J9VERSION_STRING, J9TARGET_OS, J9TARGET_CPU_OSARCH,
                __DATE__, __TIME__, J9USERNAME, J9COMPILER_VERSION_STRING);
#endif /* J9JDK_EXT_VERSION && J9JDK_EXT_NAME */
#else /* OPENJ9_BUILD */
	j9tty_err_printf(PORTLIB, "internal version not supported\n");
#endif /* OPENJ9_BUILD */
}
