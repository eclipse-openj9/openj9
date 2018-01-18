/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;
import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.util.PatternString;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.j9.walkers.FilteredROMClassesIterator;
import com.ibm.j9ddr.vm29.j9.walkers.J9ROMClassAndMethod;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMNameAndSignaturePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;

public class FilteredROMMethodsIterator implements Iterator<J9ROMClassAndMethod>, Iterable<J9ROMClassAndMethod> {
	

	J9ROMClassAndMethod nextMethod = null;
	J9ROMClassPointer currentRomClass = null;
	final FilteredROMClassesIterator classIterator;
	private  PatternString classPattern = null;
	private  PatternString methodPattern;
	private  PatternString signaturePattern;

	private J9ROMMethodPointer currentRomMethod;
	private int remainingMethods = 0;

	public FilteredROMMethodsIterator(PrintStream out, Context context, String methodPattern) throws CorruptDataException {
		setPatterns(methodPattern);
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		if (null == vm) {
			throw new CorruptDataException("Unable to find the VM in core dump!");
		}
		classIterator = new FilteredROMClassesIterator(out, vm.classMemorySegments(), classPattern);
		resetMethodIterationState();
	}

	private void resetMethodIterationState() {
		currentRomClass = null;
		currentRomMethod = null;
		remainingMethods = 0;
	}

	private void setPatterns(String searchExpression) {
		String classNameString = "*";
		String methodNameString= "*";
		String signatureString = "*";

		int parenLocation = searchExpression.indexOf('(');
		final int periodLocation = searchExpression.indexOf('.');
		if (periodLocation != -1) {
			int endOfClassName = (parenLocation >= 0) ? parenLocation: searchExpression.length();
			classNameString = searchExpression.substring(0, periodLocation);
			methodNameString = searchExpression.substring(periodLocation + 1, endOfClassName);
		} else {
			methodNameString = searchExpression;
		}

		if (parenLocation != -1) {
			signatureString = searchExpression.substring(parenLocation);
		}

		classPattern = new PatternString(classNameString);
		methodPattern = new PatternString(methodNameString);
		signaturePattern = new PatternString(signatureString);
	}

	public boolean hasNext() {
		if (null == nextMethod) {
			nextMethod = next();
		}
		return null != nextMethod;
	}

	public J9ROMClassAndMethod next() {
		J9ROMClassAndMethod result = nextMethod; /* hasNext may have already go the next method */
		nextMethod = null; /* destructive read */
		if (null == result) {
			do {
				try {
					if ((null != currentRomMethod) && (remainingMethods > 0)) {
						J9ROMMethodPointer romMethod = currentRomMethod;
						J9ROMNameAndSignaturePointer nameAndSignature = romMethod.nameAndSignature();
						J9UTF8Pointer methNameUTF = nameAndSignature.name();
						String methName = J9UTF8Helper.stringValue(methNameUTF);
						if (methodPattern.isMatch(methName)) {
							String methSig = J9UTF8Helper.stringValue(nameAndSignature.signature());
							if (signaturePattern.isMatch(methSig)) {
								result = new J9ROMClassAndMethod(romMethod, currentRomClass);
							}
						}
						currentRomMethod = ROMHelp.nextROMMethod(currentRomMethod);
						--remainingMethods;
					} else { 
						if (!goToNextClass()) {
							break; /* ran out of classes */
						}
					}			
				} catch (CorruptDataException e) {
					throw new NoSuchElementException();
				}
			} while (null == result);
		}
		return result;
	}

	private boolean goToNextClass() throws CorruptDataException {
		resetMethodIterationState();
		boolean classAvailable = false;
		while (null == currentRomMethod) {
			if (classIterator.hasNext()) {
				J9ROMClassPointer tempRomClass = classIterator.next();
				String className = J9UTF8Helper.stringValue(tempRomClass.className());
				if (classPattern.isMatch(className)) {
					if (tempRomClass.romMethodCount().longValue() > 0) {
						currentRomClass = tempRomClass;
						currentRomMethod = currentRomClass.romMethods();
						remainingMethods = currentRomClass.romMethodCount().intValue();
						classAvailable = true;
					}
				}
			} else {
				break; /* no more classes or classloaders */
			}
		}
		return classAvailable;
	}

	public Iterator<J9ROMClassAndMethod> iterator() {
		return this;
	}

	public void remove() {
		return;
	}
}
