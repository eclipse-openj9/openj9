/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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
import java.net.URL;

import CustomClassloaders.CustomTokenClassLoader;
import CustomClassloaders.CustomURLClassLoader;
import CustomClassloaders.CustomURLLoader;
import Utilities.StringManipulator;
import Utilities.URLClassPathCreator;

/**
 * @author Matthew Kilner
 */
public class TokenIncompatibilityTest {
	
	StringManipulator manipulator = new StringManipulator();

	public static void main(String[] args) {
		
		TokenIncompatibilityTest test = new TokenIncompatibilityTest();
		
		test.run();
	}
	
	public void run(){
		boolean passed = true;
		
		URLClassPathCreator pathCreator = new URLClassPathCreator("./Pets;./Sports;");
		
		CustomTokenClassLoader loader = new CustomTokenClassLoader(pathCreator.createURLClassPath());
		
		String[] classesToLoad = new String[]{"Dog","SpeedSkating"};
		
		String tok = "FindStore";
		loader.setToken(tok);
		for(int index = 0; index < classesToLoad.length; index++){
			try{
				loader.loadClass(classesToLoad[index]);
			} catch(ClassNotFoundException e){
				e.printStackTrace();
			}
		}
		for(int index = 0; index < classesToLoad.length; index++){
			if(true != loader.isClassInSharedCache(tok, classesToLoad[index])){
				System.out.println("\nClass: "+classesToLoad[index]+" not in cache");
				passed = false;
			}
		}		
		
		pathCreator = new URLClassPathCreator("./Pets;");
		CustomURLClassLoader urlCl = new CustomURLClassLoader(pathCreator.createURLClassPath());
		if(true == urlCl.isClassInSharedCache("Dog")){
			passed = false;
			System.out.println("\nURLClassLoader succesfully loaded class stored with token.");
		}
		
		pathCreator = new URLClassPathCreator("./Sports;");
		CustomURLLoader urlL = new CustomURLLoader(pathCreator.createURLClassPath());
		if(true == urlL.isClassInSharedCache(0,"Dog")){
			passed = false;
			System.out.println("\nURLLoader succesfully loaded class stored with token.");
		}
		
		//Load with URLCP
		try{
			urlCl.loadClass("Cat");
		} catch(ClassNotFoundException e){
			e.printStackTrace();
		}
		
		//Load with URLL
		urlL.loadClassFrom("Cricket",0);
		
		//Find with Token
		if(true == loader.isClassInSharedCache(tok, "Cat")){
			passed = false;
			System.out.println("\nTokenLoader found class loaded by URLClassLoader");
		}
		
		if(true == loader.isClassInSharedCache(tok, "Cricket")){
			passed = false;
			System.out.println("\nTokenLoader found class loaded by URLLoader");
		}
		
		//Find with token == url
		pathCreator = new URLClassPathCreator("./Pets;");
		URL[] urlarray = pathCreator.createURLClassPath();
		String urlToken = urlarray[0].toString();
		if(true == loader.isClassInSharedCache(urlToken, "Cricket")){
			passed = false;
			System.out.println("\nTokenLoader found class loaded by URLLoader using a token that looked like a url");
		}
				
		if(passed){
			System.out.println("\nTEST PASSED");
		} else {
			System.out.println("\nTEST FAILED");
		}
	}
}
