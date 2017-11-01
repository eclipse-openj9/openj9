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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;
import java.util.Collection;
import java.util.Collections;

import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.types.U64;

public class ExtendedMethodFlagInfoCommand extends Command
{
	public ExtendedMethodFlagInfoCommand()
	{
		addCommand("j9extendedmethodflaginfo", "<flags>", "give information about extended method flags");
	}
	
	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException {
		U64 flags = new U64(Long.decode(args[0]));
		
		if (flags.anyBitsIn(J9Consts.J9_RAS_METHOD_UNSEEN)) {
			out.println("J9_RAS_METHOD_UNSEEN");
		}
		if (flags.anyBitsIn(J9Consts.J9_RAS_METHOD_SEEN)) {
			out.println("J9_RAS_METHOD_SEEN");
		}
		if (flags.anyBitsIn(J9Consts.J9_RAS_METHOD_TRACING)) {
			out.println("J9_RAS_METHOD_TRACING");
		}
		if (flags.anyBitsIn(J9Consts.J9_RAS_METHOD_TRACE_ARGS)) {
			out.println("J9_RAS_METHOD_TRACE_ARGS");
		}
		if (flags.anyBitsIn(J9Consts.J9_RAS_METHOD_TRIGGERING)) {
			out.println("J9_RAS_METHOD_TRIGGERING");
		}
	}

}

