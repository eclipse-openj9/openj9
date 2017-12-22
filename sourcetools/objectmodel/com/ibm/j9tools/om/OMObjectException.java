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
import java.util.HashSet;
import java.util.Set;

/**
 * Object Model exception used to encapsulate errors found in an OMObject.
 * 
 * @author	Gabriel Castro
 * @since	v1.5.0
 */
public abstract class OMObjectException extends OMException {
	private static final long serialVersionUID = 1L;	/** Identifier for serialized instances. */

	protected Set<Throwable> errors = new HashSet<Throwable>();
	protected Set<Throwable> warnings = new HashSet<Throwable>();
	
	protected String objectId;

	public OMObjectException(Throwable cause, String objectId) {
		super();

		this.objectId = objectId;
		this.errors.add(cause);
	}

	public OMObjectException(Collection<Throwable> errors, Collection<Throwable> warnings, String objectId) {
		super();

		this.objectId = objectId;
		this.errors.addAll(errors);
		this.warnings.addAll(warnings);
	}

	public OMObjectException(Collection<Throwable> causes, OMObject object) {
		super();

		this.object = object;
		this.errors.addAll(causes);
	}

	/**
	 * Retrieves the errors associated with this exception.
	 * 
	 * @return	the errors associated with this exception
	 */
	public Collection<Throwable> getErrors() {
		return errors;
	}

	/**
	 * Retrieves the warnings associated with this exception.
	 * 
	 * @return	the warnings associated with this exception
	 */
	public Collection<Throwable> getWarnings() {
		return warnings;
	}

	public String getObjectId() {
		return objectId;
	}

	public void setObjectId(String objectId) {
		this.objectId = objectId;
	}
	
	

}
