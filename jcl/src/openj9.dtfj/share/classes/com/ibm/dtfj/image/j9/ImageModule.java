/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.j9;

import java.util.Iterator;
import java.util.Properties;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;

/**
 * @author jmdisher
 *
 */
public class ImageModule implements com.ibm.dtfj.image.ImageModule
{
	private String _name;
	private Vector _sections = new Vector();
	private Vector _symbols = new Vector();
	private Properties _properties;
	private long _loadAddress;

	public ImageModule(String moduleName, Properties properties, Iterator sections, Iterator symbols, long loadAddress) {
		_name = moduleName;
		while (sections.hasNext()) {
			_sections.add(sections.next());
		}
		while (symbols.hasNext()) {
			_symbols.add(symbols.next());
		}
		_properties = properties;
		
		_loadAddress=loadAddress;

		
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageModule#getName()
	 */
	public String getName() throws CorruptDataException
	{
		return _name;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageModule#getSections()
	 */
	public Iterator getSections()
	{
		return _sections.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageModule#getSymbols()
	 */
	public Iterator getSymbols()
	{
		return _symbols.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageModule#getProperties()
	 */
	public Properties getProperties() throws CorruptDataException
	{
		if (null == _properties) {
			//usually, this is because of DataUnavailable and not CorruptData but they both mean similar things
			throw new CorruptDataException(new CorruptData("no properties available for module " + getName(), null));
		}
		return _properties;
	}

	public long getLoadAddress() throws DataUnavailable {
		if (_loadAddress!=0) {
			return _loadAddress;
		} else {
			String moduleName;
			try {
				moduleName=getName();
			} catch (CorruptDataException e) {
				moduleName="<module name not available>";
			}
			throw new DataUnavailable("Load address not available for module: "+ moduleName);
		}
	}
}
