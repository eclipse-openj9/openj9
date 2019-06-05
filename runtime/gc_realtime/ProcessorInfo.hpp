/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

/**
 * @file
 * @ingroup GC_Metronome
 */

#if !defined(PROCESSORINFO_HPP_)
#define PROCESSORINFO_HPP_

#include "BaseVirtual.hpp"
#include "GCExtensionsBase.hpp"

class MM_EnvironmentBase;
class MM_OSInterface;

/**
 * @todo Provide class documentation
 * @ingroup GC_Metronome
 */
class MM_ProcessorInfo : public MM_BaseVirtual
{
public:

	MM_ProcessorInfo() :
		_freq(0)
	{
		_typeId = __FUNCTION__;
	}
	
	static MM_ProcessorInfo *newInstance(MM_EnvironmentBase *env);
	bool initialize(MM_EnvironmentBase *env);

	virtual void kill(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	double _freq;   /**< The measured frequency of this processor */

	double tickToSecond(U_64 t);
	U_64 secondToTick(double s); /**< Users should note that double's have less precision than U_64 */

private:
	double readFrequency();
};


#endif /* PROCESSORINFO_HPP_ */
