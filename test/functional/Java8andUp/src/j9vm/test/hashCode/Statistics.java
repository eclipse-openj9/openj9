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
package j9vm.test.hashCode;

import java.io.PrintStream;

/**
 * @author pdbain
 *
 */
public class Statistics {
	/**
	 * Track the statistics of numbers to get maximum, minimum, mean, and standard deviation.
	 */

	long count, maximum, minimum, sum, sumOfSquares;
	String variableName;
	
	/**
	 * @param name  Name of the variable being measured
	 */
	public Statistics(String name) {
		this.variableName = name;
		count = 0;
		maximum = Long.MIN_VALUE;
		minimum = Long.MAX_VALUE;
		sum = 0;
		sumOfSquares = 0;
	}
	
	/**
	 * Add another value to the statistics.
	 * @param newValue Value of the current sample of the variable
	 */
	public void add(long newValue) {
		sum += newValue;
		sumOfSquares += newValue*newValue;
		maximum = java.lang.Math.max(newValue, maximum);
		minimum = java.lang.Math.min(newValue, minimum);
		++count;
	}

	/**
	 * @return number of values
	 */
	public long getCount() {
		return count;
	}

	/**
	 * @return Maximum value, or MIN_VALUE if count == 0
	 */
	public long getMaximum() {
		return maximum;
	}

	/**
	 * @return Minimum value, or MAX_VALUE if count == 0
	 */
	public long getMinimum() {
		return minimum;
	}

	/**
	 * @return Sum of the values; 0 if count = 0
	 */
	public long getSum() {
		return sum;
	}

	/**
	 * @return Arithmetic mean of the values; 0 if count = 0
	 */
	public long getMean() {
		return (count > 0)? (sum/count): 0;
	}

	/**
	 * @return Variance of the values; 0 if count < 2
	 */
	public long getVariance() {
		long variance = 0;
		if (count > 1) {
			variance = ((count * sumOfSquares) -  (sum*sum))/(count * (count-1));
		}
		return variance;
	}

	/**
	 * @return Standard deviation of the values; 0 if count = 0
	 */
	public long getStandardDeviation() {
		
		return (long) java.lang.Math.sqrt(getVariance());
	}

	/**
	 * @return Name of the variable being measured
	 */
	public String getVariableName() {
		return variableName;
	}
	
	/**
	 * Print the count, sum, max, min, mean, variance, and std. deviation, one per line prefixed by the variable name.
	 * @param output Print stream to which to write the results
	 */
	public void printStatistics(PrintStream output) {
		output.println("Statistics for "+variableName);
		output.println(variableName+" count = "+count);
		output.println(variableName+" sum = "+sum);
		output.println(variableName+" minimum = "+minimum);
		output.println(variableName+" maximum = "+maximum);
		output.println(variableName+" mean = "+getMean());
		output.println(variableName+" variance = "+getVariance());
		output.println(variableName+" standard deviation = "+getStandardDeviation());
	}
}
