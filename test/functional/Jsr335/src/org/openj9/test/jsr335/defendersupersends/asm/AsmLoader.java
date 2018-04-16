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
package org.openj9.test.jsr335.defendersupersends.asm;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * Classloader that caches classes, allows us to use AsmLoader, and calls to parent classloader
 *
 */
public class AsmLoader extends ClassLoader {
	private final ClassLoader parent;
	private final HashMap<String, Class<?>> map = new HashMap<String, Class<?>>();
	private final HashMap<String, AsmGenerator> asmGenerators = new HashMap<String, AsmGenerator>();
	
	/**
	 * Create a new shimloader with parent as the parent classloader
	 * @param parent parent classloader, if null, ShimLoader's classloader will be used
	 */
	public AsmLoader(ClassLoader parent) {
		if (parent == null) {
			parent = AsmLoader.class.getClassLoader();
		}
		this.parent = parent;
	}
	
	/**
	 * Add an AsmLoader to the list of loaders
	 * @param generator
	 */
	public void addAsmGenerator(AsmGenerator generator) {
		synchronized (asmGenerators) {
			synchronized (map) {
				// Already loaded and defined
				if (map.containsKey(generator.name())) {
					return;
				}
			}
			asmGenerators.put(generator.name(), generator);
		}
	}
	
	private Class<?> asmLoad(String name) {
		synchronized (asmGenerators) {
			if (asmGenerators.containsKey(name)) {
				AsmGenerator loader = asmGenerators.get(name);
				byte b[] = loader.getClassData();
				Class<?> c = defineClass(name, b, 0, b.length);
				synchronized (map) {
					map.put(name, c);
				}
				return c;
			}
		}
		return null;
	}
	
	public Class<?> findClass(String name) throws ClassNotFoundException{
		Class<?> c = null;
		synchronized (map) {
			c = map.get(name);
			if (c != null) {
				return c;
			}
		}
		
		c = asmLoad(name);
		if (c != null) {
			return c;
		}
		
		return parent.loadClass(name);
	}

	public void dumpAsmClasses(String destination) throws IOException {
		File f = new File(destination);
		
		if (!f.exists()) {
			f.mkdirs();
		} else if (f.isFile()) {
			throw new IOException("Destination directory MUST NOT be a file");
		}
		
		Iterator<Map.Entry<String, AsmGenerator>> it = asmGenerators.entrySet().iterator();
	    while (it.hasNext()) {
	        Map.Entry<String, AsmGenerator> pairs = it.next();
	        
	        String fileName = f.getAbsolutePath()+"/"+pairs.getKey().replace('.', '/') + ".class";
	        File fout = new File(fileName);
	        String parent = fout.getParent();
	        File fpar = new File(parent);
	        if (!fpar.exists()) {
	        	fpar.mkdirs();
	        }
	        byte d[] = pairs.getValue().getClassData();
	        FileOutputStream fos = new FileOutputStream(fout);
	        fos.write(d);
	        fos.close();
	    }
	}
}
