/*******************************************************************************
 * Copyright (c) 2008, 2019 IBM Corp. and others
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
#ifndef XMLBENCHOUTPUTWRITER_HPP_
#define XMLBENCHOUTPUTWRITER_HPP_

#include "j9port.h"

class XMLBenchOutputWriter {
	
private:
	bool _outputXml;
	J9PortLibrary* _portLibrary;
	
protected:
	
	/**
	 * This redefines the new operator for this class and subclasses
	 *
	 * @param size the size of the object
	 * @param the pointer to the object
	 * @returns a pointer to the object
	 */
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };
	
	/**
	 * constructor
	 * 
	 * @param portLibrary
	 * @param createOutput set this to true if you want xml output to be created
	 */
	XMLBenchOutputWriter(J9PortLibrary* portLibrary, bool createOutput);

public:
	
	/**
	 * This method should be used to destroy this class and release any associated memory
	 */
	virtual void kill();

	 /**
	  * This method can be used to create an instanced of this class
	  *
	  * @param portLib the port library that should be associated with the class
	  * @param createOutput set this to true if you want xml output to be created
	  */
	static XMLBenchOutputWriter* newInstance(J9PortLibrary* portlib, bool createOutput);
	
	
public:
	
	/**
	 * this indicates xml output is enabled or not
	 * @returns true if xml output is enabled
	 */
	bool isEnabled(){
		return _outputXml;
	}
	
	/**
	 * This writes out the xml to start a chart set
	 */
	void startChartSet();

	/**
	 * This writes out the xml to end a chart set
	 */
	void endChartSet();

	/**
	 * This writes out the xml to start the definition of the charts in a chartset
	 */
	void startCharts();

	/** 
	 * This writes out the xml to end the definition of the charts in a chartset
	 */
	void endCharts();

	/** 
	 * This writes out the xml to start a chart
	 * @param chartName the name to be associated with the chart
	 * @param chartImage the name of the file containing the chart
	 * @param xAxisLabel the label for the X axis
	 * @param yAxisLabel the label for the Y axis
	 * @param xAxisLog true if the X axis should be logarithmic
	 * @param yAxisLog true if the Y axis should be logarithmic
	 */
	void startChart(char* chartName, char* chartImage, char* xAxisLabel, char* yAxisLabel, bool xAxisLog, bool yAxisLog);

	/**
	 * This writes out the xml to end a chart
	 */
	void endChart();

	/**
	 * This writes out the xml for a dataset selector
	 * 
	 * @param baseId the base id for the dataset to be included in the chart
	 * @param extendedId the extended id for the dataset to be included in the chart, a value of 0 matches all extendedId values
	 * @param datasetLabel the label to be shown in the chart for the dataset 
	 */
	void writeDataSetSelector(U_32 baseId, I_32 extendedId, char* datasetLabel);

	/**
	 * This writes out the xml to start the datasets 
	 */
	void startDataSets();

	/**
	 * This writes out the xml to end the datasets
	 */
	void endDataSets();

	/**
	 * This writes out the xml to start a dataset
	 * 
	 * @param baseId the base Id to be associated with the dataset
	 * @param extendedID the extended Id to be associated with the dataset.  This must be >0
	 * @param extendedLabel the label to be added to the base name for the dataset when selected based on the extended id
	 */
	void startDataSet(U_32 baseId, U_32 extendedID, char* extendedLabel);

	/**
	 * This writes out the xml to end a dataset 
	 */
	void endDataSet();
	
	/**
	 * This writes out a datapoint which is part of a dataset
	 * @param xvalue the x value for the datapoint
	 * @param yvalue the y value for the datapoint
	 */
	void writeDataPoint(float xvalue, float yvalue);
};

#endif /* XMLBENCHOUTPUTWRITER_HPP_ */
