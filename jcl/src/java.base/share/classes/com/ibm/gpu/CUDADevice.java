/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

/**
 * Representation of a real, connected CUDA device.
 */
public final class CUDADevice {

	private final int deviceID;

	private final int doubleThresholdValue;

	private final int floatThresholdValue;

	private final int intThresholdValue;

	private final int longThresholdValue;

	private final String model;

	CUDADevice(int deviceID, String modelName, int doubleThresholdValue,
			int floatThresholdValue, int intThresholdValue,
			int longThresholdValue) {
		super();
		this.deviceID = deviceID;
		this.doubleThresholdValue = doubleThresholdValue;
		this.floatThresholdValue = floatThresholdValue;
		this.intThresholdValue = intThresholdValue;
		this.longThresholdValue = longThresholdValue;
		this.model = modelName;
	}

	/**
	 * Returns the device ID of this CUDA device.
	 *
	 * @return the device ID of the device
	 */
	public int getDeviceID() {
		return deviceID;
	}

	/**
	 * Returns the minimum size of a double array that will be sorted using
	 * this CUDA device if enabled.
	 *
	 * @return the minimum size of a double array that will be sorted using
	 * this CUDA device
	 */
	public int getDoubleThreshold() {
		return doubleThresholdValue;
	}

	/**
	 * Returns the minimum size of a float array that will be sorted using
	 * this CUDA device if enabled.
	 *
	 * @return the minimum size of a float array that will be sorted using
	 * this CUDA device
	 */
	public int getFloatThreshold() {
		return floatThresholdValue;
	}

	/**
	 * Returns the minimum size of an int array that will be sorted using
	 * this CUDA device if enabled.
	 *
	 * @return the minimum size of an int array that will be sorted using
	 * this CUDA device
	 */
	public int getIntThreshold() {
		return intThresholdValue;
	}

	/**
	 * Returns the minimum size of a long array that will be sorted using
	 * this CUDA device if enabled.
	 *
	 * @return the minimum size of a long array that will be sorted using
	 * this CUDA device
	 */
	public int getLongThreshold() {
		return longThresholdValue;
	}

	/** {@inheritDoc} */
	@Override
	public String toString() {
		return deviceID + ": " + model; //$NON-NLS-1$
	}

}
