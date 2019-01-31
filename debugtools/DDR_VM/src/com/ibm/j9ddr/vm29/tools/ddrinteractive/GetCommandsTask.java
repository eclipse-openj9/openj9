/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.util.List;

import com.ibm.j9ddr.IBootstrapRunnable;
import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.tools.ddrinteractive.BaseJVMCommands;
import com.ibm.j9ddr.tools.ddrinteractive.ICommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.ACCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.AllClassesCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.ITableSizeCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.AnalyseRomClassUTF8Command;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.BuildFlagsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.BytecodesCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.ClassForNameCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.ClassloadersSummaryCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.CompressedRefMappingCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.CoreInfoCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpAllClassesInModuleCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.CPDescriptionCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpAllClassloadersCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpAllRamClassLinearCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpAllRegionsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpAllRomClassLinearCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpAllSegmentsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpContendedLoadTable;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpModuleCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpModuleReadsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpModuleExportsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpModuleDirectedExportsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpPackageCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpRamClassLinearCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpRomClassCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpRomClassLinearCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpRomMethodCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpSegmentsInListCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpSegregatedStatsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.DumpStringTableCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.ExtendedMethodFlagInfoCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.FindAllModulesCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.FindAllReadsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.FindMethodFromPcCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.FindModuleByNameCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.FindModulesCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.FindOverlappingSegmentsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.FindPatternCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.FindStackValueCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.FindVMCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.GCCheckCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.HashCodeCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.J9ClassShapeCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.J9MemTagCommands;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.J9RegCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.J9StaticsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.J9VTablesCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.JitMetadataFromPcCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.JitstackCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.LocalMapCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.MarkMapCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.MethodForNameCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.MonitorsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.NativeMemInfoCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.ObjectSizeInfo;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.QueryRomClassCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.RamClassSummaryCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.RomClassForNameCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.RomClassSummaryCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.RootPathCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.RuntimeSettingsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.SearchStringTableCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.SetVMCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.ShowDumpAgentsCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.ShrCCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.StackWalkCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.StackmapCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.TraceConfigCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.VMConstantPoolCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.VmCheckCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.WalkInternTableCommand;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.commands.WalkJ9PoolCommand;

/**
 * @author andhall
 * 
 */
public class GetCommandsTask extends BaseJVMCommands implements IBootstrapRunnable 
{

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.ibm.j9ddr.IBootstrapRunnable#run(com.ibm.j9ddr.IVMData,
	 * java.lang.Object[])
	 */
	public void run(IVMData vmData, Object[] userData) 
	{
		Object[] passbackArray = (Object[]) userData[0];
		Object loader = (Object) passbackArray[1];

		List<ICommand> toPassBack = getBaseJVMCommands();

		toPassBack.add(new StackWalkCommand());
		toPassBack.add(new StructureCommand());
		toPassBack.add(new ThreadsCommand());
		toPassBack.add(new ACCommand());
		toPassBack.add(new AllClassesCommand());
		toPassBack.add(new ITableSizeCommand());
		toPassBack.add(new BuildFlagsCommand());
		toPassBack.add(new WalkInternTableCommand());
		toPassBack.add(new ClassForNameCommand());
		toPassBack.add(new DumpAllClassloadersCommand());
		toPassBack.add(new DumpAllRegionsCommand());
		toPassBack.add(new DumpAllSegmentsCommand());
		toPassBack.add(new FindOverlappingSegmentsCommand());
		toPassBack.add(new FindStackValueCommand());
		toPassBack.add(new VmCheckCommand());
		toPassBack.add(new DumpRomClassCommand());
		toPassBack.add(new WhatIsCommand());
		toPassBack.add(new DumpRamClassLinearCommand());
		toPassBack.add(new DumpRomClassLinearCommand());
		toPassBack.add(new DumpRomMethodCommand());
		toPassBack.add(new DumpAllRomClassLinearCommand());
		toPassBack.add(new DumpAllRamClassLinearCommand());
		toPassBack.add(new VMConstantPoolCommand());
		toPassBack.add(new J9VTablesCommand());
		toPassBack.add(new MethodForNameCommand());
		toPassBack.add(new BytecodesCommand());
		toPassBack.add(new FindPatternCommand());
		toPassBack.add(new JitMetadataFromPcCommand());
		toPassBack.add(new FindMethodFromPcCommand());
		toPassBack.add(new DumpSegmentsInListCommand());
		toPassBack.add(new J9StaticsCommand());
		toPassBack.add(new JitstackCommand());
		toPassBack.add(new LocalMapCommand());
		toPassBack.add(new StackmapCommand());
		toPassBack.add(new J9ClassShapeCommand());
		toPassBack.add(new FindVMCommand());
		toPassBack.add(new ShrCCommand());
		toPassBack.add(new QueryRomClassCommand());
		toPassBack.add(new RamClassSummaryCommand());
		toPassBack.add(new RomClassSummaryCommand());
		toPassBack.add(new ClassloadersSummaryCommand());
		toPassBack.add(new ExtendedMethodFlagInfoCommand());
		toPassBack.add(new AnalyseRomClassUTF8Command());
		toPassBack.add(new J9MemTagCommands());
		toPassBack.add(new CompressedRefMappingCommand());
		toPassBack.add(new ShowDumpAgentsCommand());
		toPassBack.add(new NativeMemInfoCommand());
		toPassBack.add(new SetVMCommand());
		toPassBack.add(new TraceConfigCommand());
		toPassBack.add(new WalkJ9PoolCommand());
		toPassBack.add(new J9RegCommand());
		toPassBack.add(new CoreInfoCommand());
		toPassBack.add(new GCCheckCommand());
		toPassBack.add(new DumpStringTableCommand());
		toPassBack.add(new SearchStringTableCommand());
		toPassBack.add(new RomClassForNameCommand());
		toPassBack.add(new RuntimeSettingsCommand());
		toPassBack.add(new RootPathCommand());
		toPassBack.add(new HashCodeCommand());
		toPassBack.add(new MonitorsCommand());
		toPassBack.add(new MarkMapCommand());
		toPassBack.add(new DumpSegregatedStatsCommand());
		toPassBack.add(new ObjectSizeInfo());
		toPassBack.add(new DumpContendedLoadTable());
		toPassBack.add(new CPDescriptionCommand());
		toPassBack.add(new FindAllModulesCommand());
		toPassBack.add(new FindModuleByNameCommand());
		toPassBack.add(new DumpModuleExportsCommand());
		toPassBack.add(new DumpModuleReadsCommand());
		toPassBack.add(new FindAllReadsCommand());
		toPassBack.add(new DumpModuleDirectedExportsCommand());
		toPassBack.add(new DumpAllClassesInModuleCommand());
		toPassBack.add(new FindModulesCommand());
		toPassBack.add(new DumpModuleCommand());
		toPassBack.add(new DumpPackageCommand());

		loadPlugins(toPassBack, loader);

		passbackArray[0] = toPassBack;
	}

}
