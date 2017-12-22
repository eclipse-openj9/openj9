/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands;

import java.io.PrintStream;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.jvm.dtfjview.Session;
import com.ibm.jvm.dtfjview.SessionProperties;
import com.ibm.jvm.dtfjview.spi.ICombinedContext;
import com.ibm.jvm.dtfjview.spi.ISession;
import com.ibm.jvm.dtfjview.spi.ISessionContextManager;

public class CloseCommand extends BaseJdmpviewCommand {
	private static final String CMD_NAME = "close";
	
	{
		addCommand(CMD_NAME, "[context id]","closes the connection to a core file");
	}

	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		Object obj = ctx.getProperties().get(SessionProperties.SESSION_PROPERTY);
		if(obj == null) {
			logger.fine("Could not close a new context as the session property has not been set");
			return;
		}
		if(!(obj instanceof Session)) {
			logger.fine("Could not close an existing context as the session type was not recognised [" + obj.getClass().getName() + "]");
			return;
		}
		ISessionContextManager mgr = ((ISession)obj).getContextManager();
		switch(args.length) {
			case 0 :		//close all contexts
				out.println("Closing all contexts");
				mgr.removeAllContexts();
				out.println("All contexts have been closed");
				break;
			case 1 :		//close all images shared with the specified context
				out.println("Closing all contexts sharing the same Image");
				int id = Integer.valueOf(args[0]);
				ICombinedContext toclose = mgr.getContext(id);
				if(toclose == null) {
					out.println("The specified context id " + args[0] + " was invalid");
					return;
				}
				mgr.removeContexts(toclose.getImage().getSource());
				out.println("Closed all contexts created from " + toclose.getImage().getSource());
				break;
			default :
				out.println("This command takes 0 or 1 parameters. Execute 'close help' for more information");
				return;
		}
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("Closes the connection to a image source. The connection to be closed is determined by the current context, " +
				"which means that more than one context may be removed depending on how many were created against the original source.");
	}

}
