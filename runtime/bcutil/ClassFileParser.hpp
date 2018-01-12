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

/*
 * ClassFileParser.hpp
 *
 *  Created on: Aug 24, 2009
 *      Author: rservant
 */

#ifndef CLASSFILEPARSER_HPP_
#define CLASSFILEPARSER_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"
#include "j9.h"

#include "BuildResult.hpp"

typedef  IDATA (*VerifyClassFunction) (J9PortLibrary *aPortLib, J9CfrClassFile* classfile, U_8* segment, U_8* segmentLength, U_8* freePointer, U_32 vmVerisonShifted, U_32 flags, I_32 *hasRET);

class ROMClassCreationContext;

class ClassFileParser
{
public:
	ClassFileParser(J9PortLibrary *portLibrary, VerifyClassFunction verifyClassFunction) :
		_portLibrary(portLibrary),
		_verifyClassFunction(verifyClassFunction),
		_j9CfrClassFile(NULL)
	{
	}

	BuildResult parseClassFile(ROMClassCreationContext *context, UDATA *initialBufferSize, U_8 **classFileBuffer);

	void restoreOriginalMethodBytecodes();

	J9CfrClassFile *getParsedClassFile() { return _j9CfrClassFile; }

private:
	J9PortLibrary * _portLibrary;
	VerifyClassFunction _verifyClassFunction;
	J9CfrClassFile *_j9CfrClassFile;
};
#endif /* CLASSFILEPARSER_HPP_ */
