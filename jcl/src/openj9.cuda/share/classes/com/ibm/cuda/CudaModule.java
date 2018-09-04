/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
package com.ibm.cuda;

import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicLong;

import com.ibm.cuda.internal.CudaUtil;

/**
 * The {@code CudaModule} class represents a module that has been loaded
 * on a CUDA-capable device.
 * <p>
 * When no longer required, a module must be unloaded (see {@link #unload()}).
 */
public final class CudaModule {

	/**
	 * The {@code Cache} class provides a simple mechanism to avoid reloading
	 * modules repeatedly. The set of loaded modules is specific to each device
	 * so two pieces of identification are required for each module: the device
	 * and a user-supplied key.
	 * <p>
	 * Note: Because this class is implemented with {@link HashMap}, keys
	 * must implement {@link #equals(Object)} and {@link #hashCode()}.
	 */
	public static final class Cache {

		private final Map<Object, Map<CudaDevice, CudaModule>> store;

		/**
		 * Creates a new cache.
		 */
		public Cache() {
			super();
			this.store = new HashMap<>(1);
		}

		/**
		 * Retrieves an existing module for the specified device and key.
		 *
		 * @param device
		 *          the specified device
		 * @param key
		 *          the specified key
		 * @return
		 *          return the module associated with the given key on the
		 *          specified device, or null if no such module exists
		 */
		public CudaModule get(CudaDevice device, Object key) {
			Map<?, CudaModule> map = store.get(key);

			return map == null ? null : map.get(device);
		}

		/**
		 * Stores a module in this cache, associating it with the given
		 * device and key.
		 *
		 * @param device
		 *          the specified device
		 * @param key
		 *          the specified key
		 * @param module
		 *          the module to be stored
		 * @return
		 *          the module previously associated with the given key on
		 *          the specified device, or null if no such module exists
		 */
		public CudaModule put(CudaDevice device, Object key, CudaModule module) {
			Map<CudaDevice, CudaModule> map = store.get(key);

			if (map == null) {
				store.put(key, map = new HashMap<>());
			}

			return map.put(device, module);
		}
	}

	private static native long getFunction(int deviceId, long moduleHandle,
			String name) throws CudaException;

	private static native long getGlobal(int deviceId, long moduleHandle,
			String name) throws CudaException;

	private static native long getSurface(int deviceId, long moduleHandle,
			String name) throws CudaException;

	private static native long getTexture(int deviceId, long moduleHandle,
			String name) throws CudaException;

	private static native long load(int deviceId, byte[] image,
			long optionsHandle) throws CudaException;

	private static native void unload(int deviceId, long moduleHandle)
			throws CudaException;

	final int deviceId;

	private final Map<String, CudaFunction> functions;

	private final Map<String, CudaGlobal> globals;

	private final AtomicLong nativeHandle;

	private final Map<String, CudaSurface> surfaces;

	private final Map<String, CudaTexture> textures;

	/**
	 * Loads a module on the specified device, using the given image and the
	 * default options.
	 *
	 * @param device
	 *          the specified device
	 * @param image
	 *          the module image
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws SecurityException
	 *          if a security manager exists and the calling thread
	 *          does not have permission to load GPU modules
	 */
	public CudaModule(CudaDevice device, byte[] image) throws CudaException {
		this(device, image, null);
	}

	/**
	 * Loads a module on the specified device, using the given image and the
	 * given options.
	 *
	 * @param device
	 *          the specified device
	 * @param image
	 *          the module image
	 * @param options
	 *          the desired options
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws SecurityException
	 *          if a security manager exists and the calling thread
	 *          does not have permission to load GPU modules
	 */
	public CudaModule(CudaDevice device, byte[] image, CudaJitOptions options)
			throws CudaException {
		super();

		SecurityManager security = System.getSecurityManager();

		if (security != null) {
			security.checkPermission(CudaPermission.LoadModule);
		}

		if (image == null) {
			throw new NullPointerException();
		}

		this.deviceId = device.getDeviceId();

		long optionsHandle = options == null ? 0 : options.getHandle();

		try {
			this.functions = new HashMap<>();
			this.globals = new HashMap<>();
			this.nativeHandle = new AtomicLong( // <br/>
					load(this.deviceId, image, optionsHandle));
			this.surfaces = new HashMap<>();
			this.textures = new HashMap<>();
		} finally {
			if (options != null) {
				options.releaseHandle(true);
			}
		}
	}

	/**
	 * Loads a module on the specified device from the given input stream using
	 * the default options.
	 *
	 * @param device
	 *          the specified device
	 * @param input
	 *          a stream containing the module image
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IOException
	 *          if an I/O error occurs reading {@code input}
	 * @throws SecurityException
	 *          if a security manager exists and the calling thread
	 *          does not have permission to load GPU modules
	 */
	public CudaModule(CudaDevice device, InputStream input)
			throws CudaException, IOException {
		this(device, input, null);
	}

	/**
	 * Loads a module on the specified device from the given input stream using
	 * the specified options.
	 *
	 * @param device
	 *          the specified device
	 * @param input
	 *          a stream containing the module image
	 * @param options
	 *          the desired options
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IOException
	 *          if an I/O error occurs reading {@code input}
	 * @throws SecurityException
	 *          if a security manager exists and the calling thread
	 *          does not have permission to load GPU modules
	 */
	public CudaModule(CudaDevice device, InputStream input,
			CudaJitOptions options) throws CudaException, IOException {
		this(device, CudaUtil.read(input, true), options);
	}

	/**
	 * Returns the function of the specified name from this module.
	 *
	 * @param name
	 *          the link-name of the desired function
	 * @return
	 *          the function of the specified name
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this module has been unloaded (see {@link #unload()})
	 */
	public CudaFunction getFunction(String name) throws CudaException {
		CudaFunction function = functions.get(name);

		if (function == null) {
			long address = getFunction(deviceId, getHandle(), name);

			functions.put(name, function = new CudaFunction(deviceId, address));
		}

		return function;
	}

	/**
	 * Returns the global variable of the specified name from this module.
	 *
	 * @param name
	 *          the link-name of the desired global variable
	 * @return
	 *          the global variable of the specified name
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this module has been unloaded (see {@link #unload()})
	 */
	public CudaGlobal getGlobal(String name) throws CudaException {
		CudaGlobal global = globals.get(name);

		if (global == null) {
			long address = getGlobal(deviceId, getHandle(), name);

			globals.put(name, global = new CudaGlobal(address));
		}

		return global;
	}

	private long getHandle() {
		long handle = nativeHandle.get();

		if (handle == 0) {
			throw new IllegalStateException();
		}

		return handle;
	}

	/**
	 * Returns the surface of the specified name from this module.
	 *
	 * @param name
	 *          the link-name of the desired surface
	 * @return
	 *          the surface of the specified name
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this module has been unloaded (see {@link #unload()})
	 */
	public CudaSurface getSurface(String name) throws CudaException {
		CudaSurface surface = surfaces.get(name);

		if (surface == null) {
			long address = getSurface(deviceId, getHandle(), name);

			surfaces.put(name, surface = new CudaSurface(address));
		}

		return surface;
	}

	/**
	 * Returns the texture of the specified name from this module.
	 *
	 * @param name
	 *          the link-name of the desired texture
	 * @return
	 *          the texture of the specified name
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this module has been unloaded (see {@link #unload()})
	 */
	public CudaTexture getTexture(String name) throws CudaException {
		CudaTexture texture = textures.get(name);

		if (texture == null) {
			long address = getTexture(deviceId, getHandle(), name);

			textures.put(name, texture = new CudaTexture(address));
		}

		return texture;
	}

	/**
	 * Unloads this module from the associated device.
	 * <p>
	 * Note that this has no effect on any {@link Cache caches}.
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public void unload() throws CudaException {
		long handle = nativeHandle.getAndSet(0);

		if (handle != 0) {
			functions.clear();
			globals.clear();
			surfaces.clear();
			textures.clear();
			unload(deviceId, handle);
		}
	}
}
