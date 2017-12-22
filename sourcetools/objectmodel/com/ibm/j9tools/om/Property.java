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

/**
 * A container classes used to keep track of build spec properties.
 * 
 * @author 	Maciek Klimkowski
 * @author	Gabriel Castro
 */
public class Property extends OMObject implements Comparable<Property> {
	private String _name;
	private String _value;

	/**
	 * Creates a property with the given name and value.
	 * 
	 * @param 	name		the property name
	 * @param 	value		the property value
	 */
	public Property(String name, String value) {
		this._name = name;
		this._value = value;
	}

	/**
	 * Retrieves the name of this property.
	 * 
	 * @return	the name
	 */
	public String getName() {
		return _name;
	}
	
	/**
	 * Sets the name of this property.
	 * 
	 * @param 	name	the name
	 */
	public void setName(String name) {
		_name = name;
	}

	/**
	 * Retrieves this property's value.
	 * 
	 * @return	the value
	 */
	public String getValue() {
		return _value;
	}
	
	/**
	 * Sets this property's value.
	 * 
	 * @param 	value	the new value
	 */
	public void setValue(String value) {
		_value = value;
	}

	/**
	 * @see java.lang.Comparable#compareTo(java.lang.Object)
	 */
	public int compareTo(Property arg0) {
		return _name.compareToIgnoreCase(arg0.getName());
	}

}
