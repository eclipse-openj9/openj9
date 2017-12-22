/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.view.dtfj.java;

import static com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils.corruptIterator;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaVMInitArgs;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMInitArgsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.JavaVMOptionPointer;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

@SuppressWarnings("rawtypes")
public class DTFJJavaVMInitArgs implements JavaVMInitArgs {
	private boolean isCachePopulated = false;
	private LinkedList<Object> options = new LinkedList<Object>();
	private Iterator corruptCache = null;
	private int version = 0;
	private boolean isCorrupt = false;
	private CorruptData cdata = null;
	private Logger log = DTFJContext.getLogger();
	private boolean ignoreFlag = false;

	public boolean getIgnoreUnrecognized() throws DataUnavailable, CorruptDataException {
		loadData();
		if(isCorrupt) {
			throw new CorruptDataException(cdata);
		}
		return ignoreFlag;
	}

	public Iterator getOptions() throws DataUnavailable {
		loadData();
		if(isCorrupt) {
			throw new DataUnavailable("The VM options are not available : " + cdata.toString());
		}
		if(corruptCache == null) {
			return options.iterator();
		} else {
			return corruptCache;
		}
	}

	public int getVersion() throws DataUnavailable, CorruptDataException {
		loadData();
		if(isCorrupt) {
			throw new CorruptDataException(cdata);
		}
		return version;
	}

	private void loadData() {
		if(isCachePopulated || isCorrupt)		//cache has been populated
			return;
		
		//obtain the vm init args, logic copied from javadump.cpp so dtfj output
		//matches javacore output.
		J9VMInitArgsPointer args = null;
		try {
			args = DTFJContext.getVm().vmArgsArray();
			ignoreFlag = !args.actualVMArgs().ignoreUnrecognized().eq(0);
			version = args.actualVMArgs().version().intValue();
		} catch (Throwable t) {
			isCorrupt = true;
			cdata = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
		}

		if (isCorrupt) { 			//structure is fundamentally unusable
			isCachePopulated = true;	//prevent continual reloading
			return;
		}
		
		//now see if we can extract the options
		try {
			int optionCount = args.nOptions().intValue();
			JavaVMOptionPointer option = args.actualVMArgs().options();
			for(int i = 0; i < optionCount; i++) {
				try {
					DTFJJavaVMOption vmoption = new DTFJJavaVMOption(option);
					options.add(vmoption);
					if(log.isLoggable(Level.FINE)) {
						try {
							log.fine(String.format("Found VM option %s", vmoption.getOptionString()));
						} catch (Exception e) {
							log.warning(e.getMessage());
						} 
					}
					option = option.add(1);
				} catch (Throwable t) {
					CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
					options.add(cd);
				}
			}	
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			corruptCache = corruptIterator(cd);
		}
		isCachePopulated = true;	//prevent continual reloading
	}
	
}
