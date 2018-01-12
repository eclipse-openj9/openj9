/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.j9;

import java.util.Collections;
import java.util.Iterator;
import java.util.Properties;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.java.j9.JavaRuntime;

/**
 * @author jmdisher
 *
 */
public class ImageProcess implements com.ibm.dtfj.image.ImageProcess
{
	private Vector _runtimes = new Vector();
	private Vector _libraries = new Vector();
	private Vector _threads = new Vector();
	private int _pointerSize;
	private ImageModule _executable;
	private String _id;
	private String _commandLine;
	private Properties _environment;	//reading the env ahead of time like this is odd but saves us having to stuff the dump in here
	private ImageThread _currentThread;
	private long _faultingNativeID = 0;		//the ID of the native thread which caused the GPF (if there was one)
	private int _signalNumber = 0;
	private Exception _runtimeCheckFailure;
	private static final String JAVA_COMMAND_LINE_ENVIRONMENT_VARIABLE = "IBM_JAVA_COMMAND_LINE";
	
	
	public ImageProcess(String pid, String commandLine, Properties environment, ImageThread currentThread, Iterator threads, ImageModule executable, Iterator libraries, int pointerSize)
	{
		_id = pid;
		_commandLine = commandLine;
		_environment = environment;
		setCurrentThread(currentThread);
		setThreads(threads);
		_executable = executable;
		while (libraries.hasNext()) {
			_libraries.add(libraries.next());
		}
		_pointerSize = pointerSize;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getCommandLine()
	 */
	public String getCommandLine() throws DataUnavailable, CorruptDataException
	{
		// We can't get the command line from the core dump on zOS, or on recent Windows versions. On Linux
		// it may be truncated. The java launcher stores the command line in an environment variable, so for
		// all platforms we now try that first, with the core reader as a fallback.
		Properties environment = getEnvironment();
		String javaCommandLine = environment.getProperty(JAVA_COMMAND_LINE_ENVIRONMENT_VARIABLE);
		
		if (javaCommandLine != null) {
			return javaCommandLine;
		}
		if (_commandLine == null) {
			throw new DataUnavailable("Command line unavailable from core dump");
		}
		return _commandLine;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getEnvironment()
	 */
	public Properties getEnvironment() throws DataUnavailable,
			CorruptDataException
	{
		if (null == _environment)
		{
			//something went wrong so decide which kind of exception should be thrown
			if (null == _runtimeCheckFailure)
			{
				//no runtime startup failure so this must just be unavailable
				throw new DataUnavailable("Environment base address when core file parsed");
			}
			else
			{
				//this is probably because the XML was corrupt
				throw new CorruptDataException(new CorruptData("Environment not found due to:  " + _runtimeCheckFailure.getMessage(), null));
			}
		}
		else
		{
			//business as usual
			return _environment;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getID()
	 */
	public String getID() throws DataUnavailable, CorruptDataException
	{
		return _id;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getLibraries()
	 */
	public Iterator getLibraries() throws DataUnavailable, CorruptDataException
	{
		return _libraries.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getExecutable()
	 */
	public ImageModule getExecutable() throws DataUnavailable,
			CorruptDataException
	{
		if (null == _executable) {
			throw new DataUnavailable("Executable image not found");
		} else {
			return _executable;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getThreads()
	 */
	public Iterator getThreads()
	{
		return _threads.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getCurrentThread()
	 */
	public com.ibm.dtfj.image.ImageThread getCurrentThread() throws CorruptDataException
	{
		ImageThread current = _currentThread;
		
		if (0 != _faultingNativeID) {
			//look up this thread
			Iterator threads = getThreads();
			
			while (threads.hasNext()) {
				Object next = threads.next();
				if (next instanceof CorruptData)
					continue;
				ImageThread thread = (ImageThread) next;
				if (Long.decode(thread.getID()).longValue() == _faultingNativeID) {
					current = thread;
					break;
				}
			}
		}
		//ensure that this is sane
		if ((0 != _faultingNativeID) && (null == current)) {
			throw new CorruptDataException(new CorruptData("no current thread", null));
		}
		return current;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getRuntimes()
	 */
	public Iterator getRuntimes()
	{
		//left un-initialized so the compiler will warn us if we miss a code path
		Iterator iter;
		
		if (null == _runtimeCheckFailure)
		{
			//this is the normal case where we have a real list of runtimes to work with so just return the iterator
			iter = _runtimes.iterator();
		}
		else
		{
			// If something went wrong during startup to the point where we didn't even get to see what the JavaRuntime 
			// is (currently this happens if the XML is corrupt), fake up a corrupt data object and return it.
			iter = Collections.singleton(new CorruptData("No runtimes due to early startup error:  " + _runtimeCheckFailure.getMessage(), null)).iterator();
		}
		return iter;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getSignalNumber()
	 */
	public int getSignalNumber() throws DataUnavailable, CorruptDataException
	{
		int agreedSignal = (null == _currentThread) ? 0 : _currentThread.getSignal();
		if (0 != _signalNumber) {
			//this would happen if we are on a platform where we don't currently extract signal numbers from threads and the "gpf" tag over-rode us
			//TODO:  once those platforms can all access this information via the current thread, this _signalNumber ivar can be removed
			agreedSignal = _signalNumber;
		}
		return agreedSignal;
	}

	private static String[] names = {
		"ZERO",
		"SIGHUP",		// 1
		"SIGINT",		// 2 + win
		"SIGQUIT",
		"SIGILL",		// 4 + win
		"SIGTRAP",		// 5
		"SIGABRT",
		"SIGEMT",
		"SIGFPE",		// 8 + win
		"SIGKILL",
		"SIGBUS",		// 10
		"SIGSEGV",		// 11 + win
		"SIGSYS",
		"SIGPIPE",
		"SIGALRM",
		"SIGTERM",		// 15 + win
		"SIGUSR1",
		"SIGUSR2",
		"SIGCHLD",
		"SIGPWR",
		"SIGWINCH",		// 20
		"SIGURG/BREAK",	// 21 / win
		"SIGPOLL/ABRT",	// 22 / win
		"SIGSTOP",
		"SIGTSTP",
		"SIGCONT",		// 25
		"SIGTTIN",
		"SIGTTOU",
		"SIGVTALRM",
		"SIGPROF",
		"SIGXCPU",		// 30
		"SIGXFSZ",
		"SIGWAITING",
		"SIGLWP",
		"SIGAIO",		// 34
		"SIGFPE_DIV_BY_ZERO",		// 35 - synthetic from generic signal
		"SIGFPE_INT_DIV_BY_ZERO",	// 36 - synthetic from generic signal
		"SIGFPE_INT_OVERFLOW"		// 37 - synthetic from generic signal
	};
	private String resolvePlatformName(int num) {
		if (num >= 0 && num < names.length) return names[num];
		else return "Signal."+Integer.toString(num);
	}
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getSignalName()
	 */
	public String getSignalName() throws DataUnavailable, CorruptDataException
	{
		if (0 == getSignalNumber()) {
			// there was no signal so don't bother resolving
			return null;
		} else {
			// At this point we have a signal number to convert to a name which really should be done by
			// something platform specific like the corereader.  There is a separate problem in that sometimes
			// on a GPF we get a 'generic' signal which would need some tricky mapping.  To simplify this situation
			// we have chosen to merge the signal names for Windows and Unix and have a single routine to do 
			// the conversion.
			int num = getSignalNumber();
			return resolvePlatformName(num);
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageProcess#getPointerSize()
	 */
	public int getPointerSize()
	{
		return _pointerSize;
	}

	public void addRuntime(JavaRuntime vm)
	{
		_runtimes.add(vm);
	}
	
	public void setFaultingThreadID(long nativeID)
	{
		_faultingNativeID = nativeID;
	}

	/**
	 * TODO:  REMOVE THIS once there is a reliable way to find signal numbers from ImageThreads on all platforms!
	 * 
	 * @param signalNumber
	 */
	public void setSignalNumber(int signalNumber)
	{
		_signalNumber = signalNumber;
	}

	/**
	 * Called if the early extraction of the data from the meta-data and core file failed in some unrecoverable way.  If this is called it
	 * essentially means that we are stuck in native Image data and can't describe anything from the Java side.
	 * 
	 * @param e The exception which caused the failure
	 */
	public void runtimeExtractionFailed(Exception e)
	{
		_runtimeCheckFailure = e;
	}

	/* Used (at least) by derivative class PartialProcess */
	protected void setThreads(Iterator threads) {
		_threads.clear();
		while (threads.hasNext()) {
			_threads.add(threads.next());
		}
	}

	/* Also exposed for use by derivative class */
	protected void setCurrentThread(ImageThread thread) {
		_currentThread = thread;
	}

	//currently returns no OS specific properties
	public Properties getProperties() {
		return new Properties();
	}
}
