/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck;

import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;

class ScanFormatter
{
	protected static final int NUMBER_ELEMENTS_DISPLAYED_PER_LINE = 8;
	protected long _currentCount = 0;
	protected boolean _displayedData = false;
	protected CheckReporter _reporter;
	
	public ScanFormatter(Check check, String title, AbstractPointer pointer)
	{
		_reporter = check.getReporter();
		_reporter.println(String.format("<gc check: Start scan %s (%s)>", title, formatPointer(pointer)));
	}
	
	public ScanFormatter(Check check, String title)
	{
		_reporter = check.getReporter();
		_reporter.println(String.format("<gc check: Start scan %s>", title));
	}

	static String formatPointer(AbstractPointer pointer) 
	{
		if(J9BuildFlags.env_data64) {
			return String.format("%016X", pointer.getAddress());
		} else {
			return String.format("%08X", pointer.getAddress());
		}
	}
	
	public void section(String type)
	{
		_reporter.println(String.format("  <%s>", type));
		_currentCount = 0;
	}
	
	public void section(String type, AbstractPointer pointer)
	{
		_reporter.println(String.format("  <%s (%s)>", type, formatPointer(pointer)));
		_currentCount = 0;
	}
	
	public void endSection()
	{
		if((0 != _currentCount) && _displayedData) {
			_reporter.println(">");
			_currentCount = 0;
		}
	}
	
	public void entry(AbstractPointer pointer)
	{
		if(0 == _currentCount) {
			_reporter.print("    <");
			_displayedData = true;
		}
		_reporter.print(formatPointer(pointer) + " ");
		
		_currentCount += 1;
		
		if(NUMBER_ELEMENTS_DISPLAYED_PER_LINE == _currentCount) {
			_reporter.println(">");
			_currentCount = 0;
		}
	}
	
	public void end(String type, AbstractPointer pointer)
	{
		if((0 != _currentCount) && _displayedData) {
			_reporter.println(">");
		}
		_reporter.println(String.format("<gc check: End scan %s (%s)>", type, formatPointer(pointer)));
	}
	
	public void end(String type)
	{
		if((0 != _currentCount) && _displayedData) {
			_reporter.println(">");
		}
		_reporter.println(String.format("<gc check: End scan %s>", type));		
	}
}
