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
package CustomClassloaders;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.ByteBuffer;
import java.io.ByteArrayOutputStream;

import com.ibm.oti.shared.Shared;
import com.ibm.oti.shared.SharedDataHelper;
import com.ibm.oti.shared.SharedDataHelperFactory;

/**
 * Uses the shared cache to store some resource data.  In this implementation the getResourceAsStream() method 
 * is wired to use the shared cache.
 */
public class DataCachingClassLoader extends URLClassLoader {

	private static final boolean debug = true; // ==true means logging messages come out as this loader executes
	
	// Classloader users can check the counters, reset them with 'resetCounters()'
	public int cacheStatsFindCount; /* Number of times we have called findSharedData() */
	public int cacheStatsStoreCount; /* Number of times we have called storeSharedData() */
	public int cacheStatsCacheHitCount; /* Number of times we have successfully found the data in the cache on findSharedData() */
	public int cacheStatsCacheMissCount; /* Number of times we have not found the data in the cache on findSharedData() */
	
	// Control whether the cache should be used
	public boolean storeInCache = true;
	public boolean findInCache  = true;
	
	private SharedDataHelper sdHelper;
	
	// ---
	
	public DataCachingClassLoader(URL[] urls, ClassLoader parent) {
		super(urls, parent);
		SharedDataHelperFactory helperFactory = Shared.getSharedDataHelperFactory();
		if (helperFactory==null) throw new RuntimeException("No SharedDataHelperFactory found, are you running -Xshareclasses?");
		sdHelper = (helperFactory==null?null:helperFactory.getDataHelper(this));
		resetCounters();
	}

	@Override
	public InputStream getResourceAsStream(String name) {
		if (findInCache && sdHelper!=null) {
			ByteBuffer bBuffer = sdHelper.findSharedData(name);
			if (bBuffer!=null) {
				log("Returning an entry found in the cache");
				return newInputStream(bBuffer);
			}
		}
		InputStream realResource = super.getResourceAsStream(name);
		if (realResource!=null && storeInCache && sdHelper!=null) {
			log("Caching the data with token '"+name+"'");
			BufferedInputStream bis = new BufferedInputStream(realResource);
			ByteArrayOutputStream baos = new ByteArrayOutputStream();
			byte[] data = new byte[256];
			int readBytes = -1;
			try {
				while ( (readBytes=bis.read(data))!=-1) {
					baos.write(data,0,readBytes);
				}
			} catch (IOException e) {
				e.printStackTrace();
			}
			byte[] dataBytes = baos.toByteArray();
			ByteBuffer bBuffer = ByteBuffer.allocateDirect(dataBytes.length);
			bBuffer.put(dataBytes);
			bBuffer = sdHelper.storeSharedData(name, bBuffer);
			if (bBuffer==null) throw new RuntimeException("storeSharedData('"+name+"',...) has failed!");
			return newInputStream(bBuffer);
		} else {
			return realResource;
		}
		
	}


	/** Return an InputStream that wraps a ByteBuffer */
	public static InputStream newInputStream(final ByteBuffer buf) {
        return new InputStream() {
            public synchronized int read() throws IOException {
                if (!buf.hasRemaining()) {
                    return -1;
                }
                return buf.get();
            }
    
            public synchronized int read(byte[] bytes, int off, int len) throws IOException {
            	if (!buf.hasRemaining()) {
                    return -1;
                }
            	// Read only what's left
                len = Math.min(len, buf.remaining());
                buf.get(bytes, off, len);
                return len;
            }
        };
    }

	public void resetCounters() {
		cacheStatsCacheHitCount = 0;
		cacheStatsCacheMissCount = 0;
		cacheStatsFindCount = 0;
		cacheStatsStoreCount = 0;
	}

	// -- operations that work directly on the cache
	
	public InputStream findInCache(String token) {
		if (sdHelper!=null) {
			ByteBuffer bBuffer = sdHelper.findSharedData(token);
			if (bBuffer!=null) return newInputStream(bBuffer);
		}
		return null;
	}
	
	public void markStale(String token) {
		if (sdHelper!=null) {
			sdHelper.storeSharedData(token, null);
		}
	}
	
	// --- helpers
	private void log(String msg) {
		if (debug) System.out.println("DataCachingClassLoader: "+msg);
	}

	public void storeNull(String token) {
		if (sdHelper!=null) {
			sdHelper.storeSharedData(token, null);
		}		
	}
	
	public boolean forceStore(String token,String data) {
		byte[] dataBytes = data.getBytes();
		ByteBuffer bBuffer = ByteBuffer.allocateDirect(dataBytes.length);
		bBuffer.put(dataBytes);
		bBuffer = sdHelper.storeSharedData(token, bBuffer);
		return (bBuffer!=null);
	}
}
