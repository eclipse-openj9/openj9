/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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

package tests.sharedclasses;

import java.io.InputStream;

import java.util.*;
import java.io.*;

/** 
 * Instances of StreamGobbler are used to capture output from the spawned process
 */
class StreamGobbler extends Thread {
    private InputStream is; // Can be used for stderr/stdout
    private String type; // some identifier tag for this gobbler (possibly STDERR or STDOUT)
    
    private List<String> expected = new ArrayList<String>(); // Expected lines of output to be found in the gobbled output
    private String waitForMessage;
    
    private StringBuffer gobbledData = new StringBuffer();
    private List lines = new ArrayList();
    public volatile boolean finished = false;
    public volatile boolean waitForMessageFound = false;
    public volatile boolean ignoreRest = false;
    
    StreamGobbler(InputStream is, String type, String[] expected, String waitForMessage) {
        this.is = is;
        this.type= type;
        this.waitForMessage = waitForMessage;
        if (expected!=null) {
	        for (int i = 0; i < expected.length; i++) {
				String string = expected[i];
				if (string!=null)
				this.expected.add(string);
			}
        }
    }
    
    public String getType() { return type; }
    
    public void run() {
        try {
            InputStreamReader isr = new InputStreamReader(is);
            BufferedReader br = new BufferedReader(isr);
            String line=null;
            while ( (line = br.readLine()) != null) {
            	//System.out.println(type + ">" + line);
                gobbledData.append(/*type + ">" + */line+"\n");
                lines.add(line);
                int pos = 0;
                int found =-1;
                for (Iterator iterator = expected.iterator(); iterator.hasNext();) {
					String name = (String) iterator.next();
					if (line.indexOf(name) != -1) {
						found = pos;
						break;
					}
					pos++;
				}
                if (found != -1) {
                	expected.remove(found);
                }
                if ((false == waitForMessageFound) && (null != waitForMessage) && (line.indexOf(waitForMessage) != -1)) {
                	waitForMessageFound = true;
                }
            }
            finished=true;
        } catch (IOException ioe) {
        	if (ignoreRest == false) {
        		ioe.printStackTrace();
        	}
        }
    }
    
    public boolean allExpectedMessagesWereFound() {
    	return expected.isEmpty();
    }

	public void dumpOutput() {
		System.out.println(gobbledData.toString());
	}
	
	public String getOutput() {
		return gobbledData.toString();
	}

	public String[] getOutputAsLines() {
		return (String[])lines.toArray(new String[]{});
	}
}

public class RunCommand {
	private static Process lastProcess;
	public static String lastCommand, the2ndLastCommand;
	public static String lastCommandStdout, the2ndLastCommandStdout;
	public static String[] lastCommandStdoutLines;
	public static String lastCommandStderr, the2ndLastCommandStderr;
	public static String[] lastCommandStderrLines;
	public static boolean logCommands = false;
	public static int processRunawayTimeout = 120000; // 2 min timer, kill process if its taking longer!
	
	/* if the process executed last is still running, it returns the process object, else returns null */
	public static Process getLastProcess() {
		try {
			if (null != lastProcess) {
				lastProcess.exitValue();
			}
			/* process got terminated */
			return null;
		} catch (IllegalThreadStateException e) {
			/* process is still running */
			return lastProcess;
		}
	}
	
	public static void executeMayFail(String cmd) {
		execute(cmd,(String[])null,(String[])null,true,true, null);
	}
	
	public static void execute(String cmd) {
		execute(cmd,(String[])null,(String[])null,true,false, null);
	}
	
	public static void execute(String cmd,boolean careAboutExitValue) {
		execute(cmd,(String[])null,(String[])null,careAboutExitValue,false, null);
	}
	
	public static void execute(String cmd, String waitForMessage) {
		execute(cmd,(String[])null, (String[])null, false, false, waitForMessage);
	}
	
	public static void execute(String cmd,String sysoutMessages,String syserrMessages,boolean careAboutExitValue) {
		execute(cmd,new String[]{sysoutMessages},new String[]{syserrMessages},careAboutExitValue,false, null);
	}
	
    public static void execute(String cmd,String[] expectedSysoutMessages,String[] expectedSyserrMessages,boolean careAboutExitValue, boolean MayFail, String waitForMessage)
    {
    	int exitVal = 0;
        Runtime rt = Runtime.getRuntime();
        the2ndLastCommand = lastCommand;
        the2ndLastCommandStdout = lastCommandStdout;
        the2ndLastCommandStderr = lastCommandStderr;
        lastCommand = null;
        lastCommandStdout = null;lastCommandStderr = null;
        lastCommandStdoutLines = null;lastCommandStderrLines = null;
        lastProcess = null;
        
        try {
        	if (logCommands) {
        		System.out.println("Executing command: "+cmd);
        	}
        	long processStartTime = System.currentTimeMillis();
        	lastProcess = rt.exec(cmd);
        	// any error message?
        	StreamGobbler errorGobbler = new 
            	StreamGobbler(lastProcess.getErrorStream(), "ERROR",expectedSyserrMessages, null);            
        
        	// any output?
        	StreamGobbler outputGobbler = new 
            	StreamGobbler(lastProcess.getInputStream(), "OUTPUT",expectedSysoutMessages, waitForMessage);
            
        	// kick them off
        	errorGobbler.start();
        	outputGobbler.start();
                                
        	// any error???
        	if (null == waitForMessage) {
	        	exitVal = lastProcess.waitFor();
	            while (!outputGobbler.finished || !errorGobbler.finished) {
	            	try { Thread.sleep(250); } catch (Exception e) {}
	            	if ((System.currentTimeMillis()-processStartTime)>processRunawayTimeout) {
	            		// kill the process, it is taking too long!
	            		lastProcess.destroy();
	            		try {
	            			lastProcess.waitFor();
						} catch (java.lang.InterruptedException e) {
							e.printStackTrace();
						}
	            		System.err.println("Process killed, was executing for longer than "+(processRunawayTimeout/1000)+"seconds");
	            	}
	            }
		        
		        lastCommandStdout = outputGobbler.getOutput();
		        lastCommandStderr = errorGobbler.getOutput();
		        lastCommandStdoutLines = outputGobbler.getOutputAsLines();
		        lastCommandStderrLines = errorGobbler.getOutputAsLines();
		        lastCommand = cmd;
		        if (!outputGobbler.allExpectedMessagesWereFound()) {
		        	outputGobbler.dumpOutput();
		        	errorGobbler.dumpOutput();
		        	throw new RuntimeException("Not all expected messages were found");
		        }
		        if (!errorGobbler.allExpectedMessagesWereFound()) {
		        	outputGobbler.dumpOutput();
		        	errorGobbler.dumpOutput();
		        	throw new RuntimeException("Not all expected messages were found");
		        }
		        //System.out.println("ExitValue: " + exitVal);       
		        if (careAboutExitValue && exitVal!=0) {
		        	if (MayFail==false) {
		        		outputGobbler.dumpOutput();
		        		errorGobbler.dumpOutput();
		        		throw new RuntimeException("TestFailed: exitvalue="+exitVal);
		        	}
		        }
	        } else {
	        	while (false == outputGobbler.waitForMessageFound) {
	        		try { 
	        			Thread.sleep(250); 
	        		} catch (Exception e) {
	        		}
	            	if ((System.currentTimeMillis() - processStartTime) > processRunawayTimeout) {
	            		// kill the process, it is taking too long!
	            		lastProcess.destroy();
	            		try {
	            			lastProcess.waitFor();
						} catch (java.lang.InterruptedException e) {
							e.printStackTrace();
						}
	            		System.err.println("Process killed, was taking longer than "+(processRunawayTimeout/1000)+" seconds to generate expected output");
	            	}
	        	}
	        	outputGobbler.ignoreRest = true;
                errorGobbler.ignoreRest = true;
	        }
        } catch (IOException ioe) {
        	throw new RuntimeException("Problem invoking command: "+ioe.toString());
        } catch (InterruptedException ie) {
        	throw new RuntimeException("Problem invoking command: "+ie.toString());
		}
    
    }
}