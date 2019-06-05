/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

import java.lang.Thread.State;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import org.testng.log4testng.Logger;

/**
 * @author mesbaha
 * ZipTestClassLoaderMT extends {@link jit.test.jar.ZipTestClassLoader} and overrides the processZipFiles(java.lang.String) method 
 * in order to accommodate multi-threaded class loading.
 */
public class ZipTestClassLoaderMT extends ZipTestClassLoader {
	private static Logger logger = Logger.getLogger(ZipTestClassLoaderMT.class);

	/*By default, we use 5 threads to load classes*/
	private static final int JARS_PER_THREAD = 5;
	
	private boolean pauseRequested = false;
	
	private boolean stopCompilation = false;
	
	private boolean parallelLoad = false; 
	
	private ArrayList<CompilationThread> compilationThreadList = null;

	
	/**
	 * Default Constructor for ZipTestClassLoaderMT
	 * @param lifespan life space of a surrogate class loader, a smaller value means more class loaders used
	 * @param forceGC flag to indicate whether GC should be forced
	 * @param parallelLoad flag to indicate whether multiple threads should be used for class loading
	 */
	public ZipTestClassLoaderMT ( int lifespan, boolean forceGC , boolean parallelLoad ) {
		super( lifespan, forceGC);
		this.parallelLoad = parallelLoad;
		this.compilationThreadList = new ArrayList<CompilationThread> () ;
	}
	
	/* (non-Javadoc)
	 * @see jit.test.jar.ZipTestClassLoader#processZipFiles(java.lang.String)
	 * This method is used for loading classes in a multi-threaded manner instead of sequential loading which is what 
	 * the super class version of processZipFiles() does. This method uses one dedicated class loader thread for each 
	 * subset of JARS_PER_THREAD number of jars to be loaded*/
	@SuppressWarnings("rawtypes")
	@Override
	public void processZipFiles( String classFilter ) {
		/*Iterate zip files in order*/
		Iterator classIter = orderedZipRecords.iterator();
		
		errMsgs = new HashMap();
		
		int jarCounter = 0 ; 
		CompilationThread clThread = null ; 
		ArrayList<ZipRecord> jarList = new ArrayList<ZipRecord>();
		
		/*If parallel load is chosen, we dedicate one class loader thread per JARS_PER_THREAD number of jars*/
		if ( parallelLoad ) {
			/*Iterate through the given jar files*/
			while( classIter.hasNext() ) {
				jarCounter ++ ; 
				
				/*Once we have counted 'n' number of jars, we want to dedicate the loading task of this subset to a new thread*/
				if ( jarCounter % JARS_PER_THREAD == 0 ) {
					clThread = new CompilationThread( this, jarList, classFilter );
					compilationThreadList.add(clThread);
					if ( jarCounter < orderedZipRecords.size() ) {
						jarList = new ArrayList<ZipRecord>();
					}
				}
				
				ZipRecord record =  (ZipRecord) classIter.next(); // a jar file 
				jarList.add(record); // construct a list of jars 
			}
		
			/*Create a new class loader thread if we have left-over jars*/
			if ( jarList.size() != 0 ) {
				clThread = new CompilationThread ( this, jarList, classFilter );
				compilationThreadList.add( clThread );
			}
		}
		
		/*If parallel load is NOT requested, we only use one class loader thread*/
		else {
			while( classIter.hasNext() ) {
				ZipRecord record =  (ZipRecord) classIter.next(); // a jar file 
				jarList.add(record); // construct a list of jars 
			}
			clThread = new CompilationThread( this, jarList, classFilter);
			compilationThreadList.add(clThread);
		}
		
		logger.debug("Total JarTesterMT compilation thread to be used = " + compilationThreadList.size() );
		
		/*Once we have divided up the class loading work to threads, start them off*/
		for ( int i = 0 ; i < compilationThreadList.size() ; i ++ ) {
			compilationThreadList.get(i).start();
		}
	
		/*Wait for all threads to finish, process pause, resume and stop requests if they are issued*/
		
		boolean stillLoading = true;
		
		while ( stillLoading ) {
			
			/*If class loading pause has been requested, pause all running class loader thread and sleep*/
			if ( pauseRequested ) {
				logger.debug("INFO: ZipTestClassLoaderMT: Compilation PAUSE request received, suspending compilation...");
				for ( int i = 0 ; i < compilationThreadList.size() ; i++ ) {
					compilationThreadList.get(i).requestPause();
				}
				
				while( pauseRequested ) {
					try {
						Thread.sleep( 1000 );
					} catch (InterruptedException e) {
						e.printStackTrace();
					}
				}
				
				/*When resumption is requested after pause, wake up the class loader threads and resume business*/
				logger.debug("INFO: ZipTestClassLoaderMT: Compilation RESUMPTION request received, starting compilation...");
				
				for (int i = 0 ; i < compilationThreadList.size() ; i++) {
					compilationThreadList.get(i).requestResumption();
					compilationThreadList.get(i).interrupt();
				}
			}
			
			/*If user requested to abort class loading, issue stop request to each class loader thread, make sure 
			 * each class loader thread exited and quit*/
			if ( stopCompilation ) {
				
				logger.debug("INFO: ZipTestClassLoaderMT: Compilation STOP request received, attempting to stop compilation threads...");
				
				for ( int i = 0 ; i < compilationThreadList.size() ; i++ ) {
					compilationThreadList.get(i).requestStop();
				}
				
				while ( isStillCompiling() ) {
					try {
						Thread.sleep(200);
					}
					catch(InterruptedException e){
						e.printStackTrace();
					}
				}
				
				logger.debug("INFO: ZipTestClassLoaderMT: Compilation threads have all exited");
			}
			
			/*During regular operation of class loading, the 'main' ZipTestClassLoaderMT thread should mostly sleep*/
			/*Check if there is at least one class loader thread alive, if yes, we are still not done*/
			stillLoading = isStillCompiling();
			
			if( stillLoading ) {
				try {
					Thread.sleep( 3000 );
				} catch ( InterruptedException e ) {
					e.printStackTrace();
				}
			}
		}
			
		/*Accumulate the results*/
		int error = 0 ; 
		int classCount = 0 ; 
		
		for ( int i = 0 ; i < compilationThreadList.size() ; i++ ) {
			error += compilationThreadList.get(i).getTotalErrorCount();
			classCount += compilationThreadList.get(i).getTotalClassCount();
		}

		logger.debug( "Total class compiled = " + classCount );
		logger.debug( "Total error = " + error );
	}
	
	/*The thread class to be used for compilation*/
	private static class CompilationThread extends Thread {
		private ZipTestClassLoaderMT loader ; 
		private boolean wait;
		private boolean stopCompilation;
		private ArrayList<ZipRecord> jarsToLoad;
		private int cCount;
		private int eCount;
		private String classFilter;
		
		public CompilationThread ( ZipTestClassLoaderMT loader, ArrayList<ZipRecord> jarsToLoad, String f ) {
			this.loader = loader;
			this.jarsToLoad = jarsToLoad;
			this.classFilter = f;
			cCount = 0; 
			eCount = 0;
		}
		
		@SuppressWarnings("rawtypes")
		public void run () {
			boolean stopped = false;
			this.setName("JarTesterMT compilation " + this.getName());
			for ( int m = 0 ; m < jarsToLoad.size() ; m ++ ) {
				
				if ( stopped ) {
					break;
				}
				
				int jarLevelClassCount = 0 ; 
				int jarLevelErrorCount = 0 ; 
			    ZipRecord aJar = jarsToLoad.get(m);
				Iterator entryIter = aJar.entries.entrySet().iterator(); 
			    
			    while( entryIter.hasNext() ) {
			    	String className = null;
					ZipEntry zipEntry = null;
					Map.Entry classPair = null;
					
			    	classPair = (Map.Entry)entryIter.next();
			   
			    	/*Check if compilation should be paused*/
		            if ( wait ) {
	                   synchronized(this) {
	                	   try {
								wait();
							} catch (InterruptedException e) {}
	                   	}
	                }
		            
		            /*Check if compilation should be stopped*/
		            if ( stopCompilation ) {
                    	stopped = true;
                    	break;
	                }

		            zipEntry = (ZipEntry) classPair.getValue();
					className = (String) classPair.getKey();
					
					/*Compile the class only if it's name contains classFilter*/ 
					if ( className.indexOf(classFilter) < 0 ) {
						continue;
					}
					
					Class clazz = loader.defineClass( aJar.file, zipEntry, className );
		
					if ( clazz != null ) {
						jarLevelClassCount++;
						cCount++;
						synchronized ( this ) {
							loader.surrogate.age++;
						}
					} else {
						jarLevelErrorCount++;
						eCount++;
					}
				}
			    
			    StringBuffer buff = new StringBuffer();
				buff.append(jarLevelClassCount);
				buff.append("/");
				buff.append(aJar.classCount);
				buff.append(" classes compiled in ");
				buff.append(aJar.file.getName());
				buff.append(", ");
				buff.append("by thread: " + Thread.currentThread().getName());
				buff.append(", ");
				buff.append(jarLevelErrorCount);
				buff.append(" errors reported.");
				logger.debug(buff.toString());
				cCount += jarLevelClassCount;
				eCount += jarLevelErrorCount;
			}
		}
		
		public int getTotalErrorCount () {
			return eCount;
		}
		public int getTotalClassCount () {
			return cCount;
		}
		public void requestPause() {
			wait = true;
		}
		public void requestResumption() {
			wait = false;
		}
		public void requestStop() {
			stopCompilation = true;
		}
	}

	/* (non-Javadoc)
	 * @see jit.test.jar.ZipTestClassLoader#defineClass(java.util.zip.ZipFile, java.util.zip.ZipEntry, java.lang.String)
	 */
	@SuppressWarnings("rawtypes")
	@Override
	protected Class defineClass( ZipFile file, ZipEntry entry, String className ) {
		Class clazz = super.defineClass( file, entry, className );
		if ( clazz != null ) {
			if ( Compiler.compileClass( clazz ) == false ) {
				logger.error( "Compilation of " + className + " failed" );
				return null;
			}
		}
		return clazz;
	}
	
	/**
	 * @return true if any class loader thread is still alive
	 */
	public boolean isStillCompiling() {
		for ( int i = 0 ; i < compilationThreadList.size() ; i++ ) {
			if ( compilationThreadList.get(i).isAlive()) {
				return true;
			}
		}
		return false;
	}
	
	/**
	 * This method is used externally to issue a request to pause all the class loader threads 
	 * The method returns only when all compilation child threads have been suspended
	 */
	public void pauseCompilation() {
		
		//first make sure the threads are running
		for ( int i = 0 ; i < compilationThreadList.size() ; i++ ) {
			while( compilationThreadList.get(i).getState().compareTo(State.WAITING) == 0 ) {
				try {
					Thread.sleep(50);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
		}
		
		//request pause
		pauseRequested = true;
		
		//make sure the threads have been paused
		for ( int i = 0 ; i < compilationThreadList.size() ; i++ ) {
			while( compilationThreadList.get(i).getState().compareTo(State.RUNNABLE) == 0) {
				try {
					Thread.sleep(50);
					//logger.debug("Waiting for thread to pause : " + compilationThreadList.get(i).getName() );
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
		}
		
	}
	
	/**
	 * This method is used externally to issue a request to resume all paused class loader threads 
	 * The method returns only when all compilation child threads have been woken up
	 */
	public void resumeCompilation() {
		
		pauseRequested = false;
		
		for ( int i = 0 ; i < compilationThreadList.size() ; i++ ) {
			while( compilationThreadList.get(i).getState().compareTo(State.WAITING) == 0 ) {
				try {
					Thread.sleep(50);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
		}
	}
	
	/**
	 * This method is used externally to issue a request to stop all class loader threads
	 * The method returns 
	 */
	public void stopCompilation() {
		
		stopCompilation = true;
		
		while ( isStillCompiling() ) {
			try {
				Thread.sleep(200);
			}
			catch(InterruptedException e){
				e.printStackTrace();
			}
		}
	}
}
