/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.spi;

import java.net.URI;
import java.util.ArrayList;
import java.util.Map;

import com.ibm.dtfj.image.Image;

/**
 * Controls the creation and deletion of contexts within a session.
 * 
 * @author adam
 *
 */
public interface ISessionContextManager {

	/**
	 * Remove all contexts which have been derived from an image source.
	 * e.g. if there are multiple JVMs in a single core then closing the core
	 * file would result in all images derived from that file also being closed.
	 * 
	 * @param image the source image from which derived contexts should be closed.
	 */
	public void removeContexts(Image image);

	/**
	 * Remove all contexts which have been defined as coming from a specified URI
	 * (note that the URI may or may not be a file URI).
	 *  
	 * @param URI the source URI from which derived contexts should be closed.
	 */
	public void removeContexts(URI source);

	/**
	 * Close and remove all contexts from this manager.
	 */
	public void removeAllContexts();

	/**
	 * Lists all contexts keyed by the URI from which they were derived.
	 * 
	 * @return map of URI's to contexts
	 */
	public Map<URI, ArrayList<ICombinedContext>> getContexts();

	/**
	 * Convenience method for determining if more than one context is currently open. It
	 * is a less expensive call than getContexts()
	 * 
	 * @return true if more than one context is currently open and available
	 */
	public boolean hasMultipleContexts();

	/**
	 * Gets the context with the specified ID.
	 * 
	 * @param id the context ID
	 * @return the located context or null if it was not found
	 */
	public ICombinedContext getContext(int id);

	/**
	 * A number of internal operations which could affect the list of currently open
	 * and available contexts happen in an unlinked or asynchronous manner. By calling this
	 * method the session (or external clients) are able to tell if the list of contexts has changed
	 * and anything related to this such as a display may need to be update.
	 * @return
	 */
	public boolean hasChanged();

}
