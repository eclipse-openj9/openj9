/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

package com.ibm.j9ddr.tools;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Stack;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.Image;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory;

/**
 * @author apilkington
 * 
 * Class used to provide a high level indication of the validity of a given core file.
 * It walks all iterators looking to exercise as much of the DTFJ API as possible and
 * reports back the location of any exceptions or corrupt data encountered 
 *
 */

@SuppressWarnings("restriction")
public class DTFJWalker {
	private final Logger logger = Logger.getLogger(getClass().getName());
	private final Stack<String> path = new Stack<String>();
	private final ArrayList<InvocationResult> results = new ArrayList<InvocationResult>();

	public static void main(String[] args) {
		DTFJWalker walker = new DTFJWalker();
		if (args.length != 1) {
			printHelp();
			System.exit(1);
		}
		walker.walkCoreFile(args[0]);
	}
	
	/**
	 * Print usage help to stdout
	 */
	private static void printHelp() {
		System.out.println("Usage :\n\njava com.ibm.j9ddr.tools.DTFJWalker <core file>\n");
		System.out.println("<core file>       : the path and file name of the core file");
	}
	
	public void walkCoreFile(String path) {
		File file = new File(path);
		walkCoreFile(file);
	}
	
	public void walkCoreFile(File file) {
		//the factory will have to be on the classpath as it is in the same jar as this class
		J9DDRImageFactory factory = new J9DDRImageFactory();
		Image image = null;
		try {
			image =  factory.getImage(file);
		} catch (IOException e) {
			System.err.println("Failed to create an Image from the core file : " + e.getMessage());
			logger.log(Level.WARNING, "Failed to create Image from core file", e);
			return;
		}
		try {
			Method iterator = image.getClass().getDeclaredMethod("getAddressSpaces", (Class[]) null);
			iterate(image, iterator, 100);
		} catch (Exception e) {
			System.err.println("Failed to get address space iterator method : " + e.getMessage());
			logger.log(Level.WARNING, "Failed to get address space iterator method", e);
			return;
		}
		System.out.println("Walk complete");
		showResults();
	}
	
	private void showResults() {
		for(InvocationResult result : results) {
			System.out.println(result.toString());
		}
	}
	
	private void iterate(Object object, Method iterator, int maxItems) {
		int count = 0;
		path.push(getNameFromMethod(iterator));
		iterator.setAccessible(true);
		 try {
			Object result = iterator.invoke(object, (Object[]) null);
			if(result instanceof Iterator<?>) {
				Iterator<?> list = (Iterator<?>) result;
				while(list.hasNext() && (count < maxItems)) {
					Object data = list.next();
					if(data instanceof CorruptData) {
						results.add(new InvocationResult(getCurrentPath(), (CorruptData) data));
					} else {
						Method[] methods = data.getClass().getMethods();
						for(Method method : methods) {
							if(method.getReturnType().getSimpleName().equals("Iterator")) {
								path.push("[" + count + "]/");
								iterate(data, method, maxItems);
								path.pop();
							}
						}
					}
					count++;
				}
			}
			if(result instanceof CorruptData) {
				results.add(new InvocationResult(getCurrentPath(), (CorruptData) result));
			}
		} catch (Exception e) {
			System.err.println("Exception thrown by iterator : " + e.getMessage());
			logger.log(Level.WARNING, "Exception thrown by iterator", e);
			results.add(new InvocationResult(getCurrentPath(), e.getCause()));
			//fall through and exit the invocation of this iterator
		}
		path.pop();
	}
	
	private String getCurrentPath() {
		Iterator<String> elements = path.iterator();
		StringBuilder builder = new StringBuilder();
		while(elements.hasNext()) {
			builder.append(elements.next());
		}
		return builder.toString();
	}
	
	private String getNameFromMethod(Method method) {
		String name = method.getName();
		for(int i = 0; i < name.length(); i++) {
			if(name.charAt(i) < 'a') {
				return name.substring(i);
			}
		}
		return name;
	}
	
	private class InvocationResult {
		private final String path;
		private final CorruptData corruptData;
		private final Throwable exception;
		
		InvocationResult(String path, CorruptData corruptData, Throwable exception) {
			this.path = path;
			this.corruptData = corruptData;
			this.exception = exception;
		}
		
		InvocationResult(String path,CorruptData corruptData) {
			this.path = path;
			this.corruptData = corruptData;
			this.exception = null;
		}
		
		InvocationResult(String path,Throwable exception) {
			this.path = path;
			this.corruptData = null;
			this.exception = exception;
		}

		@Override
		public String toString() {
			StringBuilder builder = new StringBuilder(path);
			builder.append(" = ");
			if(null != corruptData) {
				builder.append(corruptData.toString());
			}
			if(null != exception) {
				builder.append(exception.getClass().getName());
				builder.append("[");
				builder.append(exception.getMessage());
				builder.append("] ");
			}
			return builder.toString();
		}
		
		
	}
}
