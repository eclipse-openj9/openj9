/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package j9vm.test.terminate;
import java.io.*;

public class TerminateTestRunner extends j9vm.runner.Runner {

/* WARNING: SystemProcess.destroy() currently does not close the process pipes
	before calling destroyImpl() (because of Win98 problems).  If that were
	changed, this test might become bogus---i.e. even if the process is still
	alive in the background, it will look dead to TerminateTestRunner.
	
	The solution would be to lock some kind of resource (such as a temporary
	file or socket or something?) and verify that the resource gets	released.
	
	Might not work on non-robust platforms i.e. Neutrino???   TODO.  */

public TerminateTestRunner(String className, String exeName, String bootClassPath, String userClassPath, String javaVersion) {
	super(className, exeName, bootClassPath, userClassPath, javaVersion);
}

public int runCommandLine(String commandLine)  {
	System.out.println("command: " + commandLine);
	System.out.println();
	Process process;
	try  {
		process = Runtime.getRuntime().exec(commandLine);
	} catch (Throwable e)  {
		System.out.println("Exception starting process!");
		System.out.println("(" + e.getMessage() + ")");
		e.printStackTrace();
		return 99999;
	}

	/* Wait up to 60 seconds, checking for output. */
	long startTime = System.currentTimeMillis();
	boolean ready = false;
	boolean failed = false;

	BufferedInputStream is = new BufferedInputStream(process.getInputStream());
	
	for (int i = 0; (i<60) && !ready; i++)  {
		try  {
			Thread.sleep(1000);
		} catch (InterruptedException e)  {
			System.out.println("parent sleep() 1," + i + " interrupted");	
		}
		try  {
			if (is.available() > 0)  ready = true;
		} catch (IOException e)  {
			System.out.println("parent is.available() 1 threw exception");
			e.printStackTrace();
		}
	}
	try  {
		Thread.sleep(1000);  /* jic: let parent finish first blob of output? */
	} catch (InterruptedException e)  {
		System.out.println("parent sleep() 2 interrupted");	
	}

	byte[] buf = new byte[100];
	int length = 0;
	try  {
		length = is.read(buf);
		while (length >= 0)  {
			System.out.write(buf, 0, length);
			/* TODO: Sleep() here to reduce contention? */
			length = is.read(buf);

			if (ready)  {
				/* We've supposedly read output, try to kill process now */
				ready = false;
				process.destroy();
			} else if (!failed && (length != 0)) {
				try  {
					Thread.sleep(500);
				} catch (InterruptedException e)  {
					System.out.println("parent sleep() 3 interrupted");	
				}
				try  {
					if (is.available() > 0)  {
						System.out.println("");
						System.out.println("Bad: new output still arriving after process.destroy()!");
						failed = true;
					}
				} catch (IOException e)  {
					System.out.println("parent is.available() 2 threw exception");
					e.printStackTrace();
				}
			}
		}
		
	} catch (Throwable e)  {
		/* TODO: Silently ignore IOExceptions? */
		System.out.println("Exception reading process output!");
		e.printStackTrace();
	}
	System.gc();
	if (failed)  return 5;
	System.out.println("ok");
	return 0;
}

}
