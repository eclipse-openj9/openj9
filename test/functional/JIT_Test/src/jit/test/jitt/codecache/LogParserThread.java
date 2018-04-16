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
package jit.test.jitt.codecache;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * LogParserThread class is used by the JIT code cache harness to monitor JIT verbose log on a continuous basis
 * and provide information related to JIT code cache events being logged.
 *
 * @author mesbah
 */
public class LogParserThread extends Thread{

	/*Path to the JIT verbose log file*/
	private String logFileName;

	/*Pivot value indicating the line number in the verbose log file upto which the log file was parsed
	 * before the parser thread went into sleep*/
	private int lastCursorPosition;

	/*Value indicates the size of the verbose log file when it was last parsed*/
	private long lastSize;

	/*Flag indicating if an external request to stop LogParserThread has been received*/
	private boolean stopRequested;

	/*Flag indicating if maximum code cache has been allocated*/
	private boolean isCodeCacheMaxAllocated = false;

	/*Flag indicating if space reuse is happening*/
	private boolean isSpaceReuseHappening = false;

	/*Counter for unloaded methods*/
	private int unloadCount;

	/*Code Cache Map index*/
	private int ccIndex = 0 ;

	/*Indicator of the CodeCacheObj object in code cache map that represents the code cache being
	 * allocated into recently*/
	private int currentCodeCache = 0 ;

	/*Flag indicating test type, depending on this varied levels of parsings are performed*/
	private int testType;

	/*Flag indicating if method reclamation has been detected in recent past*/
	private boolean reclamationTookPlace = false;

	/*Code cache map holds information related to created code caches and methods allocations*/
	private ConcurrentHashMap < String, CodeCacheObj > codeCacheMap = null;

	private ArrayList<MethodObj> aotMethodList = null;

	/**
	 * Default constructor of LogParserThread.
	 * @param logFileName - Full path to the JIT verbose log file to monitor
	 * @param testType - Level of parsing to perform
	 */
	public LogParserThread( String logFileName, int testType, int startingOffset ) {
		this.logFileName = logFileName;
		this.testType = testType;
		this.codeCacheMap = new ConcurrentHashMap < String, CodeCacheObj > ();
		this.lastCursorPosition = startingOffset;
		this.aotMethodList = new ArrayList<MethodObj>();
	}

	/* The LogParser thread sleeps for 2 seconds, wakes up and performs parsing on log file to extract
	 * latest updates to it, then it goes back to sleep again. The process continues until no update
	 * to log is available or stop is requested externally by TestManager.
	 * ( non-Javadoc )
	 * @see java.lang.Thread#run()
	 */
	public void run() {
		System.out.println("Starting LogParserThread at offset : " + lastCursorPosition);
		while( true ) {
			try{
				Thread.sleep( 2000 );
			} catch ( InterruptedException e ) {
				e.printStackTrace();
			}

			long currentSize = new File( logFileName ).length();

			// if new update is available, parse starting from the line after the last parsed line
			if ( currentSize > lastSize ) {
				lastSize = currentSize;
				parse();
			}

			// break if stop has been requested
			if ( stopRequested ) {
				break;
			}
		}
	}

	/**
	 * The parser thread parses the JIT verbose log file starting from the line after the last line
	 * that was parsed before the LogParserThread went into its last sleep.
	 * This method updates the flags that are used by the query methods referenced by various
	 * code cache tests
	 */
	private void parse() {

		int currentCurorPosition = 0 ;

		try {
			BufferedReader br = new BufferedReader( new FileReader( logFileName ) );

			while ( true ) {

				String aLine = br.readLine();

				currentCurorPosition++;

				if ( aLine == null ) {
					break;
				}

				if ( currentCurorPosition != (lastCursorPosition + 1) ) {
					continue;
				}

				/*Build up & maintain code cache map when the "# CodeCache Allocated.." message is encountered in the verbose log
				 * It is important to perform this step here in case if we clear previously built code cache map in between trampoline tests*/

				if ( aLine.startsWith( Constants.CC_ALLOCATED ) ) {
					String mccAddress = aLine.substring( Constants.CC_ALLOCATED.length()+1, aLine.indexOf( "@" )-1 ) ;
					//if ( codeCacheMap.containsKey( String.valueOf( currentCodeCache+1 )  ) == false ) {  //if ( codeCacheMap.containsKey( mccAddress ) == false ) {
						String ccAddrRange = aLine.substring( aLine.indexOf( "@" ) + 2 );
						String [] tokens = ccAddrRange.split( "-" );

						if ( tokens.length == 2 ) {

							String startAddr = tokens[0].toUpperCase();
							String endAddr = tokens[1].toUpperCase();

							if ( startAddr.startsWith("0X")) {
								startAddr = startAddr.substring(2);
							}

							if ( endAddr.startsWith("0X")) {
								endAddr = endAddr.substring(2);
							}

							CodeCacheObj ccObj = new CodeCacheObj( mccAddress, startAddr, endAddr );

							/*By default, we are configured to start with 4 code caches which are listed from bottom up, so adjust the code cache index accordingly
							 This step is ignored if user has set -Xjit:numCodeCacheOnStartup=1*/

							if ( ccIndex < 4 && !TestManager.isSingleCodeCacheStartup() ) {
								currentCodeCache = 3 - ccIndex;
							}
							else {
								currentCodeCache = ccIndex;
							}

							codeCacheMap.put( String.valueOf( currentCodeCache ) , ccObj );
							ccIndex++;

							//System.out.println("\nNew CodeCache DataStructure created at : " + currentCodeCache);//TODO: remove later
							//System.out.println("code cache alloc count in code cache at zero "+ codeCacheMap.get( "0").getAllocCount() + "\n") ;//TODO : remove later
						}
					//}
				}

				else if ( ( testType == Constants.BASIC_TEST || testType == Constants.AOT_TEST )
						&& ( aLine.startsWith( Constants.PREFIX_JITTED ) || aLine.startsWith( Constants.METHOD_RECLAIMED ) || aLine.startsWith( Constants.METHOD_RECOMPILED ) )
						&& aLine.contains( "@" )
						&& aLine.contains( "interpreted" ) == false ) {
					String ccAddressRange = null,
			 		methodSignature = null,
			 		className = null,
			 		optLevel = null,
			 		queueSize = null,
					gc=null,
					atlas=null,
					time=null,
					compThread=null,
					mem=null,
					profiling=null;
					boolean sync = false;
					boolean reclaimed = false;
					boolean recompiled = false;

					try {
						if ( aLine.startsWith( Constants.PREFIX_JITTED ) ) {
							int a = aLine.indexOf( "(" );
							int b = aLine.indexOf( ")" ) + 1;
							optLevel = aLine.substring(a, b);
							methodSignature = aLine.substring( aLine.indexOf( ")" ) + 2, aLine.indexOf( "@" ) - 1 );
							if ( aLine.contains(Constants.AOT_LOAD) ) {
								ccAddressRange = aLine.substring( aLine.indexOf("@") + 2 , aLine.indexOf("compThread") -1 ) ;
							} else {
								ccAddressRange = aLine.substring( aLine.indexOf( "@" ) + 2, aLine.indexOf( "Q_SZ" ) - 1 );
							}
						} else if ( aLine.startsWith( Constants.METHOD_RECLAIMED ) ) {
							reclaimed = true;
							reclamationTookPlace = true;
							methodSignature = aLine.substring( aLine.indexOf( Constants.METHOD_RECLAIMED ) + Constants.METHOD_RECLAIMED.length() + 1, aLine.indexOf( "@" ) - 1 );
							int start = aLine.indexOf( "@" ) + 2;
							int end = aLine.lastIndexOf( "(" ) -1 ;
							if ( start > end ) {
								end = aLine.length() -1 ;
							}
							ccAddressRange = aLine.substring( start, end );
						} else if ( aLine.startsWith( Constants.METHOD_RECOMPILED ) ) {
							recompiled = true;
							methodSignature = aLine.substring( aLine.indexOf( Constants.METHOD_RECOMPILED ) + Constants.METHOD_RECOMPILED.length() + 1, aLine.indexOf( "@" ) - 1 );
							ccAddressRange = aLine.substring( aLine.indexOf("->") + 3 , aLine.lastIndexOf("(") );
						}


						MethodObj currentMethod = new MethodObj( optLevel, className, methodSignature, ccAddressRange, queueSize, gc, atlas, time, compThread, mem, profiling, sync, reclaimed, recompiled );

						/*If we are performing AOT test, keep track of methods that
						 * have been loaded from shared class cache*/
						if ( aLine.startsWith(Constants.AOT_LOAD) && testType == Constants.AOT_TEST ) {
							aotMethodList.add(currentMethod);
						}

						Set<String> keys = codeCacheMap.keySet();
						Iterator<String> iter = keys.iterator();

						/*If method has been recompiled and it is a method we have loaded ourselves
						remove it from its previous code cache*/
						if ( currentMethod.isRecompiled() ) {
							if( currentMethod.getMethodSignature().contains("jit/test/codecache") ) {
								while ( iter.hasNext() ) {
									String aKey = iter.next();
									CodeCacheObj ccObj = codeCacheMap.get( aKey );
									boolean oldRemoved = ccObj.removeMethod(currentMethod);
									if ( oldRemoved ) {
										System.out.println("Old CodeCacheObj location removed for : "+ currentMethod.getMethodSignature());
										break;
									}
								}
							}
						}

						/*Add info of this method entry in its CodeCacheObj that represents the JIT
						 * code cache to which this method has been allocated
						 */
						Iterator<String> iter2 = keys.iterator();

						String range = currentMethod.getCcAddressRange();

						while ( iter2.hasNext() ) {
							String aKey = iter2.next();
							CodeCacheObj ccObj = codeCacheMap.get( aKey );
							boolean contains = false;

							/*For zLinus and zOS , the address range value of methods could be of form addr11-addr12/addr21-addr22
							 * In that case, we make sure that both the address ranges of the hot body and cold body of the method are
							 * in a particular code cache before declaring that the method belongs to that code cache.*/
							if ( range.contains("/")) {

								String ranges [] = range.split("/");

								if ( ccObj.addressRangeIncludes(ranges[0]) && ccObj.addressRangeIncludes(ranges[1]) ) {
									contains = true;
								}
							}

							/*For all other platforms, the address range value of method is of form addr1-addr2*/
							else {
								contains = ccObj.addressRangeIncludes(range);
							}

							if ( contains ) {

								currentMethod.setCodeCacheIndicator( Integer.parseInt( aKey ) );

								//System.out.println("Adding method " + currentMethod.getMethodSignature() + " To code cache " + ccObj + " at : " + aKey);//TODO: remove later

								ccObj.addMethod(currentMethod);
								break;
							}
						}

						/* if reclamation happened lately and this method is being compiled into a code cache that was
						 * filled previous to the 'current' code cache, space re-use is happening.
						*/
						if ( currentCodeCache > 1
								&& reclamationTookPlace
								&& currentMethod.isJitted() )  {
							for ( int i = 0 ; i < currentCodeCache ; i++ ) {
								if ( i == currentMethod.getCodeCacheIndicator() ) {
									isSpaceReuseHappening = true;
									break;
								}
							}
						}
					} catch ( StringIndexOutOfBoundsException e ) {
						e.printStackTrace();
						System.out.println( "Error occurred while parsing following line: " );
						System.out.println( aLine );
						System.out.println( ".........." );
					}
				}

				if ( aLine.contains( Constants.CC_MAX_ALLOCATED ) ) {
					isCodeCacheMaxAllocated = true;
				}

				if ( aLine.contains( Constants.METHOD_UNLOADED ) ) {
					unloadCount ++ ;
				}

				lastCursorPosition = currentCurorPosition;
			}
		} catch ( FileNotFoundException e ) {
			e.printStackTrace();
		} catch ( IOException e ) {
			e.printStackTrace();
		}
	}

	/**
	 * Causes the LogParserThread to stop
	 */
	public void requestStop() {
		stopRequested = true;
	}

	/**
	 * Query method for basic test indicating if the first set of code caches have been created
	 * @return
	 */
	public boolean isCodeCacheCreated() {
		return codeCacheMap.size() > 0;
	}

	/**
	 * Query method for boundary test
	 * @return
	 */
	public boolean isCodeCacheMaxAllocated() {
		return isCodeCacheMaxAllocated;
	}

	/**
	 * Query method for basic test to check if the first code cache is populating
	 * @return
	 */
	public boolean isCachePopulating() {
		CodeCacheObj ccObj1 =  codeCacheMap.get( "0" );
		if ( ccObj1 != null ) {
			return ccObj1.getAllocCount() > 0 ;
		}
		return false;
	}

	/**
	 * Query method for basic test to check if we are expanding to the second code cache after filling up the first one
	 * @return
	 */
	public boolean isCodeCacheExpanding() {
		CodeCacheObj ccObj2 =  codeCacheMap.get( "1" );
		if ( ccObj2 != null ) {
			return ccObj2.getAllocCount() > 0 ;
		}
		return false;
	}

	/**
	 * Query method for space reuse test
	 * @return
	 */
	public boolean isSpaceReuseHappening() {
		return isSpaceReuseHappening;
	}

	public int getUnloadCount() {
		return this.unloadCount;
	}

	public void clearCache() {
		Set<String> keys = codeCacheMap.keySet();
		Iterator<String> iter = keys.iterator();
		while ( iter.hasNext() ) {
			String aKey = iter.next();
			CodeCacheObj ccObj = codeCacheMap.get( aKey );
			ccObj.clearMethodList();
		}
		codeCacheMap.clear();
		codeCacheMap = null;
	}

	public boolean isAotLoaded( String methodSignature ) {
		for ( int i = 0 ; i < aotMethodList.size() ; i++ ) {
			if ( aotMethodList.get(i).getMethodSignature().equals(methodSignature) ) {
				return true;
			}
		}
		return false;
	}

	public int getCodeCacheDifference ( String callee, String caller ) {
		Set<String> keys = codeCacheMap.keySet();
		Iterator<String> iter = keys.iterator();

		int ccIndicatorCallee = -1;
		int ccIndicatorCaller = -1;
		int returnVal = -1;

		while ( iter.hasNext() ) {
			String aKey = iter.next();
			CodeCacheObj ccObj = codeCacheMap.get( aKey );

			if ( ccIndicatorCallee == -1 && ccObj.hasMethod(callee) ) {
				ccIndicatorCallee = Integer.parseInt (aKey) ;
			}

			if ( ccIndicatorCaller == -1  && ccObj.hasMethod(caller) ) {
				ccIndicatorCaller = Integer.parseInt (aKey) ;
			}

			if( ccIndicatorCallee != -1 && ccIndicatorCaller != -1 ) {
				returnVal = Math.abs(ccIndicatorCallee - ccIndicatorCaller);
				break;
			}
		}

		if ( ccIndicatorCallee == -1 ) {
			//System.out.println("Can not find in any code cache, method : " + callee );
		} else {
			//System.out.println("Found code cache for method : " + callee + " - " + ccIndicatorCallee);
		}

		if ( ccIndicatorCaller == -1 ) {
			//System.out.println("Can not find in any code cache, method : " + caller );
		} else {
			//System.out.println("Found code cache for method : " + caller + " - " + ccIndicatorCaller);
		}

		return returnVal;
	}

	public int getLastCursorPosition () {
		return this.lastCursorPosition;
	}

	public int getCurrentCodeCacheIndex() {
		return this.ccIndex;
	}

	public static void main ( String [] args ) {
		LogParserThread p = new LogParserThread("C:\\codecachetest\\jit.test.jitt.aotcc.log",Constants.AOT_TEST,0);
		p.start();
	}
}
