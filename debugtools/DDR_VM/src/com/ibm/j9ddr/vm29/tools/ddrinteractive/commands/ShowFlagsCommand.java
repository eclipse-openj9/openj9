/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;
import java.util.Collection;

import com.ibm.j9ddr.StructureReader.ConstantDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

public class ShowFlagsCommand extends Command {
    
    public ShowFlagsCommand() {
        addCommand("showflags", "[flags structure] [specific flag]" , "Retreive the specified flags and its value");
    }

    public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
        if (args.length > 2) {
            out.println("showflags expects 2 arguments or less");
            return;
        }

        Collection<StructureDescriptor> currentStructures = context.vmData.getStructures();

        //If no arguments are specified, output all the structures
        if (args.length == 0) {
            out.println("Possible flags:");
            for (StructureDescriptor i : currentStructures) {
                if (i.getName().contains("flags") || i.getName().contains("Flags")) {
                    out.println(i);
                }
            }
        }

        //Output all the constants of the flags structure 
        if (args.length > 0) {
            StructureDescriptor currentStruct = null;
            ConstantDescriptor currentFlag = null;
            for (StructureDescriptor i : currentStructures) {
                if (i.getName().equals(args[0])) {
                    currentStruct = i;
                    if (args.length == 2) {
                        for (ConstantDescriptor j : i.getConstants()) { 
                            if (j.getName().equals(args[1])) {
                                currentFlag = j;
                                out.println(j);
                            }
                        }
                        if (currentFlag == null) {
                            out.println("That flag does not exist");
                        }
                    } else {
                        for (ConstantDescriptor j : i.getConstants()) {
                            out.println(j);
                        }
                    }
                }
            }
            if (currentStruct == null) { 
                out.println("That structure does not exist");
            }
        }
    }
}
