/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package j9vm.test.classunloading;

import java.io.IOException;

/**
 *	A classloader which fails to find a class unless it has a particular name.
 *
 *		Named instances of SelectiveJarClassLoader report their own finalization to 
 *	the FinalizationIndicator class, which may be querried to determine whether any
 *	given instance has been finalized.
 **/
public class FriendlyJarClassLoader extends SimpleJarClassLoader {
	String allowedClassNames[];
	FriendlyJarClassLoader friend;
	
	
public FriendlyJarClassLoader(String name, String jarFileName, ClassLoader parent, String[] allowedClassNames) 
	throws IOException
{
	super(name, jarFileName, parent);
	this.allowedClassNames = allowedClassNames;
}
public FriendlyJarClassLoader(String name, String jarFileName, String[] allowedClassNames) 
	throws IOException
{
	super(name, jarFileName);
	this.allowedClassNames = allowedClassNames;
}

public void setFriend(FriendlyJarClassLoader friend) {
	this.friend = friend;
}

protected Class findClass(String name)
   	throws ClassNotFoundException
{
	for (int i = 0; i < allowedClassNames.length; i++) {
		if(allowedClassNames[i].equals(name) || isInnerClass(allowedClassNames[i],name)) {
			return super.findClass(name);
		}
	}
	if (friend != null) {
		String[] friendClassNames = friend.getAllowedClassNames();
		for (int i = 0; i < friendClassNames.length; i++) {
			if(friendClassNames[i].equals(name) || isInnerClass(allowedClassNames[i],name)) {
				return friend.findClass(name);
			}
		}
	}
	throw new ClassNotFoundException(name);
}



protected String[] getAllowedClassNames() {
	return allowedClassNames;
}

protected boolean isInnerClass(String allowedClassName, String checkClassName)
{
	return  checkClassName.startsWith(allowedClassName + "$");
}

}
