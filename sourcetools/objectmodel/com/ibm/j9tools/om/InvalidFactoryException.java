/*******************************************************************************
 * Copyright (c) 2007, 2011 IBM Corp. and others
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
package com.ibm.j9tools.om;

import java.util.Collection;

/**
 * @author	Gabriel Castro
 * @since	v1.5.0
 */
public class InvalidFactoryException extends OMObjectException {
	private static final long serialVersionUID = 1L;
	/** Identifier for serialized instances. */

	private ObjectFactory factory;

	/**
	 * @param 	causes
	 */
	public InvalidFactoryException(Collection<Throwable> causes, ObjectFactory factory) {
		super(causes, null);
		this.factory = factory;
	}

	/**
	 * @param factory
	 */
	public void setFactory(ObjectFactory factory) {
		this.factory = factory;
	}

	/**
	 * Retrieves the {@link ObjectFactory} associated with this exception.
	 * 
	 * @return	the object factory
	 */
	public ObjectFactory getFactory() {
		return factory;
	}

	/**
	 * @see java.lang.Throwable#getMessage()
	 */
	@Override
	public String getMessage() {
		StringBuilder sb = new StringBuilder(Messages.getString("InvalidFactoryException.0") + ":\n"); //$NON-NLS-1$//$NON-NLS-2$

		for (Throwable e : errors) {
			sb.append(" - "); //$NON-NLS-1$
			sb.append(e.getMessage());
		}

		return sb.toString();
	}

}
