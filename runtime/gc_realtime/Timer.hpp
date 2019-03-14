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

#if !defined(TIMER_HPP_)
#define TIMER_HPP_

#include "Metronome.hpp"

class MM_EnvironmentBase;
class MM_OSInterface;

class MM_Timer : public MM_BaseVirtual
{
/* Data members & types */
public:
protected:
private:
	U_64 _tickBase; /**< Current tick count from the TSC */
	U_64 _sytemTimeBase; /**< Current system time in nanoseconds */
	MM_OSInterface* _osInterface; /**< OS Interface used to set the time base on reset/initialize. */
	
/* Methods */
public:
	static MM_Timer* newInstance(MM_EnvironmentBase* env, MM_OSInterface* osInterface);
	virtual void kill(MM_EnvironmentBase* env);
	
	U_64 peekElapsedTime(U_64 base);
	U_64 getTimeInNanos();
	U_64 nanoTime();
	void reset();
	bool hasTimeElapsed(U_64 startTimeInNanos, U_64 timeToWaitInNanos);
	
protected:
	bool initialize(MM_EnvironmentBase* env, MM_OSInterface* osInterface);
	void tearDown(MM_EnvironmentBase* env);
private:
	U_64 rebaseTime();
	
	MM_Timer() :
		_tickBase(J9CONST64(0)),
		_sytemTimeBase(J9CONST64(0)),
		_osInterface(NULL)
	{
		_typeId = __FUNCTION__;
	}
	
};

#endif /*TIMER_HPP_*/
