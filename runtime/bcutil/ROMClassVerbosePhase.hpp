/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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


#ifndef ROMCLASSVERBOSEPHASE_HPP_
#define ROMCLASSVERBOSEPHASE_HPP_

/* @ddr_namespace: default */
#include "BuildResult.hpp"
#include "ROMClassCreationContext.hpp"
#include "ROMClassCreationPhase.hpp"

/*
 * ROMClassVerbosePhase is a helper class for -verbose:romclass reporting.
 * Instances are intended to be stack allocated and their lifetimes correspond to phases of ROM class creation.
 */
class ROMClassVerbosePhase
{
public:
	ROMClassVerbosePhase(ROMClassCreationContext *context, ROMClassCreationPhase phase) :
		_context(context),
		_phase(phase),
		_result(NULL)
	{
		_context->recordPhaseStart(_phase);
	}

	ROMClassVerbosePhase(ROMClassCreationContext *context, ROMClassCreationPhase phase, BuildResult *result) :
		_context(context),
		_phase(phase),
		_result(result)
	{
		_context->recordPhaseStart(_phase);
	}

	~ROMClassVerbosePhase()
	{
		_context->recordPhaseEnd(_phase, NULL == _result ? OK : *_result);
	}

private:
	ROMClassCreationContext *_context;
	ROMClassCreationPhase _phase;
	BuildResult *_result;
};

/*
 * Set RECORD_HOT_PHASES to 1 to enable recording of hot verbose phases, for detailed
 * ROM class performance analysis at the cost of extra overhead.
 */
#define RECORD_HOT_PHASES 0

#if RECORD_HOT_PHASES
	#define ROMCLASS_VERBOSE_PHASE_HOT(context, phase) ROMClassVerbosePhase v(context, phase)
#else
	#define ROMCLASS_VERBOSE_PHASE_HOT(context, phase)
#endif

#endif /* ROMCLASSVERBOSEPHASE_HPP_ */
