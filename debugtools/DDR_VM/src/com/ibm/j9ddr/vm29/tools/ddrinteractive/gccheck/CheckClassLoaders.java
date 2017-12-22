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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;


import static com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck.CheckBase.*;
import static com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck.ScanFormatter.formatPointer;

class CheckClassLoaders extends Check
{
	@Override
	public void check()
	{
		try {
			GCClassLoaderIterator classLoaderIterator = GCClassLoaderIterator.from();
			while(classLoaderIterator.hasNext()) {
				J9ClassLoaderPointer classLoader = classLoaderIterator.next();
				if(!classLoader.gcFlags().allBitsIn(J9_GC_CLASS_LOADER_DEAD)) {
					PointerPointer slot = classLoader.classLoaderObjectEA();
					if(_engine.checkSlotPool(slot, VoidPointer.cast(classLoader)) != J9MODRON_SLOT_ITERATOR_OK){
						return;
					}
				}
			}
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

	@Override
	public String getCheckName()
	{
		return "CLASS LOADERS";
	}

	@Override
	public void print()
	{
		try {
			GCClassLoaderIterator classLoaderIterator = GCClassLoaderIterator.from();
			getReporter().println(String.format("<gc check: Start scan classLoaderBlocks (%s)>", formatPointer(getJavaVM().classLoaderBlocks())));
			while(classLoaderIterator.hasNext()) {
				J9ClassLoaderPointer classLoader = classLoaderIterator.next();
				getReporter().println(String.format("  <classLoader (%s)>", formatPointer(classLoader)));
				getReporter().println(String.format("    <flags=%d, classLoaderObject=%s>", classLoader.gcFlags().longValue(), formatPointer(classLoader.classLoaderObject())));
			}
			getReporter().println(String.format("<gc check: End scan classLoaderBlocks (%s)>", formatPointer(getJavaVM().classLoaderBlocks())));
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

}
