/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2014, 2017 IBM Corp. and others
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
package com.ibm.gpu;

import java.io.IOException;
import java.io.InputStream;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.BitSet;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Properties;
import java.util.TreeMap;

import com.ibm.cuda.Cuda;
import com.ibm.cuda.CudaDevice;

/*[IF Sidecar19-SE]*/
import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
/*[ELSE]
import sun.misc.Unsafe;
/*[ENDIF]*/

/**
 * This class contains information important to IBM GPU enabled functions.
 */
public final class CUDAManager {

	private static final class Configuration {

		private static final String DEFAULT_MODEL_NAME = "DEFAULT"; //$NON-NLS-1$

		private static final int DEFAULT_THRESHOLD = 30000;

		private static void loadProperties(Properties properties, String resourceName) throws IOException {
			PrivilegedAction<InputStream> action = () -> CUDAManager.class.getResourceAsStream(resourceName);

			try (InputStream input = AccessController.doPrivileged(action)) {
				if (input != null) {
					properties.load(input);
				}
			}
		}

		private static boolean startsWithIgnoreCase(String string, String prefix) {
			int prefixLength = prefix.length();

			if (string.length() >= prefixLength) {
				return string.regionMatches(true, 0, prefix, 0, prefixLength);
			}

			return true;
		}

		private final CUDAManager manager;

		/*
		 * Keyed by model name; then by type.
		 */
		private final Map<String, Map<Type, Integer>> thresholds;

		Configuration(CUDAManager manager) {
			super();
			this.manager = manager;
			this.thresholds = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);

			readThresholds();
		}

		boolean checkSortProperty(String name) {
			String value = getProperty(name);

			if (value != null) {
				if (value.isEmpty() // <br/>
						|| value.equalsIgnoreCase("all") //$NON-NLS-1$
						|| value.equalsIgnoreCase("sort")) { //$NON-NLS-1$
					return true;
				}

				manager.outputIfVerbose(String.format(
						"Invalid value \"%s\" given on system property %s", //$NON-NLS-1$
						value, name));
			}

			return false;
		}

		private int getDefaultThreshold(Type type) {
			Map<Type, Integer> modelMap = thresholds.get(DEFAULT_MODEL_NAME);

			if (modelMap != null) {
				Integer threshold = modelMap.get(type);

				if (threshold != null) {
					return threshold.intValue();
				}
			}

			return DEFAULT_THRESHOLD;
		}

		int getDoubleThreshold() {
			return getDefaultThreshold(Type.DOUBLE);
		}

		int getDoubleThreshold(String modelName) {
			return getThreshold(modelName, Type.DOUBLE);
		}

		int getFloatThreshold() {
			return getDefaultThreshold(Type.FLOAT);
		}

		int getFloatThreshold(String modelName) {
			return getThreshold(modelName, Type.FLOAT);
		}

		int getIntThreshold() {
			return getDefaultThreshold(Type.INT);
		}

		int getIntThreshold(String modelName) {
			return getThreshold(modelName, Type.INT);
		}

		int getLongThreshold() {
			return getDefaultThreshold(Type.LONG);
		}

		int getLongThreshold(String modelName) {
			return getThreshold(modelName, Type.LONG);
		}

		private int getThreshold(String modelName, Type type) {
			Map<Type, Integer> modelMap = thresholds.get(modelName);

			if (modelMap != null) {
				Integer threshold = modelMap.get(type);

				if (threshold != null) {
					return threshold.intValue();
				}
			}

			return getDefaultThreshold(type);
		}

		private void readThresholds() {
			Properties properties = new Properties();

			try {
				loadProperties(properties, "ibm_gpu_thresholds.properties"); //$NON-NLS-1$
			} catch (IOException e) {
				manager.outputIfVerbose("Warning: couldn't load threshold properties file: " //$NON-NLS-1$
						+ e.getLocalizedMessage());
			}

			for (Entry<Object, Object> property : properties.entrySet()) {
				String propertyName = String.valueOf(property.getKey());

				for (Type type : Type.values()) {
					String prefix = type.propertyPrefix;

					if (!startsWithIgnoreCase(propertyName, prefix)) {
						continue;
					}

					String propertyValue = String.valueOf(property.getValue());

					try {
						int value = Integer.parseInt(propertyValue);

						if (0 < value && value < Integer.MAX_VALUE) {
							String model = propertyName.substring(prefix.length()).replace('_', ' ');
							Map<Type, Integer> modelMap = thresholds.get(model);

							if (modelMap == null) {
								thresholds.put(model, modelMap = new HashMap<>());
							}

							modelMap.put(type, Integer.valueOf(value));
						}
					} catch (NumberFormatException e) {
						manager.outputIfVerbose(String.format(
								"Warning: ignoring non-numeric threshold: %s = %s", //$NON-NLS-1$
								propertyName, propertyValue));
					}
					break;
				}
			}
		}

	}

	private static final class Lock {

		Lock() {
			super();
		}

	}

	private static final class Singleton {

		static final CUDAManager INSTANCE = new CUDAManager();

	}

	private static enum Type {

		DOUBLE, FLOAT, INT, LONG;

		final String propertyPrefix;

		Type() {
			propertyPrefix = "com.ibm.gpu." + name() + "sortThreshold."; //$NON-NLS-1$ //$NON-NLS-2$
		}

	}

	private static final Lock lock = new Lock();

	/**
	 * Return a CUDAManager instance.
	 *
	 * @return a CUDAManager instance
	 * @throws GPUConfigurationException
	 *          This exception is not actually thrown; use {@code instance()} instead.
	 * @throws SecurityException
	 *          If a security manager exists and the calling thread does not
	 *          have permission to access the CUDAManager instance.
	 * @deprecated Use {@code instance()} instead.
	 */
	@Deprecated
	public static CUDAManager getInstance()
			throws GPUConfigurationException, SecurityException {
		return instance();
	}

	/**
	 * Return a CUDAManager instance.
	 *
	 * @return a CUDAManager instance
	 * @throws SecurityException
	 *          If a security manager exists and the calling thread does not
	 *          have permission to access the CUDAManager instance.
	 */
	public static CUDAManager instance() throws SecurityException {
		SecurityManager security = System.getSecurityManager();

		if (security != null) {
			security.checkPermission(GPUPermission.Access);
		}

		return instanceInternal();
	}

	static CUDAManager instanceInternal() {
		return Singleton.INSTANCE;
	}

	/**
	 * Get the header used to prefix all IBM GPU related output.
	 *
	 * @return The header used for IBM GPU related output.
	 */
	public static String getOutputHeader() {
		return "[IBM GPU]:"; //$NON-NLS-1$
	}

	static String getProperty(String name) {
		PrivilegedAction<String> action = () -> System.getProperty(name);

		return AccessController.doPrivileged(action);
	}

	/**
	 * Get the version of this class.
	 *
	 * @return Returns the version of this class.
	 */
	public static String getVersion() {
		return Version.VERSION;
	}

	/**
	 * Performs cleanup on the CUDAManager class.
	 *
	 * @deprecated This method has no effect; it will be removed in a future version.
	 */
	@Deprecated
	public static void tearDown() {
		return;
	}

	private final BitSet busyDevices;

	private int defaultDeviceId;

	private final int defaultDoubleThreshold;

	private final int defaultFloatThreshold;

	private final int defaultIntThreshold;

	private final int defaultLongThreshold;

	private CUDADevice[] devices;

	/*[IF Sidecar19-SE]*/
	private final VarHandle devicesHandle;
	/*[ELSE]
	private final long devicesOffset;
	private final Unsafe unsafe;
	/*[ENDIF]*/

	private boolean doSortOnGPU;

	private boolean enforceGPUSort;

	private boolean verboseOutput;

	CUDAManager() {
		super();
		busyDevices = new BitSet();
		defaultDeviceId = 0;
		devices = null;
		doSortOnGPU = false;
		enforceGPUSort = false;

		// set this early for better feedback
		verboseOutput = getProperty("com.ibm.gpu.verbose") != null; //$NON-NLS-1$

		/*[IF Sidecar19-SE]*/
		try {
			devicesHandle = MethodHandles.lookup().findVarHandle(CUDAManager.class, "devices", CUDADevice[].class); //$NON-NLS-1$
		} catch (IllegalAccessException | NoSuchFieldException e) {
			throw new InternalError(e.toString(), e);
		}
		/*[ELSE]
		try {
			unsafe = Unsafe.getUnsafe();
			devicesOffset = unsafe.objectFieldOffset(CUDAManager.class.getDeclaredField("devices")); //$NON-NLS-1$
		} catch (NoSuchFieldException e) {
			InternalError error = new InternalError(e.toString());
			error.initCause(e);
			throw error;
		}
		/*[ENDIF]*/

		Configuration configuration = new Configuration(this);

		defaultDoubleThreshold = configuration.getDoubleThreshold();
		defaultFloatThreshold = configuration.getFloatThreshold();
		defaultIntThreshold = configuration.getIntThreshold();
		defaultLongThreshold = configuration.getLongThreshold();

		if (configuration.checkSortProperty("com.ibm.gpu.enforce")) { //$NON-NLS-1$
			doSortOnGPU = true;
			enforceGPUSort = true;
		} else if (configuration.checkSortProperty("com.ibm.gpu.enable")) { //$NON-NLS-1$
			doSortOnGPU = true;
		}

		if (configuration.checkSortProperty("com.ibm.gpu.disable")) { //$NON-NLS-1$
			doSortOnGPU = false;
			enforceGPUSort = false;
		}
	}

	/**
	 * Look for the next free device and mark it as busy.
	 *
	 * @return Returns the device ID of the next free device.
	 */
	public int acquireFreeDevice() {
		synchronized (lock) {
			int deviceId = busyDevices.nextClearBit(0);

			if (deviceId < getDeviceCount()) {
				outputIfVerbose("Acquired device: " + deviceId); //$NON-NLS-1$
				busyDevices.set(deviceId);
			} else {
				outputIfVerbose("No available devices found"); //$NON-NLS-1$
				deviceId = -1;
			}

			return deviceId;
		}
	}

	private CUDADevice[] findDevices() {
		int deviceCount = 0;

		try {
			deviceCount = Cuda.getDeviceCount();
		} catch (Exception e) {
			// Cuda.getDeviceCount() declares but never throws CudaException.
			outputIfVerbose("Couldn't count devices due to: " + e.getLocalizedMessage()); //$NON-NLS-1$
		} catch (NoClassDefFoundError e) {
			outputIfVerbose("Unsupported platform detected"); //$NON-NLS-1$
		}

		CUDADevice[] allDevices = new CUDADevice[deviceCount];

		if (deviceCount != 0) {
			Configuration configuration = new Configuration(this);

			for (int deviceId = 0; deviceId < deviceCount; ++deviceId) {
				String modelName = ""; //$NON-NLS-1$

				try {
					modelName = new CudaDevice(deviceId).getName();
				} catch (Exception e) {
					// This is likely a CudaException but we can't catch it specifically
					// or class loading verification would fail for this class.
					outputIfVerbose("Warning: couldn't get the GPU model name for device " + deviceId); //$NON-NLS-1$
				}

				allDevices[deviceId] = new CUDADevice(deviceId, modelName, // <br/>
						configuration.getDoubleThreshold(modelName), // <br/>
						configuration.getFloatThreshold(modelName), // <br/>
						configuration.getIntThreshold(modelName), // <br/>
						configuration.getLongThreshold(modelName));
			}

			outputIfVerbose("Discovered " + deviceCount + " device(s)"); //$NON-NLS-1$ //$NON-NLS-2$
		}

		return allDevices;
	}

	/**
	 * Use this method to obtain a reference to an ArrayList containing
	 * references to all discovered CUDA devices.
	 *
	 * @return Returns an ArrayList containing all discovered CUDA
	 * devices - see {@link CUDADevice} for details.
	 */
	public ArrayList<CUDADevice> getCUDADevices() {
		return new ArrayList<>(Arrays.asList(getDevices()));
	}

	/**
	 * Gets the ID of the default device, set to 0 by default.
	 *
	 * @return Returns the device ID of the current default device.
	 */
	public int getDefaultDevice() {
		return defaultDeviceId;
	}

	/**
	 * Get a reference to the CUDA device by means of its index (with 0 being the first).
	 *
	 * @param deviceId The index of the device to retrieve (with 0 being the first).
	 * @return Returns a CUDA device at the specified index - see
	 * {@link CUDADevice} for details.
	 * @throws GPUConfigurationException Throws this exception if an invalid deviceId
	 * has been specified.
	 */
	public CUDADevice getDevice(int deviceId) throws GPUConfigurationException {
		CUDADevice[] allDevices = getDevices();

		if (0 <= deviceId && deviceId < allDevices.length) {
			return allDevices[deviceId];
		} else {
			throw newGPUConfigurationException("Invalid device"); //$NON-NLS-1$
		}
	}

	/**
	 * Identifies the number of available CUDA devices.
	 *
	 * @return Returns how many CUDA devices have been detected.
	 */
	public int getDeviceCount() {
		return getDevices().length;
	}

	private CUDADevice[] getDevices() {
		CUDADevice[] allDevices = devices;

		if (allDevices == null) {
			synchronized (lock) {
				allDevices = devices;

				if (allDevices == null) {
					allDevices = findDevices();
					/*[IF Sidecar19-SE]*/
					devicesHandle.setRelease(this, allDevices);
					/*[ELSE]
					unsafe.putOrderedObject(this, devicesOffset, allDevices);
					/*[ENDIF]*/
				}
			}
		}

		return allDevices;
	}

	/**
	 * Identifies the CUDA device that has the most memory available.
	 *
	 * @return Returns a reference to the CUDA device with the most memory available.
	 * @throws GPUConfigurationException Throws this exception if an
	 * attempt was made to access an invalid device (no longer available).
	 */
	public CUDADevice getDeviceWithMostAvailableMemory()
			throws GPUConfigurationException {
		CUDADevice[] allDevices = getDevices();
		CUDADevice bestDevice = null;
		int deviceCount = allDevices.length;
		long mostFreeMem = 0;

		for (int deviceId = 0; deviceId < deviceCount; ++deviceId) {
			try {
				long freeMem = new CudaDevice(deviceId).getFreeMemory();

				if (mostFreeMem < freeMem || deviceId == 0) {
					bestDevice = allDevices[deviceId];
					mostFreeMem = freeMem;
				}
			} catch (Exception e) {
				// ignore
			}
		}

		return bestDevice;
	}

	/**
	 * Gets the minimum length of a double array that will be
	 * sorted using a GPU if enabled.
	 *
	 * @return The minimum length of a double array that will be
	 * sorted using a GPU.
	 */
	public int getDoubleThreshold() {
		return defaultDoubleThreshold;
	}

	/**
	 * Use this method to return an array of enabled CUDA devices.
	 *
	 * @return Returns an array containing enabled CUDA devices -
	 * see {@link CUDADevice} for details.
	 */
	public CUDADevice[] getEnabledCUDADevices() {
		return getDevices().clone();
	}

	/**
	 * Gets the minimum length of a float array that will be
	 * sorted using a GPU if enabled.
	 *
	 * @return The minimum length of a float array that will be
	 * sorted using a GPU.
	 */
	public int getFloatThreshold() {
		return defaultFloatThreshold;
	}

	/**
	 * Get the amount of free memory (in bytes) available for the provided CUDA device.
	 * Does not persistently change the current device.
	 *
	 * @param deviceId The index of the device to query (with 0 being the first).
	 * @return Returns the amount of free memory available.
	 * @throws GPUConfigurationException Throw this exception if cannot get free memory amount.
	 */
	public long getFreeMemoryForDevice(int deviceId)
			throws GPUConfigurationException {
		if (0 <= deviceId && deviceId < getDeviceCount()) {
			try {
				CudaDevice device = new CudaDevice(deviceId);

				return device.getFreeMemory();
			} catch (Exception e) {
				// This is likely a CudaException but we can't catch it specifically
				// or class loading verification would fail for this class.
				throw new GPUConfigurationException(e.getLocalizedMessage(), e);
			}
		} else {
			throw newGPUConfigurationException("Invalid device"); //$NON-NLS-1$
		}
	}

	/**
	 * Gets the minimum length of an int array that will be
	 * sorted using a GPU if enabled.
	 *
	 * @return The minimum length of an int array that will be
	 * sorted using a GPU.
	 */
	public int getIntThreshold() {
		return defaultIntThreshold;
	}

	/**
	 * Gets the minimum length of a long array that will be
	 * sorted using a GPU if enabled.
	 *
	 * @return The minimum length of a long array that will be
	 * sorted using a GPU.
	 */
	public int getLongThreshold() {
		return defaultLongThreshold;
	}

	/**
	 * Returns the next CUDA device that is available to run calculations on.
	 *
	 * @return -1 if there are no free devices, otherwise returns
	 * the ID of the free CUDA device.
	 */
	public int getNextAvailableDevice() {
		synchronized (lock) {
			int deviceId = busyDevices.nextClearBit(0);

			if (deviceId < getDeviceCount()) {
				outputIfVerbose("Device " + deviceId + " was free"); //$NON-NLS-1$ //$NON-NLS-2$
			} else {
				outputIfVerbose("No free devices!"); //$NON-NLS-1$
				deviceId = -1;
			}

			return deviceId;
		}
	}

	/**
	 * Get the value of the verboseGPUOutput flag.
	 *
	 * @return Whether or not verbose output should be produced.
	 */
	public boolean getVerboseGPUOutput() {
		return verboseOutput;
	}

	/**
	 * Use this method to identify if CUDA is supported on this machine and
	 * within this environment: returns true if the number of CUDA devices
	 * detected is greater than 0.
	 *
	 * @return Returns true if one or more CUDA devices have been detected.
	 */
	public boolean hasCUDASupport() {
		return getDeviceCount() != 0;
	}

	/**
	 * This method provides a means to determine if sort is
	 * enabled to be used by any available CUDA device.
	 *
	 * @return Returns true if GPU sort is enabled.
	 */
	public boolean isSortEnabledOnGPU() {
		return doSortOnGPU;
	}

	/**
	 * This method provides a means to determine if sort is
	 * forced to be used by any available CUDA device.
	 *
	 * @return Returns true if GPU sort is forced.
	 */
	public boolean isSortEnforcedOnGPU() {
		return enforceGPUSort;
	}

	private GPUConfigurationException newGPUConfigurationException(String message) {
		outputIfVerbose(message);
		return new GPUConfigurationException(getOutputHeader() + ' ' + message);
	}

	void outputIfVerbose(String message) {
		if (verboseOutput) {
			System.out.printf(
					"%s [time.ms=%d]: %s\n", //$NON-NLS-1$
					getOutputHeader(),
					Long.valueOf(System.currentTimeMillis()), message);
		}
	}

	/**
	 * Print information for each detected CUDA device.
	 */
	public void printAllDeviceInfo() {
		CUDADevice[] allDevices = getDevices();

		System.out.println("Number of devices: " + allDevices.length); //$NON-NLS-1$

		for (CUDADevice device : allDevices) {
			System.out.println(device);
		}
	}

	/**
	 * Mark a device as being free; must be in a try finally block as we MUST
	 * release the handle regardless of whether or not a sort was successful.
	 *
	 * @param deviceId The device to be marked as free.
	 */
	public void releaseDevice(int deviceId) {
		synchronized (lock) {
			if (0 <= deviceId && deviceId < getDeviceCount()) {
				busyDevices.clear(deviceId);
				outputIfVerbose("Released device: " + deviceId); //$NON-NLS-1$
			}
		}
	}

	/**
	 * Sets the default device to the given device ID.
	 * @param deviceId The new default device.
	 */
	public void setDefaultDevice(int deviceId) {
		defaultDeviceId = deviceId;
	}

	/**
	 * Use this method to set the device to use for subsequent calls.
	 *
	 * @param deviceId Set the default device ID to be this.
	 * @throws GPUConfigurationException Throws this exception if an invalid device
	 * number was specified.
	 */
	public void setDevice(int deviceId) throws GPUConfigurationException {
		if (0 <= deviceId && deviceId < getDeviceCount()) {
			this.setDefaultDevice(deviceId);
		} else {
			throw newGPUConfigurationException("Invalid device"); //$NON-NLS-1$
		}
	}

	/**
	 * Set the value of the verboseGPUOutput flag. When this flag is true, GPU
	 * output will be produced.
	 *
	 * @param condition Whether or not verbose output should be produced.
	 */
	public void setVerboseGPU(boolean condition) {
		verboseOutput = condition;
	}

}
