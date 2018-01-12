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
/**
 * Source Control Header
 * 
 * $Author: fengj $ 
 * $Date: 2012/11/23 21:12:02 $
 * $Revision: 1.4 $
 * $Name:  $
 * $Log: GPTest.java,v $
 * Revision 1.4  2012/11/23 21:12:02  fengj
 * JAZZ 60879 : : fix copyright notices in HEAD for Java 8 beta 2012
 *
 * Revision 1.3  2012/05/24 19:39:34  grollest
 * JAZZ 54880 : GAC + Francis Bogsanyi : Percolate conditions corresponding to crashes (instead of throwing abend or java exception)
 * 	- commit gptests to head, JVM_26, Java626_SR3 and Java726_SR2
 *
 * Revision 1.2  2012/05/10 20:13:49  grollest
 * JAZZ 56233 : GAC + Francis Bogsanyi : implement in HEAD -31/64-bit z/OS - The Java signal handler should return from hardware signals and throw an abend for software signals
 * 	- port of Java 6 code to head
 * 	- GPTest.java only
 *
 * Revision 1.1  2012/03/21 14:33:07  lanxia
 * Jazz 54452:Jason Feng & Ronald Servant: move cmdline_options_testresources from javasvt to ottcvs1
 *
 * Revision 1.5  2011-01-15 16:13:29  grollest
 * JAZZ 41162 : : implement the design (testing only): z/OS Batch Runtime - convert LE Condition into Java Exception
 *
 * 	- cmdlinetester harness and resources support for new -XCEEHDLR tests
 *
 * Revision 1.4  2010/11/23 14:09:22  grollest
 * JAZZ 40190 : Francis Bogsanyi + Maciek Klimkowski : ensure LE condition handler is registered on each callin
 *
 * Revision 1.2  2010/11/19 15:34:44  grollest
 * JAZZ 40190 : Francis Bogsanyi : ensure LE condition handler is registered on each callin
 *
 * Revision 1.1  2007/11/06 16:51:00  vsebe
 * Created for new build system implementation (Hudson). Project contains old cmdline_options_tester/testresources module.
 *
 * Revision 1.1  2006/02/16 00:44:03  rajeev_rattehall
 * Initial check in
 *
 * 
 */

package VMBench.GPTests;

import VMBench.GPTests.GPTest;

public class GPTest implements Runnable {
	static { System.loadLibrary("gptest"); }
	String arg;
	
	public static int globalInt; 
/**
 * 
 * @param arg java.lang.String
 */
public GPTest(String arg) {
	this.arg = arg;
}

public native void gpFloat();
public native void gpIll();
public native void gpRead();
public native void gpWrite();
public native void gpSoftwareFloat();
public native void gpHardwareFloat();
public native void gpSoftwareRead();
public native void gpHardwareRead();
public native void gpAbort();

public native void callBackIn();
public native void callInAndTriggerGPReadThenResumeAndCallInAgain();
public native void callInAndTriggerGPReadThenResumeAndReturnToJava(int count);

/**
 * Starts the application.
 * @param args an array of command-line arguments
 */
public static void main(java.lang.String[] args) {
	boolean thread = false;
	
	if (args.length != 1) {
		if (args.length == 2 && args[0].equalsIgnoreCase("thread")) {
			thread = true;
		} else {
			System.err.println("Incorrect args.");
			System.err.println("Args: [thread] [float|ill|read|write|callin]");
			System.err.println("If 'thread' is set the GP test runs in a child thread,");
			System.err.println("otherwise it runs in the main thread.");
			System.exit(1);
		}
	}

	GPTest test = new GPTest(args[args.length - 1]);
	if (thread) {
		new Thread(test).start();
	} else {
		test.run();
	}
}
public void run() {
	
	if (arg.equalsIgnoreCase("softwareFloat")) {
		this.gpSoftwareFloat();
		System.err.println("Survived software-triggered SIGFPE!");
		System.exit(3);
	}

	if (arg.equalsIgnoreCase("hardwareFloat")) {
		this.gpHardwareFloat();
		System.err.println("Survived hardware-triggered SIGFPE!");
		System.exit(3);
	}

	if (arg.equalsIgnoreCase("softwareRead")) {
		this.gpSoftwareRead();
		System.err.println("Survived software-triggered SIGSEGV!");
		System.exit(3);
	}

	if (arg.equalsIgnoreCase("hardwareRead")) {
		this.gpHardwareRead();
		System.err.println("Survived hardware-triggered SIGSEGV!");
		System.exit(3);
	}
	
	if (arg.equalsIgnoreCase("abort")) {
		this.gpAbort();
		System.err.println("Survived abort!");
		System.exit(3);
	}

	if (arg.equalsIgnoreCase("float")) {
		
		/* Call this multiple times for-XCEEHDLR to exercise running Java code following 
		 *  the conversion from LE Condition to Java ConditionException 
		 * 
		 * Calling it multiple times will not affect the non -XCEEHDLR case, as a crash is expected the first time 'round */
		for (int i = 0; i < 50 ; i++) {
			try {
				this.gpFloat();
			} catch (com.ibm.le.conditionhandling.ConditionException ce) {
				byte []feedbackToken;
				java.util.Locale locale = null;
				
				System.err.println("");
				System.err.println("");
				System.err.println(ce.toString());
				System.err.println("\t" + ce.getRoutine() + ": " + ce.getOffsetInRoutine());
				System.err.println("\tfacilityID: " + ce.getFacilityID());
				System.err.println("\tseverity: " + ce.getSeverity());
				System.err.println("\tmessageNumber: " + ce.getMessageNumber());
				feedbackToken = ce.getToken();
				System.err.print("\t feedback token: ");
				for (int j = 0 ; j< feedbackToken.length ; j++) {
					System.err.printf(locale, "%#x ", feedbackToken[j]);
				}
				System.err.println("\nSuccesfully threw exception " + (i + 1) + " times");
				ce.printStackTrace();
			}
		}
		System.err.println("Survived float gp!");
		System.exit(2);
	}

	if (arg.equalsIgnoreCase("ill")) {
		this.gpIll();
		System.err.println("Survived illegal instruction gp!");
		System.exit(3);
	}

	if (arg.equalsIgnoreCase("read")) {
		this.gpRead();
		System.err.println("Survived read gp!");
		System.exit(4);
	}

	if (arg.equalsIgnoreCase("write")) {
		this.gpWrite();
		System.err.println("Survived write gp!");
		System.exit(5);
	}
	
	if (arg.equalsIgnoreCase("callin")) {
		this.callBackIn();
	}

	if (arg.equalsIgnoreCase("callIn")) {
		this.callBackIn();
	}
	
	if (arg.equalsIgnoreCase("callInAndTriggerGPReadThenResumeAndCallInAgain")) {
		this.callInAndTriggerGPReadThenResumeAndCallInAgain();
	}
	
	if (arg.equalsIgnoreCase("callInAndTriggerGPReadThenResumeAndReturnToJava")) {
		this.callInAndTriggerGPReadThenResumeAndReturnToJava(10);
	}

	if (arg.equalsIgnoreCase("callInAndTriggerGPReadThenResumeAndReturnToJIT")) {
		this.callInAndTriggerGPReadThenResumeAndReturnToJava_wrapper_wrapper();
	}

	System.err.println("Unrecognized argument: " + arg);
	System.exit(1);

}

public void callOut() {
	this.gpRead();
	System.err.println("Survived read gp!");
	System.exit(4);
}

public void callInAndTriggerGPReadThenResumeAndReturnToJava_wrapper_wrapper() {
	
	for (int i = 0 ; i < 100 ; i++) {
		this.callInAndTriggerGPReadThenResumeAndReturnToJava_wrapper(i);
	}
}

public void callInAndTriggerGPReadThenResumeAndReturnToJava_wrapper(int count) {

	for (int i =0; i <100 ; i ++) {
		globalInt = count;
	}
	
	this.callInAndTriggerGPReadThenResumeAndReturnToJava(globalInt);
	
}

public void callOutAndTriggerGPRead() {
	this.gpRead();
	System.err.println("Survived read gp!");
	System.exit(4);
}


}
