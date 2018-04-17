package j9vm.test.benchmark.zipfile;

/*******************************************************************************
 * Copyright (c) 2010, 2012 IBM Corp. and others
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

import java.io.File;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.jar.*;


public class ClassloadingBenchmark {
	
	protected class ZipRecord {
		protected HashMap zipRecords = new HashMap(); 

		protected ZipRecord(ZipFile f) {
			Enumeration zipFileEnum = f.entries();
			ZipEntry zipEntry = null;
			String zipEntryName = null;
			String className = null;

			while (zipFileEnum.hasMoreElements()) {
				zipEntry = (ZipEntry) zipFileEnum.nextElement();
				zipEntryName = zipEntry.getName();

				// if the entry is not a class, we don't load it
				if (!zipEntryName.endsWith(".class")){
					continue;
				}
			
				className =	zipEntryName.substring(0, zipEntryName.length() - 6);
				className = className.replace('/', '.');
				if (className.startsWith("j9vm.test.benchmark.zipfile.testclasses")){
					zipRecords.put(className, this);
				}
			}
		}
		
		public int getCount(){
			return zipRecords.size();
		}
		
		public Iterator iterator(){
			return zipRecords.keySet().iterator();
		}
	}

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		ClassloadingBenchmark runner = new ClassloadingBenchmark();
		runner.run(args);
	}
	
	public void run(String[] args){
		try {
			
			/* check the arguments */
			if (args.length <1){
				System.out.println("ERROR: Missing required arguments !");
				System.out.println("	First argument is jar containing test entries");
				return;
			}
			
			if ((new File(args[0]).canRead() != true)){
				System.out.println("ERROR: cannot read jar file specified !");
				return;
			}
			
			// get a all the entries in the jar
			ZipRecord record = new ZipRecord(new ZipFile(args[0]));
			System.out.println("Loaded " + args[0] + " which contains " + record.getCount() + " test classes");
		
			// warm up to avoid resolve first time we run
			System.nanoTime();
			this.getClass().isAssignableFrom(args[0].getClass());
			Iterator classIterWarmup = record.iterator();
			classIterWarmup.hasNext();
			classIterWarmup.next();
			
			// let things settle a bit
			Thread.sleep(5);
						
			Iterator classIter = record.iterator();
	
			long startTime = System.nanoTime();
			while (classIter.hasNext()) {
				String className = (String) classIter.next();
				Class.forName(className);
			}
			long endTime = System.nanoTime();
			
			System.out.println("Took " + (endTime-startTime) + " nanoseconds to read in " + record.getCount() + " classes");
		} catch (Exception e){
			System.out.println("Unexpected exception:" + e);
			e.printStackTrace();
		}
	}
}
