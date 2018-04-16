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

import java.util.*;

public class TestIterator {
	private TestSuite _suite;

	private String _loopIndex;
	private String _loopFrom;
	private String _loopUntil;
	private String _loopInc;
	
	private boolean _singleIterationOnly;
	private boolean _loopIndexIncremented;
	
	private boolean _isInnerLoopOpen;
	private ArrayList _subItems;
	
	/**
	 * This constructor creates a standard TestIterator object, which runs a given set of tests/variable assignments/etc.
	 * a number of times. The number of times that the stuff is run depends on the <code>from</code>, <code>until</code>,
	 * and <code>inc</code> parameters.
	 * The <code>index</code> variable is started at <code>from</code>, and incremented by <code>inc</code> after each
	 * iteration. Before each iteration is started, if <code>index</code> is equal to <code>until</code>, the run is
	 * terminated.
	 * The effect is the equivalent of <code>for (index = from; index != until; index += inc) { dostuff }</code>, where
	 * <code>dostuff</code> is determined by what is added to this TestIterator using the addXXX methods.
	 * The <code>from</code>, <code>until</code>, and <code>inc</code> strings may have variables in them, but after
	 * variable substitution, must evaluate to integers.
	 */
	TestIterator( TestSuite suite, String index, String from, String until, String inc ) {
		_suite = suite;

		_loopIndex = index;
		_loopFrom = from;
		_loopUntil = until;
		_loopInc = inc;
		
		_isInnerLoopOpen = false;
		_subItems = new ArrayList();
		
		TestSuite.putVariable( _loopIndex, TestSuite.evaluateVariables( _loopFrom ) );
	}
	
	/**
	 * This constructor creates a single-iteration version of this iterator. It will run everything passed
	 * to it (by the addXXX methods) exactly once.
	 */
	TestIterator( TestSuite suite ) {
		_suite = suite;
		
		_singleIterationOnly = true;
		
		_isInnerLoopOpen = false;
		_subItems = new ArrayList();
	}
	
	void addTest( Test t ) {
		if (_isInnerLoopOpen) {
			getLastSubIterator().addTest( t );
		} else {
			_subItems.add( t );
			if (canRunTest()) {
				_suite.runTest( t );
			}
		}
	}

	void addSubIterator( TestIterator it ) {
		if (_isInnerLoopOpen) {
			getLastSubIterator().addSubIterator( it );
		} else {
			_subItems.add( it );
			_isInnerLoopOpen = true;
		}
	}
	
	void addCommand( Command c ) {
		if (_isInnerLoopOpen) {
			getLastSubIterator().addCommand( c );
		} else {
			_subItems.add( c );
			c.executeSelf();
		}
	}
	
	boolean closeInnerLoop() {
		if (_isInnerLoopOpen) {
			if (getLastSubIterator().closeInnerLoop()) {
				return true;
			}
			_isInnerLoopOpen = false;
			return true;
		}
		// end of this TestIterator's corresponding <loop> element, so now we actually run the remaining
		// iterations of the loop
		incrementLoopIndex();
		while (canRunTest()) {
			runIteration();
		}
		return false;
	}
	
	private void incrementLoopIndex() {
		if (_singleIterationOnly) {
			_loopIndexIncremented = true;
			return;
		}
		
		try {
			int increment = (_loopInc == null ? 1 : Integer.parseInt( TestSuite.evaluateVariables( _loopInc ).trim() ));
			String value = TestSuite.getVariable( _loopIndex );
			value = Integer.toString( Integer.parseInt( value.trim() ) + increment );
			TestSuite.putVariable( _loopIndex, value );
		} catch (Exception e) {
			notifyInfiniteLoop(e);
			System.exit(1);
		}
	}
	
	private void runEntireLoop() {
		TestSuite.putVariable( _loopIndex, TestSuite.evaluateVariables( _loopFrom ) );
		while (canRunTest()) {
			runIteration();
		}
	}
	
	private void runIteration() {
		for (int i = 0; i < _subItems.size(); i++) {
			Object o = _subItems.get(i);
			if (o instanceof Test && canRunTest()) {
				_suite.runTest( (Test)o );
			} else if (o instanceof TestIterator) {
				( (TestIterator)o ).runEntireLoop();
			} else if (o instanceof Command) {
				( (Command)o ).executeSelf();
			}
		}
		incrementLoopIndex();
	}
	
	private boolean canRunTest() {
		if (_singleIterationOnly) {
			return !_loopIndexIncremented;
		}
		try {
			return ! (Integer.parseInt(TestSuite.getVariable(_loopIndex)) == Integer.parseInt(TestSuite.evaluateVariables(_loopUntil)));			
		} catch (Exception e) {
			notifyInfiniteLoop(e);
			System.exit(1);
		}
		return false;	// dead code, to satisfy compiler only
	}
	
	private TestIterator getLastSubIterator() {
		return (TestIterator)_subItems.get( _subItems.size() - 1 );
	}
	
	private void notifyInfiniteLoop( Exception e ) {
		// if there is an exception here, it's an accidental infinite loop. tell user to fix!
		System.err.println( "The test file passed to the cmdLineTester application contained an invalid loop" );
		System.err.println( "of " + _loopIndex + " from " + _loopFrom + " until " + _loopUntil + ", with increment " + _loopInc + ". This loop" );
		System.err.println( "contains an error that will get it stuck in an infinite cycle. Please fix it.. or" );
		System.err.println( "modify the cmdLineTester application to deal with it, if it really is what you" );
		System.err.println( "want to do. The cmdLineTester application will now terminate." );
		e.printStackTrace();
	}
}
