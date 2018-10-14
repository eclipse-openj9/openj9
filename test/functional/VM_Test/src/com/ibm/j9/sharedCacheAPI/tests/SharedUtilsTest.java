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
package com.ibm.j9.sharedCacheAPI.tests;

import com.ibm.oti.shared.*;
import java.util.List;
import java.lang.reflect.Field;

public class SharedUtilsTest 
{
    private String cacheName = "ShareClassesUtilities";
    private int TEMP_JAVA10_SHARED_CACHE_MODLEVEL = 6;
    
    public static void main(String args[]) {
	SharedUtilsTest obj = new SharedUtilsTest();
	obj.testSharedCacheAPI();
    }
    
    public String helpSharedCacheAPI()
    {
	return "Dummy class to get the agent started";
    }
    
    public boolean testSharedCacheAPI()
    {
	List<SharedClassCacheInfo> cacheList = null;
	int rc, cacheType;
	try {
	    cacheList = SharedClassUtilities.getSharedCacheInfo(null, SharedClassUtilities.NO_FLAGS, false);
	    if (cacheList == null) {
	    	System.out.println("iterateSharedCache failed");
	    	return false;
	    } else {
	    	boolean found = false;
	    	for(SharedClassCacheInfo cacheInfo : cacheList) {
	    		if (cacheName.equals(cacheInfo.getCacheName())) {
	    			found = true;
	    		}
				if (isJvmLevelValid(cacheInfo) == false)
				{
					System.out.println("iterateSharedCache failed (jvm level is unknown: " + cacheInfo.getCacheJVMLevel() + ")");
				    return false;
				}
	    	}
	    	if (found == true) {
	    		System.out.println("iterateSharedCache passed");
	    	} else {
	    		System.out.println("iterateSharedCache failed");
	    		return false;
	    	}
	    }
	} catch (IllegalStateException e) {
	    System.out.println("IllegalStateException by getSharedCacheInfo");
	}

	try {
	    rc = SharedClassUtilities.destroySharedCache(null, SharedClassUtilities.PERSISTENCE_DEFAULT, cacheName, false);
	    if (rc != SharedClassUtilities.DESTROYED_ALL_CACHE) {
		System.out.println("destroySharedCache failed");
		return false;
	    } else {
		System.out.println("destroySharedCache passed");
	    }
	} catch (IllegalStateException e) {
	    System.out.println("IllegalStateException by destroySharedCache");
	}
	return true;
    }
    
	/**
	 * Check if the JVM level is valid.
	 *
	 * @return		true if the level is valid, false otherwise. 
	 */		
	public boolean isJvmLevelValid(SharedClassCacheInfo obj)
	{
		int min = Integer.MAX_VALUE;
		int max = 0;
		boolean rc = false;
		try {
			Class oClass = obj.getClass();
			Field[] fields = oClass.getDeclaredFields();
			for (int i = 0; i < fields.length; i++) {
				String name = fields[i].getName();
				if ( name.startsWith("JVMLEVEL_JAVA") ) {
					Object field = fields[i].get(obj);
					int value = ((Integer)field).intValue(); 
					if (value > max) {
						max = value;							
					}
					if (value < min) {
						min = value;
					}
				}
			}
			int level = obj.getCacheJVMLevel();
			if (level >= min && level <= max) {
				rc = true;
			} else if (level >= 10 || TEMP_JAVA10_SHARED_CACHE_MODLEVEL == level) {
				/* mod level equals to java version number from Java 10. There might be an exiting java10 shared cache that has modLevel 6 created before this change */
				rc = true;
			}
		}
		catch (IllegalAccessException e) {
		    System.out.println("IllegalStateException by getSharedCacheInfo");
        }
		return rc;		
	}
}
