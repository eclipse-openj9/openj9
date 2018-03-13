/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
package java.lang.invoke;

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.Map;
import java.util.WeakHashMap;

/*
 * ClassValue based Cache for mapping from a Class to its perClassCache.
 */
final class Cache extends ClassValue<Map<CacheKey, WeakReference<MethodHandle>>> {
	@Override
	protected Map<CacheKey, WeakReference<MethodHandle>> computeValue(Class<?> arg0) {
		return Collections.synchronizedMap(new WeakHashMap<CacheKey, WeakReference<MethodHandle>>());
	}	
}

/* Cache key for mapping the methodName and MethodType to the actual MethodHandle */
final class MethodCacheKey extends CacheKey {
	private final MethodType type;
	private final Class<?> specialCaller;
	
	public MethodCacheKey(String name, MethodType mt, Class<?> specialCaller) {
		super(name, calculateHashcode(name, mt, specialCaller));
		
		this.type = mt;
		this.specialCaller = specialCaller;		
	}
	
	private static int calculateHashcode(String name, MethodType mt, Class<?> specialCaller) {
		/* Hash code based off MethodType.hashCode() */
		int hash = 31 + mt.hashCode();
		hash = 31 * hash + name.hashCode();
		
		if (specialCaller != null) {
			hash = 31 * hash + specialCaller.hashCode();
		}
		return hash;
	}

	@Override
	public boolean equals(Object o) {
		if ((o == null) || !(o instanceof MethodCacheKey)) {
			return false;
		}
		MethodCacheKey other = (MethodCacheKey) o;
		if ((other.type == type) && other.name.equals(name) && (other.specialCaller == this.specialCaller)) {
			return true;
		}
		return false;
	}
}

final class FieldCacheKey extends CacheKey {
	private final Class<?> fieldType;
	
	public FieldCacheKey(String fieldName, Class<?> fieldType) {
		super(fieldName, calculateHashcode(fieldName, fieldType));
		this.fieldType = fieldType;
	}
	
	private static int calculateHashcode(String fieldName, Class<?> fieldType) {
		/* Hash code based off MethodType.hashCode() */
		int hash = 31 + fieldType.hashCode();
		hash = 31 * hash + fieldName.hashCode();
		return hash;
	}
	
	
	@Override
	public boolean equals(Object o){
		if ((o == null) || !(o instanceof FieldCacheKey)) {
			return false;
		}
		FieldCacheKey other = (FieldCacheKey) o;
		if (other.fieldType == fieldType && other.name.equals(name)) {
			return true;
		}
		return false;
	}
}

/* The handle cache itself - it holds onto the individual caches for:
 * findVirtual
 * findStatic
 * findSpecial
 * findConstructor
 */
final class HandleCache {
	private static final Cache findVirtualCache = new Cache();
	private static final Cache findStaticCache = new Cache();
	private static final Cache findSpecialCache = new Cache();
	private static final Cache findConstructorCache = new Cache();
	private static final Cache staticFieldSetterCache = new Cache();
	private static final Cache staticFieldGetterCache = new Cache();
	private static final Cache fieldSetterCache = new Cache();
	private static final Cache fieldGetterCache = new Cache();

	static Map<CacheKey, WeakReference<MethodHandle>> getVirtualCache(Class<?> c) {
		return findVirtualCache.get(c);
	}
	static Map<CacheKey, WeakReference<MethodHandle>> getStaticCache(Class<?> c) {
		return findStaticCache.get(c);
	}
	static Map<CacheKey, WeakReference<MethodHandle>> getSpecialCache(Class<?> c) {
		return findSpecialCache.get(c);
	}
	static Map<CacheKey, WeakReference<MethodHandle>> getConstructorCache(Class<?> c) {
		return findConstructorCache.get(c);
	}
	static Map<CacheKey, WeakReference<MethodHandle>> getFieldSetterCache(Class<?> c) {
		return fieldSetterCache.get(c);
	}
	static Map<CacheKey, WeakReference<MethodHandle>> getFieldGetterCache(Class<?> c) {
		return fieldGetterCache.get(c);
	}
	static Map<CacheKey, WeakReference<MethodHandle>> getStaticFieldSetterCache(Class<?> c) {
		return staticFieldSetterCache.get(c);
	}
	static Map<CacheKey, WeakReference<MethodHandle>> getStaticFieldGetterCache(Class<?> c) {
		return staticFieldGetterCache.get(c);
	}

	/* Search the 'perClassCache' returned by one of the 'get{Virtual|Static|Special|Constructor}Cache(Class)' methods
	 * for the MethodHandle with matching name and type.
	 */
	public static MethodHandle getMethodFromPerClassCache(Map<CacheKey, WeakReference<MethodHandle>> perClassCache, String name, MethodType type) {
		return getMethodWithSpecialCallerFromPerClassCache(perClassCache, name, type, null);
	}
	
	public static MethodHandle getMethodWithSpecialCallerFromPerClassCache(Map<CacheKey, WeakReference<MethodHandle>> perClassCache, String name, MethodType type, Class<?> specialCaller) {
		WeakReference<MethodHandle> handleRef = perClassCache.get(new MethodCacheKey(name, type, specialCaller));
		if (handleRef != null) {
			return handleRef.get();
		}
		return null;
	}
	
	public static MethodHandle getFieldFromPerClassCache(Map<CacheKey, WeakReference<MethodHandle>> perClassCache, String name, Class<?> fieldType) {
		WeakReference<MethodHandle> handleRef = perClassCache.get(new FieldCacheKey(name, fieldType));
		if (handleRef != null) {
			return handleRef.get();
		}
		return null;
	}

	/* Update the cache to hold the <Name, Type> -> MethodHandle mapping */
	public static MethodHandle putMethodInPerClassCache(Map<CacheKey, WeakReference<MethodHandle>> perClassCache, String name, MethodType type, MethodHandle handle) {
		return putMethodWithSpecialCallerInPerClassCache(perClassCache, name, type, handle, null);
	}
	
	/* Update the cache to hold the <Name, Type, SpecialCaller> -> MethodHandle mapping */
	public static MethodHandle putMethodWithSpecialCallerInPerClassCache(Map<CacheKey, WeakReference<MethodHandle>> perClassCache, String name, MethodType type, MethodHandle handle, Class<?> specialCaller) {
		return cacheHandle(perClassCache, new MethodCacheKey(name, type, specialCaller), handle);
	}
	
	/* Update the cache to hold the <Name, FieldType> -> MethodHandle mapping */
	public static MethodHandle putFieldInPerClassCache(Map<CacheKey, WeakReference<MethodHandle>> perClassCache, String fieldName, Class<?> fieldType, MethodHandle handle) {
		return cacheHandle(perClassCache, new FieldCacheKey(fieldName, fieldType), handle);
	}
	
	private static MethodHandle cacheHandle(Map<CacheKey, WeakReference<MethodHandle>> perClassCache, CacheKey cacheKey, MethodHandle handle){
		/* Keep a strong reference to the FieldCacheKey in the MH being cached so that it won't
		 * be immediately collected.  Uses a WeakHashMap<FieldCacheKey, WeakRef<MH>> to cache.
		 * Since the MH keeps a strong ref to the FieldCacheKey, as long as the MH is alive
		 * the Key can't be collected, despite being a weakref.
		 */
		handle.cacheKey = cacheKey;
		perClassCache.put(handle.cacheKey, new WeakReference<MethodHandle>(handle));
		return handle;
	}

}

