/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview;

import java.net.URI;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.jvm.dtfjview.spi.ICombinedContext;
import com.ibm.jvm.dtfjview.spi.ISessionContextManager;

/**
 * Class for managing jdmpview contexts as it is possible to add and remove contexts
 * without exiting the application.
 * 
 * @author adam
 *
 */
public class JdmpviewContextManager implements ISessionContextManager {
	private Map<URI, ArrayList<ICombinedContext>> contextTracker = new LinkedHashMap<URI, ArrayList<ICombinedContext>>();
	private int maxContextID = 0;
	private boolean hasChanged = false;
	
	/**
	 * Create a new context from DTFJ.
	 * 
	 * @param image the DTFJ Image
	 * @param major the DTFJ API major number
	 * @param minor the DTFJ API minor number
	 * @param space DTFJ address space
	 * @param proc DTFJ process
	 * @param rt DTFJ JavaRuntime
	 * @return the newly created context
	 */
	public ICombinedContext createContext(final Image image, final int major, final int minor, final ImageAddressSpace space, final ImageProcess proc, final JavaRuntime rt) {
		ArrayList<ICombinedContext> existingContexts = contextTracker.get(image.getSource());		//get existing contexts for this source
		ICombinedContext combinedctx = new CombinedContext(major, minor, image, space, proc, rt, maxContextID);
		combinedctx.refresh();
		maxContextID++;
		if(existingContexts == null) {
			//this is the first context for this source
			existingContexts = new ArrayList<ICombinedContext>();
			contextTracker.put(image.getSource(), existingContexts);
		}
		existingContexts.add(combinedctx);
		hasChanged = true;
		return combinedctx;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISessionContextManager#removeContexts(com.ibm.dtfj.image.Image)
	 */
	public void removeContexts(Image image) {
		removeContexts(image.getSource());
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISessionContextManager#removeContexts(java.net.URI)
	 */
	public void removeContexts(URI source) {
		if(contextTracker.containsKey(source)) {
			for(ICombinedContext context : contextTracker.get(source)) {
				context.getImage().close();
			}
			contextTracker.remove(source);
			hasChanged = true;
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISessionContextManager#removeAllContexts()
	 */
	public void removeAllContexts() {
		ArrayList<URI> sources = new ArrayList<URI>();
		for(URI source : contextTracker.keySet()) {
			//build the list of sources to avoid a concurrent modification exception
			sources.add(source);
		}
		for(URI source : sources) {
			removeContexts(source);
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISessionContextManager#getContexts()
	 */
	public Map<URI, ArrayList<ICombinedContext>> getContexts() {
		return Collections.unmodifiableMap(contextTracker);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISessionContextManager#hasMultipleContexts()
	 */
	public boolean hasMultipleContexts() {
		switch(contextTracker.size()) {
			case 0:		//no contexts
				return false;
			case 1:		//only one source, so need to look at contexts
				return contextTracker.values().size() > 1;
				
			default : //two or more sources exist = multiple contexts
				return true;
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISessionContextManager#getContext(int)
	 */
	public ICombinedContext getContext(int id) {
		if((id < 0) || (id > maxContextID)) {
			return null;		//id is outside of bounds checking
		}
		for(ArrayList<ICombinedContext> contexts : contextTracker.values()) {
			for(ICombinedContext context : contexts) {
				if(context.getID() == id) {
					return context;
				}
			}
		}
		return null;		//couldn't find the context
	}
	
	//indicates if the context list has changed since the last time this method was called
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISessionContextManager#hasChanged()
	 */
	public boolean hasChanged() {
		boolean value = hasChanged;
		hasChanged = false;
		return value;
	}
}
