/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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

import java.io.*;
import java.util.Iterator;
import java.util.Vector;

class CommandExecuter implements Runnable, Command {
	private String _command;
	private String _background;
	private String _variableStdout;
	private String _variableStderr;
	private String _return;
	private Vector _args;
	
	CommandExecuter( String cmd, String background, String var1, String var2, String ret) {
		_command = cmd;
		_background = background;
		_variableStdout = var1;
		_variableStderr = var2;
		_return = ret;
		_args = new Vector();
	}
	
	public void executeSelf() {
		String bg = TestSuite.evaluateVariables( _background );
		if (bg != null && bg.equalsIgnoreCase("yes")) {
			new Thread( this ).start();
		} else {
			this.run();
		}
	}

	public void run() {
		/**
		 * Set up a built-in variable "Q" to represent a double-quotes(").
		 * The command comes with $Q$ initially set up by tester to quote the classpath with white spaces.
		 * $Q$ in the command string will be replaced with a double-quotes(") by the parser of 
		 * test framework before passing it over to Tokenizer for further processing.
		 * @see Tokenizer
		 */
		TestSuite.putVariable("Q", "\"");
		StringBuffer buf1 = new StringBuffer();
		StringBuffer buf2 = new StringBuffer();
		String command = TestSuite.evaluateVariables( _command );
		System.out.println( "Executing command: " + command );
		try {
			/**
			 * According to the test framework, a command string is passed over to exec(String command,...) of Runtime for test execution.
			 * However, the method is unable to recognize a command string if white spaces occur in the classpath of the command string.
			 * To solve the issue, exec(String command,...) is replaced with exec(String[] cmdarray,...), in which case it requires that
			 * a command string be replaced with a command array with all arguments split up in the array.
			 * Meanwhile, a path of .jar file with white spaces should be treated as a single argument in the command array.
			 * Thus, a new class called Tokenizer is created to address the issue of classpath when splitting up a command string.
			 * NOTE: The reason why StreamTokenizer was discarded is that it wrongly interpreted escape characters (e.g. \b, \n, \t)
			 * in the classpath into a single character rather than two characters.
			 * @see Tokenizer
			 */
			String[] cmdArray = Tokenizer.tokenize(command);
			Process proc; 
			if (_args.isEmpty()) {
				proc = Runtime.getRuntime().exec(
						cmdArray, 
						null, 
						new File( System.getProperty("user.dir") ) );
			} else {
				String[] args = new String[cmdArray.length + _args.size()];
				System.arraycopy(cmdArray, 0, args, 0, cmdArray.length);
				
				for (int i = 0; i < _args.size(); i++) {
					String arg = (String) _args.get(i);
					args[cmdArray.length + i] = TestSuite.evaluateVariables(arg);
				}
				proc = Runtime.getRuntime().exec(
						args,
						null,
						new File( System.getProperty("user.dir") ) );
			}
			// need to read from input/error streams as noted in
			// http://www.javaworld.com/javaworld/jw-12-2000/jw-1229-traps.html
			CaptureThread ct = new CaptureThread( proc.getInputStream(), (null == _variableStdout) ? null : buf1);
			ct.start();
			CaptureThread ce = new CaptureThread( proc.getErrorStream(), (null == _variableStderr) ? null : buf2);
			ce.start();
			ct.join();
			ce.join();
			int retcode = proc.waitFor();
			if (_return != null) {
				//System.out.println( '"' + command + "\" returned code: " + retcode );
				TestSuite.putVariable( _return, Integer.toString( retcode ) );
			}
		} catch (Exception e) {
			System.out.println("Error while executing command");
			e.printStackTrace();
		}
		if (null != _variableStdout) {
			String value = buf1.toString();
			//System.out.println( "Captured output from \"" + command + "\": " + value );
			TestSuite.putVariable(_variableStdout, value );
		}
		if (null != _variableStderr) {
			String value = buf2.toString();
			//System.out.println( "Captured output from \"" + command + "\": " + value );
			TestSuite.putVariable(_variableStderr, value );
		}
		System.out.println("");
	}
	
	class CaptureThread extends Thread {
		BufferedReader _br;
		StringBuffer _buf;
		
		CaptureThread( InputStream in, StringBuffer buf ) {
			_br = new BufferedReader( new InputStreamReader( in ) );
			_buf = buf;
		}

		public void run() {
			try {
				String read;
				while ((read = _br.readLine()) != null) {
					//System.out.println(read);
					if (_buf != null) {
						_buf.append( read );
					}
				}
			} catch (IOException ioe) {
				/* do nothing; user can fix problem based on captured output */
			}
		}
	}

	public void addArg(String string) {
		_args.add(string);
	}
}
