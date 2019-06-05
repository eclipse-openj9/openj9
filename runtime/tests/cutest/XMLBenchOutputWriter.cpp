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

#include <stdio.h>
#include "XMLBenchOutputWriter.hpp"
#include "j9memcategories.h"

/**
 * constructor
 * 
 * @param portLibrary
 * @param createOutput set this to true if you want xml output to be created
 */
XMLBenchOutputWriter::XMLBenchOutputWriter(J9PortLibrary* portLibrary, bool createOutput) 
{
	_outputXml = createOutput;
	_portLibrary = portLibrary;
}

/**
 * This method should be used to destroy this class and release any associated memory
 */
void XMLBenchOutputWriter::kill()
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	j9mem_free_memory(this);
}

/**
 * This method can be used to create an instanced of this class
 *
 * @param portLib the port library that should be associated with the class
 * @param createOutput set this to true if you want xml output to be created
 */
XMLBenchOutputWriter* XMLBenchOutputWriter::newInstance(J9PortLibrary* portlib, bool createOutput) {
	PORT_ACCESS_FROM_PORT(portlib);
	XMLBenchOutputWriter* obj = (XMLBenchOutputWriter*) j9mem_allocate_memory(sizeof(XMLBenchOutputWriter), OMRMEM_CATEGORY_VM);
	if (obj){
		new (obj) XMLBenchOutputWriter(portlib, createOutput);
	}
	return obj;
}

/**
 * This writes out the xml to start a chart set
 */
void XMLBenchOutputWriter::startChartSet()
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB, "<chartSet>\n");
	}
};

/**
 * This writes out the xml to end a chart set
 */
void XMLBenchOutputWriter::endChartSet()
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"</chartSet>\n");
	}
};

/**
 * This writes out the xml to start the definition of the charts in a chartset
 */
void XMLBenchOutputWriter::startCharts()
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"   <charts>\n");
	}
};

/** 
 * This writes out the xml to end the definition of the charts in a chartset
 */
void XMLBenchOutputWriter::endCharts()
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"   </charts>\n");
	}
};

/** 
 * This writes out the xml to start a chart
 * @param chartName the name to be associated with the chart
 * @param chartImage the name of the file containing the chart
 * @param xAxisLabel the label for the X axis
 * @param yAxisLabel the label for the Y axis
 * @param xAxisLog true if the X axis should be logarithmic
 * @param yAxisLog true if the Y axis should be logarithmic
 */
void XMLBenchOutputWriter::startChart(char* chartName, char* chartImage, char* xAxisLabel, char* yAxisLabel, bool xAxisLog, bool yAxisLog)
{
	if (_outputXml){
		const char* xAxisLogString = "false";
		const char* yAxisLogString = "false";
		if (xAxisLog == true){
			xAxisLogString = "true";
		}
		if (yAxisLog == true){
			yAxisLogString = "true";
		}

		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"      <chart chartName=\"%s\" imageName=\"%s\" XAxisLabel=\"%s\" YAxisLabel=\"%s\" XAxisLog=\"%s\" YAxisLog=\"%s\">\n",
				chartName,chartImage,xAxisLabel,yAxisLabel,xAxisLogString,yAxisLogString);
	}
};

/**
 * This writes out the xml to end a chart
 */
void XMLBenchOutputWriter::endChart()
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"      </chart>\n");
	}
};

/**
 * This writes out the xml for a dataset selector
 * 
 * @param baseId the base id for the dataset to be included in the chart
 * @param extendedId the extended id for the dataset to be included in the chart, a value of 0 matches all extendedId values
 * @param datasetLabel the label to be shown in the chart for the dataset 
 */
void XMLBenchOutputWriter::writeDataSetSelector(U_32 baseId, I_32 extendedId, char* datasetLabel)
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"         <datasetSelector baseId=\"%d\" extendedId=\"%d\" datasetLabel=\"%s\" />\n",
				baseId, extendedId, datasetLabel);
	}
};

/**
 * This writes out the xml to start the datasets 
 */
void XMLBenchOutputWriter::startDataSets()
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"   <datasets>\n");
	}
};

/**
 * This writes out the xml to end the datasets
 */
void XMLBenchOutputWriter::endDataSets()
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"   </datasets>\n");
	}
};

/**
 * This writes out the xml to start a dataset
 * 
 * @param baseId the base Id to be associated with the dataset
 * @param extendedID the extended Id to be associated with the dataset.  This must be >0
 * @param extendedLabel the label to be added to the base name for the dataset when selected based on the extended id
 */
void XMLBenchOutputWriter::startDataSet(U_32 baseId, U_32 extendedID, char* extendedLabel)
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"      <dataset baseId=\"%d\" extendedId=\"%d\" extendedLabel=\"%s\">\n",baseId, extendedID,extendedLabel);
	}
};

/**
 * This writes out the xml to end a dataset 
 */
void XMLBenchOutputWriter::endDataSet()
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"      </dataset>\n");
	}
};

/**
 * This writes out a datapoint which is part of a dataset
 * @param xvalue the x value for the datapoint
 * @param yvalue the y value for the datapoint
 */
void XMLBenchOutputWriter::writeDataPoint(float xvalue, float yvalue)
{
	if (_outputXml){
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB,"         <datapoint xvalue=\"%f\" yvalue=\"%f\" />\n",xvalue,yvalue);
	}
};
