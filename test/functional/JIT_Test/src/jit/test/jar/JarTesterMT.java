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
package jit.test.jar;

/**
 * @author mesbaha
 * JarTesterMT extends {@link jit.test.jar.ZipTester} and initializes jit.test.jar.ZipTester.loader property to a new instance of {@link jit.test.jar.ZipTestClassLoaderMT}.
 * JarTesterMT constructor takes in a boolean value indicating whether or not we need multiple class loader instances. It initializes the surrogateLifespan property of 
 * jit.test.jar.Ziptester.loader according to this flag. It is used in JIT code cache tests for loading Jar files.
 */
public class JarTesterMT extends ZipTester {
	
	/** Constructor for creating a JarTester with multiple class loader and multi-threaded class loading support 
	 * @param requireMultipleClassLoader - Flag indicating whether or not multiple class loader instance is to be used for class loading
	 * @param multiThreadedCompilationRequired - Flag indicating whether or not multi-threaded class loading is required
	 */
	public JarTesterMT( boolean requireMultipleClassLoader, boolean multiThreadedCompilationRequired ) {
		super();
		int lifeSpan = 0 ; 
		
		if ( requireMultipleClassLoader ) {
			lifeSpan = 200;
		}
		else {
			lifeSpan = 10000000;
		}
		
		loader = new ZipTestClassLoaderMT( lifeSpan, false, multiThreadedCompilationRequired );
	}
	
	public String getClassName() {
		return "jit.test.jar.JarTesterMT";
	}
	
	/**
	 * This method is used externally to issue a request to pause all the compilation threads 
	 */
	public void pauseCompilation() {
		((ZipTestClassLoaderMT)loader).pauseCompilation();
	}
	
	/**
	 * This method is used externally to issue a request to resume all paused compilation threads 
	 */
	public void resumeCompilation() {
		((ZipTestClassLoaderMT)loader).resumeCompilation();
	}
	
	/**
	 * This method is used externally to issue a request to stop all compilation threads
	 */
	public void stopComopilation() {
		((ZipTestClassLoaderMT)loader).stopCompilation();
	}
	
	/**
	 * @return true if any compilation thread is still alive
	 */
	public boolean isStillCompiling() {
		return ((ZipTestClassLoaderMT)loader).isStillCompiling();
	}
}
