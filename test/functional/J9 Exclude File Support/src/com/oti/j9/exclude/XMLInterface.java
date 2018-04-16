/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.oti.j9.exclude;

import java.io.*;
import java.util.*;
import java.util.regex.Pattern;

public class XMLInterface implements IXMLDocumentHandler {

	private String      _fURI;
	private ExcludeList _fExcludeList;
	private String      _fTestID;
	private String      _fLastCDATA;
	private long        _fElapsedTime;
	private boolean     _fSuccess;
	private String[]    _fApplicableTests;
	private long		 _fExpiry;

	// set expiry limit in number of days
	private static long _fExpiryLimit = System.currentTimeMillis() + (86400000L * 60);

	public ExcludeList  getExcludeList() {return _fExcludeList;}

	// Progress Notification Callbacks
	public void xmlStartDocument()
	{
		System.out.println("Parsing exclude list...");
		for (int i=0; i < _fApplicableTests.length; i++) {
			System.out.println("  add excludes for [" + _fApplicableTests[i] + "]");
		}
	}

	public void xmlEndDocument()
	{
		_fSuccess = true;
		System.out.println("DONE in " + _fElapsedTime + " ms.");
	}


	public void xmlStartElement(String name, Hashtable attr)
	{
		if (name.equals("suite")) {
			_fExcludeList = new ExcludeList( (String) attr.get("id") );
			return;
		}

		if (name.equals("exclude") || name.equals("include")) {
			String allPlatforms = (String) attr.get("platform");
			String[] platforms = ExcludeList.substrings(allPlatforms, ' ');
			boolean matched = false;

			for (int i=0; i < _fApplicableTests.length; i++) {
				String logicalType = _fApplicableTests[i];
				for (int j=0; j < platforms.length; j++) {
					String aPlatform = platforms[j];
					if (Pattern.matches(aPlatform, logicalType)) {
						matched = true;
						if (name.equals("exclude")) {
							_fTestID = (String) attr.get("id");
							_fExpiry = 0;
							String expiryString = (String) attr.get("expiry");
							if (expiryString != null) {
								try {
									_fExpiry = DateUtil.parseExpiry(expiryString);
								} catch (IllegalArgumentException e) {
									System.out.println("  invalid expiry date (" + expiryString + ") for: " + _fTestID);
									break;
								}
								if (_fExpiry > _fExpiryLimit) {
									System.out.println("  expiry (" + DateUtil.printDate(_fExpiry) + ") greater than expiry limit (" + DateUtil.printDate(_fExpiryLimit) + "): " + _fTestID);
									break;
								}
							}
							return;
						}
					}
				}
			}

			if (!matched && name.equals("include")) {
				_fTestID = (String) attr.get("id");
				_fExpiry = 0;
				return;
			}

			_fTestID = null;  // ignore this one
			_fExpiry = 0;
			return;
		}

	}


	public void xmlEndElement(String name)
	{
		if (name.equals("reason")) {
			if (_fTestID == null)
				return;
			_fExcludeList.addExclude( new Exclude( _fTestID, _fLastCDATA, _fExpiry) );
		}
	}

	public void xmlCharacters(String charData)
	{
		_fLastCDATA = charData;
	}



	public boolean parse(InputStream uriStream, String[] applicableTests)
	{
		_fSuccess = false;

		try {
			_fApplicableTests = applicableTests;

			long before = System.currentTimeMillis();
			new XMLParser().parse(uriStream, this);
			long after = System.currentTimeMillis();
			_fElapsedTime = after - before;
		}
		catch (Exception genericException)
		{
			genericException.printStackTrace();
        	}

        	return _fSuccess;
	}
}
