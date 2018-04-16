/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.tests.garbagecollector;

/**
 * @author jmdisher
 * This test will spin on allocating an object for the specified number of seconds before terminating.  It was created for CMVC 129637
 * to test our ability to throw OOM on excessive GC.
 */
public class SpinAllocate
{
	public static Object _objectHolder;

	/**
	 * @param args Takes one argument:  number of seconds to spin for before terminating with a message that the test ran to completion.
	 * This argument is required.  It must be in the range [1-60]
	 */
	public static void main(String[] args)
	{
		if (1 == args.length)
		{
			int secondsToSpin = Integer.parseInt(args[0]);

			if ((secondsToSpin >= 1) && (secondsToSpin <= 60))
			{
				long finishTime = System.currentTimeMillis() + (secondsToSpin * 1000);
				while (System.currentTimeMillis() < finishTime)
				{
					_objectHolder = new Object();
				}
				System.out.println("Test ran to completion");
			}
			else
			{
				System.err.println("Invalid option given for seconds (" + secondsToSpin + ").  Value given must be in the range [1-60].");
				System.exit(2);
			}
		}
		else
		{
			System.err.println("Missing argument for test run time.  Please specify the number of seconds desired for the test run (in the range [1-60]).");
			System.exit(1);
		}
	}
}
