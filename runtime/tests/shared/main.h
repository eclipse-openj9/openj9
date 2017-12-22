/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#ifndef main_h
#define main_h

#define PASS 0
#define FAIL 1

#include "ProcessHelper.h"

#define TRANSACTIONTEST_CMDLINE_STARTSWITH "-testSCStoreTransaction"
#define TRANSACTION_WITHBCI_TEST_CMDLINE_STARTSWITH "-testSCStoreWithBCI"
#define OSCACHETEST_CMDLINE_STARTSWITH "-sharedtestchild"

#define JAVAHOMEDIR "-Djava.home="
#define JAVAHOMEDIR_LEN strlen(JAVAHOMEDIR)
UDATA buildChildCmdlineOption(int argc, char **argv, const char *options, char * newargv[SHRTEST_MAX_CMD_OPTS]);

#define REPORT_START(name) j9tty_printf(PORTLIB, "\n%s test begin...\n\n", name)
#define SHC_TEST_ASSERT(name, test, success, rc) if ((success |= (rc = test)) | 1) j9tty_printf(PORTLIB, "%s test %s. RC=%d\n\n", name, (rc ? "FAILED" : "PASSED"), rc)
#define REPORT_SUMMARY(name, success) j9tty_printf(PORTLIB, "\nResult of test %s: %s\n\n", name, (success ? "FAILED" : "PASSED"))

#endif /* main_h */
