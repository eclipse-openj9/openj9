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
package com.ibm.j9.offload.tests;

import java.io.*;
import java.util.Vector;


public class FileLister {
	
	static final String NATIVE_LIBRARY_NAME = "j9offjnitest26";
	static boolean libraryLoaded = false;
	
	private synchronized static native byte[][] listImpl(byte[] path);
	private native boolean isDirectoryImpl(byte[] path);
	
	private BufferedReader buff = new BufferedReader(new InputStreamReader( System.in ));
	private static FileLister lister = new FileLister();
	
	public FileLister(){
		ensureLibraryLoaded();
	};
	
	/**
	 * make sure the native library required to run the natives is loaded
	 */
	static public void ensureLibraryLoaded(){
		if (!libraryLoaded){
			System.loadLibrary(NATIVE_LIBRARY_NAME);
			libraryLoaded = true;
		}
	}
	
	public boolean isDir(String thePath) {
		if (thePath == null){
			return false;
		}
		byte[] path = com.ibm.oti.util.Util.getBytes(thePath);
		if (!isDirectoryImpl(path)){ 
			return false;
		}
		return true;
	}
	
	public void listFiles(String thePath) {
		byte[] path = com.ibm.oti.util.Util.getBytes(thePath);
			byte[][] implList = listImpl(path);
			if (implList != null){
				for (int index = 0; index < implList.length; index++) {
					String aName = com.ibm.oti.util.Util.toString(implList[index]);
					System.out.println(aName);
				}
			}
	}

	public String getUserInput(){
		String text = "aaa";
	    	try {
				text = buff.readLine();
			} catch (IOException e) { 
			}
		return text;
	}
	
	public String checkPath(String thePath) {
		
		// just return null if the path is null
		if (thePath == null){
			return null;
		}
		
		// first check for cases were we are at the root
		if (thePath.equals("/") || thePath.equals("\\")){
			if (!isDir(thePath)){
				return null;
			} else {
				return thePath;
			}
		}
		
		String[] pathDir = thePath.split("[/\\\\]+");
		String newPath = new String();
		Vector path = new Vector();
		
		for(int m = 0; m < pathDir.length; m++) {
			path.add(pathDir[m]);

			if(path.lastElement().equals("..")) {
			   	path.remove(path.lastElement());  
			   	path.remove(path.lastElement()); 
			 }
		}

/*		if(lister.isDir(path.get(0)+"\\\\")){
			
			
		}else if(lister.isDir(path.get(0)+"/")){
			for(int n = 0 ; n<path.size() ; n++)
				newPath +=  path.get(n)+ "/";
		}*/
		
		// try each of the slash types
		for(int n = 0 ; n<path.size() ; n++){
			newPath +=  path.get(n)+ "/";
		}
		if (!isDir(newPath)){
			// try linux slashes
			newPath = new String();
			for(int n = 0 ; n<path.size() ; n++){
				newPath +=  path.get(n)+ "\\";
			}
		}
		if (!isDir(newPath)){
			// invalid directory 
			return null;
		}

		return newPath;
	}

	public static void main(String[] args){
		boolean exit = false;
		String rootDir = null;
		String input = null;
		String thePath = "";
		String command = "";
		
		while (!exit){	
			do{
				System.out.print(rootDir + ">");
				input=lister.getUserInput();
				if ((input != null)&&(input.length() >=2)){
					command= input.substring(0, 2).toLowerCase();
					String requestedPath= input.substring((input.lastIndexOf(" ")+1), input.length());
					if (requestedPath != null){
						requestedPath = requestedPath.trim();
					}

					// validate the command specified
					if (!command.equals("ls") && !command.equals("cd") && !input.equals("exit") ){
						System.out.print("Invalid Command \n");
						continue;
					}
					
					/* check if we have a naked cd */
					if (input.trim().equals("cd")){
						System.out.print("Must specify a directory to change to \n");
						continue;
					}
					
					/* now check if we have asked to exit */
					if (input.equals("exit")){
						System.exit(0);
					}
					
					/* if we have a naked ls then just go ahead and do it */
					if (input.trim().equals("ls")){
						if ((rootDir != null)&&(!rootDir.equals(""))){
							lister.listFiles(rootDir);
						} else {
							System.out.println("Must specify a root directory before doing a naked ls");
						}
						continue;
					}
					
					/* it is either an ls or cd command, in these cases we first create the path
					 * which has been requested */
					char firstChar = '0';
					char secondChar = '0';
					if (requestedPath.length() >=1){
						firstChar = requestedPath.charAt(0);
					}
					if (requestedPath.length() >= 2){
						secondChar = requestedPath.charAt(1);
					}
					if ((firstChar == '/') || (secondChar == ':')){
						thePath = requestedPath;
					} else {
						// relative path use rootDir and requested path
						thePath = rootDir + requestedPath;
					}
			
					/* validate that the path specified to the cd or ls command is a directory */
					String specifiedPath = new String(thePath);
					thePath = lister.checkPath(thePath);
					if (!lister.isDir(thePath)){
							System.out.print(specifiedPath + " is not a directory \n");
							continue;
					}
					
					if(command.equals("ls")){
						lister.listFiles(thePath);
						continue;
					}
					
					if(command.equals("cd")){
						if (thePath != null){
							rootDir = lister.checkPath(thePath);
						}
					}

				}
			} while (true);
		}
	}
}
