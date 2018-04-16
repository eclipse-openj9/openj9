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
package j9vm.test.jni;

/**
 * Command line test to verify the -XX:+LazySymbolResolution and -XX:-LazySymbolResolution vmargs
 * options. This test loads a shared library which contains unresolved
 * references. This will fail unless lazy loading is enabled.
 * 
 * Decision table:
 * 
 * <pre>
 * vmarg 						vm version 			platform 		result					notes
 * ----------------------------------------------------------------------------------------------------- 
 * n/a 							realtime 			linux 			Result: unresolved		defaults to immediate link 
 * n/a 							any but realtime 	linux 			Result: loaded			defaults to lazy link
 * -XX:+LazySymbolResolution 	any 				linux 			Result: loaded 			force lazy link, has capability
 * -XX:-LazySymbolResolution 	any 				linux 			Result: unresolved		force immediate link
 *														same result for all non-linux platforms
 * n/a 							any		 			any but linux 	Result: unresolved		no lazy link capability
 * -XX:+LazySymbolResolution 	any 				any but linux	Result: unresolved 		force lazy link, has NO capability
 * -XX:-LazySymbolResolution 	any 				any but linux	Result: unresolved		force immediate link
 * </pre>
 */
public class LazyLinkTest {

	/**
	 * main entry into test.
	 * 
	 * @param args
	 *            no args used, batch mode test code.
	 */
	public static void main(String[] args) {

		try {
			// Verify that we're running the correct code by visual
			// verification.
			System.out.println("Setup complete");

			System.loadLibrary("j9unresolved");

			System.out.println("Result: loaded");
		} catch (UnsatisfiedLinkError e) {
			System.out.println(e.getMessage());
			System.out.println("Result: unresolved");
		}
	}
}
