/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
package com.ibm.jvmti.tests.getThreadGroupChildren;

import com.ibm.jvmti.tests.util.AgentHardException;
import com.ibm.jvmti.tests.util.SyncThread;

public class gtgc002
{
	final int THREAD_PER_GROUP = 2;
	native static boolean checkGroupName();

	public boolean testThreadGroupNameCheck() throws Exception
	{
		String[] name = {
		"\u30EA\u30F3\u30AF\u30FB\u30A4\u30F3\u30C7\u30AF\u30B5\u30FC", // japanese str from PMR 61315,999,760
		/* various language for "I need more plutonium" */
		"\u6211\u9700\u8981\u66F4\u591A\u7684\u949A", // chinese
		"\u79C1\u306F\u3088\u308A\u591A\u304F\u306E\u30D7\u30EB\u30C8\u30CB\u30A6\u30E0\u304C\u5FC5\u8981", // japanese
		"\uC880\u0020\uB354\u0020\uD50C\uB8E8\uD1A0\uB284\uC774\u0020\uD544\uC694\uD569\uB2C8\uB2E4" // korean
		};

		ThreadGroup[] tgList = new ThreadGroup[name.length];

		for (int i = 0; i < tgList.length; i++) {
			tgList[i] = new ThreadGroup(name[i]);
		}

		 SyncThread threadList[] = new SyncThread[tgList.length * THREAD_PER_GROUP];
		 for (int i = 0; i < tgList.length; i++) {
			 for (int j = 0; j < THREAD_PER_GROUP; j++) {
				 threadList[(i * THREAD_PER_GROUP) + j] = new SyncThread(tgList[i], name[i]);
			 }
		 }

		for (int i = 0; i < threadList.length; i++) {
			threadList[i].start();
		}

		checkGroupName();

		for (int i = 0; i < threadList.length; i++) {
			threadList[i].interrupt();
		}

		for (int i = 0; i < threadList.length; i++) {
			try {
				threadList[i].join();
			} catch (InterruptedException ie) {
				throw new AgentHardException("Interrupted when joining "
						+ threadList[i], ie);
			}
		}

		return true;
	}

	public String helpThreadGroupNameCheck()
	{
		return "Test if getThreadChildrenInfo is able to cope with multi-bytes UTF8 character.";
	}

}
