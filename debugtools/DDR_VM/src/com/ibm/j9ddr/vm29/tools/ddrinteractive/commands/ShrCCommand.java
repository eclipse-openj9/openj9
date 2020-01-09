/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9GenericByID.*;
import static com.ibm.j9ddr.vm29.structure.ShCFlags.*;
import static com.ibm.j9ddr.vm29.structure.ShcdatatypesConstants.*;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.J9ConstantHelper;
import com.ibm.j9ddr.vm29.pointer.*;
import com.ibm.j9ddr.vm29.pointer.generated.*;
import com.ibm.j9ddr.vm29.pointer.helper.*;
import com.ibm.j9ddr.vm29.structure.*;
import com.ibm.j9ddr.vm29.types.*;

public class ShrCCommand extends Command 
{

	private static final long ORPHAN_STATS = 1;
	private static final long ROMCLASS_STATS = 2;
	private static final long CLASSPATH_STATS = 4;
	private static final long AOT_STATS = 8;
	private static final long SCOPE_STATS = 0x10L;
	private static final long BYTE_STATS = 0x20L;
	private static final long UNINDEXED_BYTE_STATS = 0x40L;
	private static final long INV_AOT_STATS = 0x80L;
	private static final long CACHELET_STATS = 0x100L;
	private static final long PREREQ_CACHE_STATS = 0x200L;
	private static final long STARTUPHINT_STATS = 0x400L;
	private static final long CRV_SNIPPET_STATS = 0x800L;
	private static final long FIND_METHOD = 0x1000L;
	private static final long JITPROFILE_STATS = 0x2000L;
	private static final long JITHINT_STATS = 0x4000L;
	private static final long ALL_STALE_STATS = 0x8000L;
	private static final long ALL_STATS = ORPHAN_STATS | ROMCLASS_STATS
			| CLASSPATH_STATS | AOT_STATS | SCOPE_STATS | BYTE_STATS
			| UNINDEXED_BYTE_STATS | INV_AOT_STATS | CACHELET_STATS | PREREQ_CACHE_STATS
			| JITPROFILE_STATS | JITHINT_STATS | ALL_STALE_STATS;
	private static final int J9SHR_ATTACHED_DATA_TYPE_JITPROFILE = 1;
	private static final int J9SHR_ATTACHED_DATA_TYPE_JITHINT = 2;
	
	private static final String rangeDelim = "..";
	private static long cacheTotalSize = 0;
	private static final long TYPE_PREREQ_CACHE = J9ConstantHelper.getLong(ShcdatatypesConstants.class, "TYPE_PREREQ_CACHE", -1);
	private static final long J9SHR_DATA_TYPE_STARTUP_HINTS = J9ConstantHelper.getLong(ShCFlags.class, "J9SHR_DATA_TYPE_STARTUP_HINTS", -1);
	private static final long J9SHR_DATA_TYPE_CRVSNIPPET = J9ConstantHelper.getLong(ShCFlags.class, "J9SHR_DATA_TYPE_CRVSNIPPET", -1);


	public ShrCCommand()
	{
		addCommand("shrc", "[command]", "shared class cache operations");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			if (!J9BuildFlags.opt_sharedClasses) {
				CommandUtils.dbgPrint(out, "no shared cache\n");
				return;
			}
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());

			J9SharedClassConfigPointer sharedClassConfig = vm.sharedClassConfig();
			CommandUtils.dbgPrint(out, "!j9sharedclassconfig %s\n\n", sharedClassConfig.getHexAddress());
			
			if (args.length == 0) {
				printHelp(out);
			} else if (sharedClassConfig.notNull()) {
				U8Pointer[] metaStartInCache = getSharedCacheMetadataStart(vm, out);
				U8Pointer[] metaEndInCache = getSharedCacheMetadataEnd(vm, out);
				U8Pointer[] metaStart = new U8Pointer[1];
				U8Pointer[] metaEnd = new U8Pointer[1];
				boolean userSpecRange = false;
				int layer = metaStartInCache.length - 1;
				
				initTotalCacheSize(out, sharedClassConfig);
				/* check if the first parameter specifies the range of metadata region to be used */
				if (args.length > 1) {
					/* Presence of '..' indicates user specified the range */
					if (args[1].indexOf(rangeDelim) != -1) {
						String addr;
						
						addr = args[1].substring(0, args[1].indexOf(rangeDelim));
						metaStart[0] = U8Pointer.cast(CommandUtils.parsePointer(addr, J9BuildFlags.env_data64));
						
						addr = args[1].substring(args[1].indexOf(rangeDelim) + rangeDelim.length());
						metaEnd[0] = U8Pointer.cast(CommandUtils.parsePointer(addr, J9BuildFlags.env_data64));
						userSpecRange = true;
					} else if (args[1].indexOf("layer=") != -1) {
						int topLayer = dbgShrcCacheTopLayer(out, sharedClassConfig);
						if (-1 == topLayer) {
							CommandUtils.dbgPrint(out, "This cache does not have layers\n");
							return;
						}
						Pattern p = Pattern.compile("(.*)layer=(\\d+)$");
						Matcher m = p.matcher(args[1]);
						if (m.matches()) {
							String layerNumberString = m.group(2);
							layer = Integer.parseInt(layerNumberString);
							if (layer >= metaStartInCache.length) {
								CommandUtils.dbgPrint(out, "User specified layer number %d is invalid, the maximum layer number is %d\n", layer, metaStartInCache.length - 1);
								return;
							}
							metaStart[0] = U8Pointer.cast(metaStartInCache[layer]);
							metaEnd[0] = U8Pointer.cast(metaEndInCache[layer]);
							userSpecRange = true;
						}
					}
					if (userSpecRange) {
						if (metaEnd[0].lt(metaStart[0])) {
							CommandUtils.dbgPrint(out, "User specified metadata region boundary is not valid. Ensure 'end' >= 'start'\n");
							return;
						}
					
						int index1 = getArrayIndexForMetadataAddress(metaStart[0], metaStartInCache, metaEndInCache);
						int index2 = getArrayIndexForMetadataAddress(metaEnd[0], metaStartInCache, metaEndInCache);

						if ((-1 == index1) && (-1 == index2)) {
							CommandUtils.dbgPrint(out, "User specified metadata region boundary is not valid. Neither of the addresses is in the shared cache metadata region.\n");
							return;
						} else if (-1 == index1) {
							CommandUtils.dbgPrint(out, "User specified start boundary is not in the shared cache metadata region, replacing the start boundary to the start address of metadata region of layer %d\n", index2);
							metaStart[0] = metaStartInCache[index2];
						} else if (-1 == index2) {
							CommandUtils.dbgPrint(out, "User specified end boundary is not in the shared cache metadata region, replacing end boundary to the end address of metadata region of layer %d\n", index1);
							metaEnd[0] = metaEndInCache[index1];
						} else if (index1 != index2) {
							CommandUtils.dbgPrint(out, "User specified metadata region boundary is not valid. They should be in the same layer. The start address is in layer %d, but the end address is in layer %d\n", index1, index2);
							return;
						}
					}
				} 
				if (!userSpecRange) {
					metaStart = metaStartInCache;
					metaEnd = metaEndInCache;
				}
				if (args[0].equals("allstats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, ALL_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("rcstats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, ROMCLASS_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("cpstats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, CLASSPATH_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("aotstats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, AOT_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("invaotstats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, INV_AOT_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("orphanstats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, ORPHAN_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("scopestats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, SCOPE_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("bytestats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, BYTE_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("ubytestats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, UNINDEXED_BYTE_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("startuphint")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, STARTUPHINT_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("crvsnippetstats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, CRV_SNIPPET_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("clstats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, CACHELET_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("preqstats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, PREREQ_CACHE_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("stalestats")) {
					dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, ALL_STALE_STATS, null, false, VoidPointer.NULL, false);
				} else if (args[0].equals("classpath")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc classpath <address>\n");
					} else {
						long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
						
						dbgShrcPrintClasspath(out, ClasspathWrapperPointer.cast(address));
					}
				} else if (args[0].equals("findclass")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc findclass <name>\n");
					} else {
						String className = args[1];
						CommandUtils.dbgPrint(out, "Looking for class \"%s\"\n", className);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, ROMCLASS_STATS | ORPHAN_STATS, className, false, VoidPointer.NULL, false);
					}
				} else if (args[0].equals("findclassp")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc findclassp <name>\n");
					} else {
						String className = args[1];
						CommandUtils.dbgPrint(out, "Looking for class prefix \"%s\"\n", className);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, ROMCLASS_STATS | ORPHAN_STATS, className, true, VoidPointer.NULL, false);
					}
				} else if (args[0].equals("findaot")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc findaot <name>\n");
					} else {
						String methodName = args[1];
						CommandUtils.dbgPrint(out, "Looking for AOT method \"%s\"\n", methodName);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, AOT_STATS | INV_AOT_STATS, methodName, false, VoidPointer.NULL, false);
					}
				} else if (args[0].equals("findaotp")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc findaot <name>\n");
					} else {
						String methodName = args[1];
						CommandUtils.dbgPrint(out, "Looking for AOT method prefix \"%s\"\n", methodName);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, AOT_STATS | INV_AOT_STATS, methodName, true, VoidPointer.NULL, false);
					}
				} else if (args[0].equals("aotfor")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc aotfor <address>\n");
					} else {
						long addr = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
	
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, AOT_STATS | INV_AOT_STATS, null, true, VoidPointer.cast(addr), false);
					}
				} else if (args[0].equals("rcfor")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc rcfor <address>\n");
					} else {
						long addr = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
	
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, ROMCLASS_STATS | ORPHAN_STATS, null, false, VoidPointer.cast(addr), false);
					}
				} else if (args[0].equals("incache")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc incache <address>\n");
					} else {
						long addr = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
	
						dbgShrcInCache(out, vm, sharedClassConfig, VoidPointer.cast(addr));
					}
				} else if (args[0].equals("stats")) {
					if (sharedClassConfig.notNull()) {
						dbgShrcInCache(out, vm, sharedClassConfig, VoidPointer.NULL);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, 0, null, false, VoidPointer.NULL, false);
					}
				} else if (args[0].equals("method")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc method <address>\n");
					} else {
						long addr = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
	
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, FIND_METHOD, null, false, VoidPointer.cast(addr), false);
					}
				} else if (args[0].equals("cachelet")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc incache <address>\n");
					} else {
						long addr = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
						dbgShrcPrintCachelet(out, CacheletWrapperPointer.cast(addr));
					}
					
				/**
				 * JIT profiling data
				 */
				} else if (args[0].equals("jitpstats")) {
					if (sharedClassConfig.notNull()) {
						dbgShrcInCache(out, vm, sharedClassConfig, VoidPointer.NULL);
						boolean corrupt = (args.length > 1) && "corrupt".equals(args[1]);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, JITPROFILE_STATS, null, false, VoidPointer.NULL, corrupt);
					}
				} else if (args[0].equals("findjitp")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc findjitp <name>\n");
					} else {
						String methodName = args[1];
						CommandUtils.dbgPrint(out, "Looking for JIT PROFILE method \"%s\"\n", methodName);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, JITPROFILE_STATS, methodName, false, VoidPointer.NULL, false);
					}
				} else if (args[0].equals("findjitpp")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc findjitpp <name>\n");
					} else {
						String methodName = args[1];
						CommandUtils.dbgPrint(out, "Looking for JIT PROFILE method prefix \"%s\"\n", methodName);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, JITPROFILE_STATS, methodName, true, VoidPointer.NULL, false);
					}
				} else if (args[0].equals("jitpfor")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc jitpfor <address>\n");
					} else {
						long addr = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
	
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, JITPROFILE_STATS, null, false, VoidPointer.cast(addr), false);
					}
				/**
				 * JIT hints
				 */
				} else if (args[0].equals("jithstats")) {
					if (sharedClassConfig.notNull()) {
						dbgShrcInCache(out, vm, sharedClassConfig, VoidPointer.NULL);
						boolean corrupt = (args.length > 1) && "corrupt".equals(args[1]);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStart, metaEnd, JITHINT_STATS, null, false, VoidPointer.NULL, corrupt);
					}
				} else if (args[0].equals("findjith")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc findjith <name>\n");
					} else {
						String methodName = args[1];
						CommandUtils.dbgPrint(out, "Looking for JIT HINT method \"%s\"\n", methodName);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, JITHINT_STATS, methodName, false, VoidPointer.NULL, false);
					}
				} else if (args[0].equals("findjithp")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc findjithp <name>\n");
					} else {
						String methodName = args[1];
						CommandUtils.dbgPrint(out, "Looking for JIT HINT method prefix \"%s\"\n", methodName);
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, JITHINT_STATS, methodName, true, VoidPointer.NULL, false);
					}
				} else if (args[0].equals("jithfor")) {
					if (args.length != 2) {
						CommandUtils.dbgPrint(out, "Usage: !shrc jithfor <address>\n");
					} else {
						long addr = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);		
						dbgShrcPrintAllStats(out, vm, sharedClassConfig, metaStartInCache, metaEndInCache, JITHINT_STATS, null, false, VoidPointer.cast(addr), false);
					}			

				} else if (args[0].equals("rtflags")) {
					if (sharedClassConfig.notNull()) {
						UDATA runtimeFlags = sharedClassConfig.runtimeFlags();
						CommandUtils.dbgPrint(out,"Printing the shared classes runtime flags %s\n", runtimeFlags.getHexValue());
						printShCFlags(out, runtimeFlags, "RUNTIMEFLAG");
					}
				} else if (args[0].equals("extraflags")) {
					if (sharedClassConfig.notNull()) {
						ShrcConfig config = dbgShrcReadConfig(sharedClassConfig, out);
						J9SharedCacheHeaderPointer[] headers = config.getCacheStartAddress();
						UDATA extraFlags = headers[layer].extraFlags();
						CommandUtils.dbgPrint(out,"Printing the shared classes extra flags present in cache header %s for layer %d\n", extraFlags.getHexValue(), layer);
						printShCFlags(out, extraFlags, "EXTRA_FLAGS");
					}	
				} else if (args[0].equals("write")) {
					if (args.length != 2 && args.length != 3) {
						CommandUtils.dbgPrint(out, "Usage: !shrc write <cachedir> [<cachename>]\n");
					} else {
						String cacheDir = args[1];
						String cacheName = args.length == 3 ? args[2] : null; 
						try {
							dbgShrcWriteCache(out, sharedClassConfig, cacheDir, cacheName);
						} catch (CorruptDataException e) {
							/* retry writing only populated areas of cache */
							CommandUtils.dbgPrint(out, "Unable to write complete cache: %s", e.getMessage());
							CommandUtils.dbgPrint(out, "Attempting to write only populated areas of cache.\n");
							dbgShrcWriteCacheByAreas(out, sharedClassConfig, cacheDir, cacheName);
						}
					}
				} else if (args[0].equals("name")) {
					dbgShrcCacheName(out, sharedClassConfig);
				} else {
					CommandUtils.dbgPrint(out, "Unknown arg(s) : ");
					for (int i = 0; i< args.length; i++) {
						CommandUtils.dbgPrint(out, args[i] + " ");
					}
					CommandUtils.dbgPrint(out, "\nType !shrc to see all the valid options.\n" );
				}
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	/**
	 * Find which region a metadata address belongs among metadata regions specified by metaStartArray and metaEndArray
	 * @param A metadata address
	 * @param metaStartArray - An array specifying the start to each metadata region
	 * @param metaEndArray - An array specifying the end of each metadata region
	 * @return The index of the region the metadata address belongs to. -1 otherwise.
	 */
	private static int getArrayIndexForMetadataAddress(U8Pointer address, U8Pointer[] metaStartArray, U8Pointer[] metaEndArray) {
		int ret = -1;
		for (int i = 0; i < metaStartArray.length; i++) {
			if (address.gte(metaStartArray[i]) && address.lte(metaEndArray[i])) {
				ret = i;
				break;
			}
		}
		return ret;
	}
	
	private void printShCFlags(PrintStream out, UScalar flags, String type) {
		Field[] ShCFlagsfields = ShCFlags.class.getFields();
		for (Field field : ShCFlagsfields) {
			String flagName = field.getName();
			try {
				Long flagValue = (Long)field.get(null);
				if (flagName.contains(type)) {
					if (flags.anyBitsIn(flagValue)) {
						if (flags.sizeof() == 8) {
							CommandUtils.dbgPrint(out,"%-65s 0x%016X\n", flagName, flagValue);
						} else {
							CommandUtils.dbgPrint(out,"%-65s 0x%08X\n", flagName, flagValue);
						}
					}
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}

	private void dbgShrcCacheName(PrintStream out, J9SharedClassConfigPointer sharedClassConfig) throws CorruptDataException {
		SH_OSCachePointer osCache = getOSCache(out, sharedClassConfig);
		if (osCache.notNull()) {
			U8Pointer cacheNamePointer = osCache._cacheNameWithVGen();
			U8Pointer cachePathNamePointer = osCache._cachePathName();
			if (cacheNamePointer.notNull()) {
				String cacheNameString = cacheNamePointer.getCStringAtOffset(0);
				CommandUtils.dbgPrint(out, "Cache name is %s\n", cacheNameString);
			}
			if (cachePathNamePointer.notNull()) {
				String cachePathNameString = cachePathNamePointer.getCStringAtOffset(0);
				CommandUtils.dbgPrint(out, "Cache path is %s\n", cachePathNameString);
			}
		}
	}

	private int dbgShrcCacheTopLayer(PrintStream out, J9SharedClassConfigPointer sharedClassConfig) throws CorruptDataException {
		SH_OSCachePointer osCache = getOSCache(out, sharedClassConfig);
		int topLayer = 0;
		if (osCache.notNull()) {
			U8Pointer cacheNamePointer = osCache._cacheNameWithVGen();
			if (cacheNamePointer.notNull()) {
				String cacheNameString = cacheNamePointer.getCStringAtOffset(0);
				Pattern p = Pattern.compile(".*L(\\d\\d)$");
				Matcher m = p.matcher(cacheNameString);
				if (m.matches()) {
					String layerNumberString = m.group(1);
					topLayer = Integer.parseInt(layerNumberString);
				} else {
					topLayer = -1;
				}
			}
		}
		return topLayer;
	}

	/**
	 * Convenience class based on FileOutputStream that holds additionally the absolute path name of the file
	 * the output stream is associated/created with.
	 *
	 */
	static class CacheFileOutputStream extends FileOutputStream {
		private String fileName;
		
		CacheFileOutputStream(File f) throws FileNotFoundException{
			super(f);
			this.fileName = f.getAbsolutePath();
		}
		
		String getFileName() {
			return this.fileName;
		}
	}
	
	/**
	 * Method creates the cache file with the name and in the directory passed in as parameters.
	 * @param out - the PrintStream used to write progress and/or errors out to console
	 * @param sharedClassConfig - the generated J9SharedClassConfigPointer used to get cache header information
	 * @param cacheDir - the directory in which to write the cache file
	 * @param cacheName - the name given to the newly written cache file (optional)
	 * @return the CacheFileOutputStream class to use to write to the file
	 */
	CacheFileOutputStream createDbgShrcCacheFile(PrintStream out, J9SharedClassConfigPointer sharedClassConfig, String cacheDir, String cacheName) throws CorruptDataException{
		CacheFileOutputStream fout = null;
		SH_OSCachePointer osCache = getOSCache(out, sharedClassConfig);
		if (osCache.notNull()) {
			String cacheNameString = "default";
			U8Pointer cacheNamePointer = osCache._cacheNameWithVGen();
			if (cacheNamePointer.notNull()) {
				cacheNameString = cacheNamePointer.getCStringAtOffset(0);
				CommandUtils.dbgPrint(out, "Cache name is %s\n", cacheNameString);
			}
			if (cacheName != null) cacheNameString = cacheName; 
			VoidPointer headerStart = osCache._headerStart();
			UDATA cacheSize = osCache._cacheSize();
			CommandUtils.dbgPrint(out, "Cache start 0x%x size %d\n", headerStart.getAddress(), cacheSize.longValue());
			File outFile = new File(cacheDir, cacheNameString);
			CommandUtils.dbgPrint(out, "Writing cache to %s\n", outFile.getAbsolutePath());
			
			try {
				fout = new CacheFileOutputStream(outFile);
			} catch (FileNotFoundException e) {
				CommandUtils.dbgPrint(out, "Could not create %s: %s", outFile.getAbsolutePath(), e.getMessage());
			}
		}
		return fout;
	}
	
	/**
	 * Method attempts to write entire shared classes cache from start to finish, including any gaps between segment areas.
	 * 
	 * @param out - the PrintStream used to write progress and/or errors out to console
	 * @param sharedClassConfig - the generated J9SharedClassConfigPointer used to get cache header information
	 * @param cacheDir - the directory in which to write the cache file
	 * @param cacheName - the name given to the newly written cache file (optional)
	 */
	private void dbgShrcWriteCache(PrintStream out,	J9SharedClassConfigPointer sharedClassConfig, String cacheDir, String cacheName) throws CorruptDataException {
		CacheFileOutputStream fout = createDbgShrcCacheFile(out, sharedClassConfig, cacheDir, cacheName);
		ShrcConfig config = dbgShrcReadConfig(sharedClassConfig, out);
		J9SharedCacheHeaderPointer[] headers = config.getCacheStartAddress();
		for (J9SharedCacheHeaderPointer header : headers) {
			J9SharedCacheHeaderInfo helper = new J9SharedCacheHeaderInfo(header);
			try {
				/* write entire cache area */
				dbgShrcWriteCacheArea(fout, helper.getHeaderStart(), helper.getDebugAreaEnd(), false);
			} catch (IOException e) {
				CommandUtils.dbgPrint(out, "Error writing %s: %s\n", fout.getFileName(), e.getMessage());
			} finally {
				try {
					fout.close();
				} catch (IOException e) {
					CommandUtils.dbgPrint(out, "Error closing %s: %s\n", fout.getFileName(), e.getMessage());
				}
			}
		}
	}
	
	/** 
	 * Method writes the shared classes cache in segments taking care to zero out the gaps between the areas.
	 * 
	 * @param out - the PrintStream used to write progress and/or errors out to console
	 * @param sharedClassConfig - the generated J9SharedClassConfigPointer used to get cache header information
	 * @param cacheDir - the directory in which to write the cache file
	 * @param cacheName - the name given to the newly written cache file (optional)
	 */
	private void dbgShrcWriteCacheByAreas(PrintStream out, J9SharedClassConfigPointer sharedClassConfig, String cacheDir, String cacheName) throws CorruptDataException {
		CacheFileOutputStream fout = createDbgShrcCacheFile(out, sharedClassConfig, cacheDir, cacheName);
		ShrcConfig config = dbgShrcReadConfig(sharedClassConfig, out);
		J9SharedCacheHeaderPointer[] header = config.getCacheStartAddress();
		for (J9SharedCacheHeaderPointer headerIter : header) {
			J9SharedCacheHeaderInfo helper = new J9SharedCacheHeaderInfo(headerIter);
			try {
				/* write the first 3 areas in cache (header, read/write and classes) */
				dbgShrcWriteCacheArea(fout, helper.getHeaderStart(), helper.getRomClassesEnd(), false);
					
				/* zero the gap between classes and metadata */
				dbgShrcWriteCacheArea(fout, helper.getRomClassesEnd(), helper.getMetaDataEnd(), true);
					
				/* write the metadata area, note: metadata is written out right to left */
				dbgShrcWriteCacheArea(fout, helper.getMetaDataEnd(), helper.getMetaDataStart(), false);
					
				/* write the line number (LN) area */
				dbgShrcWriteCacheArea(fout, helper.getLineNumberAreaStart(), helper.getLineNumberAreaEnd(), false);
					
				/* zero the gap between LN and LV areas */
				dbgShrcWriteCacheArea(fout, helper.getLineNumberAreaEnd(), helper.getLocalVariableAreaEnd(), true);
					
				/* write the local variable (LV) area, note: LV area written out right to left */
				dbgShrcWriteCacheArea(fout, helper.getLocalVariableAreaEnd(), helper.getLocalVariableAreaStart(), false);
					
				CommandUtils.dbgPrint(out, "Cache successfully written to %s\n", fout.getFileName());
			} catch (IOException e) {
				CommandUtils.dbgPrint(out, "Error writing %s: %s\n", fout.getFileName(), e.getMessage());
			} finally {
				try {
					fout.close();
				} catch (IOException e) {				
				}
			}
		}
	}
	
	/**
	 * Utility method used to write to the newly created cache file
	 * 
	 * @param fout - FileOutputStream used to write to the file
	 * @param areaStart - start of the area of cache to write
	 * @param areaEnd - end of the area of cache to write
	 * @param zeroFill - boolean to determine to write zeros to the buffer
	 */
	private void dbgShrcWriteCacheArea(FileOutputStream fout, UDATA areaStart, UDATA areaEnd, boolean zeroFill) throws CorruptDataException, IOException {	
			long offset = 0;
			long start = areaStart.longValue();
			long end = areaEnd.longValue();
			long areaSize = end - start;
			SH_OSCachePointer hStart =  SH_OSCachePointer.cast(start);
			byte[] buffer = new byte[4096];
			if (zeroFill) {
				java.util.Arrays.fill(buffer, (byte) 0x00);
			}
			while (offset < areaSize) {
				if (areaSize - offset < buffer.length) {
					buffer = new byte[(int)(areaSize - offset)];
					if (zeroFill) {
						java.util.Arrays.fill(buffer, (byte) 0x00);
					}
				}			
				if (!zeroFill) {
					hStart.getBytesAtOffset(offset, buffer);
				}
				fout.write(buffer);
				offset += buffer.length;
			}		
	}

	private SH_OSCachePointer getOSCache(PrintStream out, J9SharedClassConfigPointer sharedClassConfig) throws CorruptDataException {
		if (sharedClassConfig.notNull()) {
			StructurePointer structPointer = sharedClassConfig.sharedClassCache();
			if (structPointer.notNull()) {
				long pointer = structPointer.getAddress();
				SH_CacheMapPointer cacheMap = SH_CacheMapPointer.cast(pointer);
				SH_CompositeCacheImplPointer compositeCache = cacheMap._cc();
				if (compositeCache.notNull()) {
					SH_OSCachePointer osCache = compositeCache._oscache();
					if (osCache.notNull()) {
						return osCache;
					} else {
						CommandUtils.dbgPrint(out, "SH_OSCache is NULL\n");
					}
				} else {
					CommandUtils.dbgPrint(out, "SH_CompositeCacheImpl is NULL\n");
				}
			} else {
				CommandUtils.dbgPrint(out, "SH_CacheMap is NULL\n");
			}
		} else {
			CommandUtils.dbgPrint(out, "J9SharedClassConfig is NULL\n");
		}
		return null;
	}

	private void printHelp(PrintStream out) {
		CommandUtils.dbgPrint(out, "!shrc stats [range|layer=<n>]                  -- Print cache stats\n");
		CommandUtils.dbgPrint(out, "!shrc allstats [range|layer=<n>]               -- Print all cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc rcstats [range|layer=<n>]                -- Print romclass cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc cpstats [range|layer=<n>]                -- Print classpath cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc aotstats [range|layer=<n>]               -- Print aot cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc invaotstats [range|layer=<n>]            -- Print invalidated aot cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc orphanstats [range|layer=<n>]            -- Print orphan cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc scopestats [range|layer=<n>]             -- Print scope cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc bytestats [range|layer=<n>]              -- Print byte data cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc startuphint [range|layer=<n>]            -- Print startup hint data cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc crvsnippetstats [range|layer=<n>]        -- Print class relationship snippet cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc ubytestats [range|layer=<n>]             -- Print unindexed byte data cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc stalestats [range|layer=<n>]             -- Print all the stale cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc clstats [range|layer=<n>]                -- Print cachelet cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc preqstats [range|layer=<n>]              -- Print prerequisite cache contents\n");
		CommandUtils.dbgPrint(out, "!shrc classpath <address>                      -- Print classpath at address\n");
		CommandUtils.dbgPrint(out, "!shrc findclass <name>                         -- Find named class\n");
		CommandUtils.dbgPrint(out, "!shrc findclassp <name>                        -- Find named class prefix\n");
		CommandUtils.dbgPrint(out, "!shrc findaot <name>                           -- Find AOT for named method\n");
		CommandUtils.dbgPrint(out, "!shrc findaotp <name>                          -- Find AOT for named method prefix\n");
		CommandUtils.dbgPrint(out, "!shrc aotfor <address>                         -- Find AOT for rom method\n");
		CommandUtils.dbgPrint(out, "!shrc rcfor <address>                          -- Find romclass metadata from romclass address\n");
		CommandUtils.dbgPrint(out, "!shrc method <address>                         -- Lookup rom method in cache\n");
		CommandUtils.dbgPrint(out, "!shrc incache <address>                        -- Lookup address in cache\n");
		CommandUtils.dbgPrint(out, "!shrc cachelet <address>                       -- Print cachelet at address\n");
		
		CommandUtils.dbgPrint(out, "!shrc jitpstats [range|layer=<n>] [corrupt]    -- Print jit profile cache contents, add corrupt to only display the corrupted caches\n");
		CommandUtils.dbgPrint(out, "!shrc findjitp <name>                          -- Find jit profile for named method\n");
		CommandUtils.dbgPrint(out, "!shrc findjitpp <name>                         -- Find jit profile for named method prefix\n");		
		CommandUtils.dbgPrint(out, "!shrc jitpfor <address>                        -- Find jit profile for rom method\n");

		CommandUtils.dbgPrint(out, "!shrc jithstats [range|layer=<n>] [corrupt]    -- Print jit hint cache contents, add corrupt to only display the corrupted caches\n");
		CommandUtils.dbgPrint(out, "!shrc findjith <name>                          -- Find jit hint for named method\n");
		CommandUtils.dbgPrint(out, "!shrc findjithp <name>                         -- Find jit hint for named method prefix\n");		
		CommandUtils.dbgPrint(out, "!shrc jithfor <address>                        -- Find jit hint for rom method\n");

		CommandUtils.dbgPrint(out, "!shrc rtflags                                  -- Display shared classes runtime flags\n");
		CommandUtils.dbgPrint(out, "!shrc extraflags [layer=<n>]                   -- Display shared classes extra flags present in cache header\n");
		CommandUtils.dbgPrint(out, "!shrc write <dir> [<name>]                     -- Write the shared cache to the given directory\n");
		CommandUtils.dbgPrint(out, "!shrc name                                     -- Display the name of the shared cache\n");
		
		CommandUtils.dbgPrint(out,  "\nNote: [range] is specified as <start addr>..<end addr> eg 0x1000..0x2000\n");
	}

	private void dbgShrcPrintAllStats(PrintStream out, J9JavaVMPointer vm, J9SharedClassConfigPointer sharedClassConfig, U8Pointer[] metaStartArray, U8Pointer[] metaEndArray, long statTypes, String searchName, boolean prefix, VoidPointer searchAddress, boolean findCorrupt) throws CorruptDataException {
		List<J9ROMClassPointer> romClassList = new ArrayList<J9ROMClassPointer>();

		boolean entryFound = false;
		
		int numRC = 0;
		int numOrphans = 0;
		int numStale = 0;
		int numCP = 0;
		int numURL = 0;
		int numToken = 0;
		int numAOT = 0;
		int numScope = 0;
		int numByte = 0;
		int numUnindexed = 0;
		int numChararray = 0;
		int numCachelets = 0;
		int numCacheletsNoSegments = 0;
		int numJITProfile = 0;
		int numJITHint = 0;
		int[] numByteOfType = new int[(int) J9SHR_DATA_TYPE_MAX + 1];
		int aotDataLen = 0;
		int aotCodeLen = 0;
		int aotMetaLen = 0;
		int rcMetaLen = 0;
		int scopeMetaLen = 0;
		int scopeDataLen = 0;
		int byteMetaLen = 0;
		int byteDataLen = 0;
		int byteDataRWLen = 0;
		int unindexedByteMetaLen = 0;
		int unindexedByteDataLen = 0;
		int chararrayMetaLen = 0;
		int chararrayDataLen = 0;
		int cacheletMetaLen = 0;
		int jitProfileDataLen = 0;
		int jitProfileMetaLen = 0;
		int jitHintDataLen = 0;
		int jitHintMetaLen = 0;
		long totalROMClassBytes = 0;
		long totalStaleBytes = 0;
		long lntBytes = 0;
		long lvtBytes = 0;
		boolean showAllStaleFlag = ((statTypes & ALL_STALE_STATS) != 0);
		int topLayer = -1;
		
		UDATA debugLNTUsed, debugLVTUsed;
		UDATA[] romclassStartAddress;
		UDATA[] segmentPtr;

		topLayer = dbgShrcCacheTopLayer(out, sharedClassConfig);
		ShrcConfig config = dbgShrcReadConfig(sharedClassConfig, out);
		J9SharedCacheHeaderPointer[] cacheHeader = config.getCacheStartAddress();
		U8Pointer[] cacheHeaderPtr = null; 

		if (topLayer >= 0) {
			cacheHeaderPtr = new U8Pointer[cacheHeader.length];
			for (int i = 0; i < cacheHeader.length; i++) {
				cacheHeaderPtr[i] = U8Pointer.cast(cacheHeader[i]);
			}
		}

		romclassStartAddress = config.getRomclassStartAddress();
		segmentPtr = config.getSegmentPtr();

		for (int i = 0; i <= J9SHR_DATA_TYPE_MAX; i++) {
			numByteOfType[i] = 0;
		}
		
		for (int i = 0; i < metaStartArray.length; i++) {
			CommandUtils.dbgPrint(out, "Meta data region to be used: %s..%s\n", metaStartArray[i].getHexAddress(), metaEndArray[i].getHexAddress());
		}

		for (int i = 0; i < metaStartArray.length; i++) {
			U8Pointer metaStart = metaStartArray[i];
			U8Pointer metaEnd = metaEndArray[i];
			SharedClassMetadataIterator iterator = new SharedClassMetadataIterator(vm, metaStart, metaEnd, 0, true, out);
			while (iterator.hasNext()) {
				ShcItemPointer it = iterator.next();
				U16 itemType = it.dataType();
	
				J9ROMClassPointer romClass;
				J9UTF8Pointer romClassName;
				ROMClassWrapperPointer rcw;
				ScopedROMClassWrapperPointer srcw;
				J9UTF8Pointer rcPartition = J9UTF8Pointer.NULL;
				J9UTF8Pointer rcModContext = J9UTF8Pointer.NULL;
				ClasspathWrapperPointer cpw;
				ClasspathItemPointer cpi;
				UDATA cpiType;
				UDATA byteDataType;
				CompiledMethodWrapperPointer cmw;
				J9ROMMethodPointer romMethod;
				UDATA dataLen, codeLen;
				J9UTF8Pointer utf8;
				ByteDataWrapperPointer bdw;
				UDATA rwOffset, len;
				CharArrayWrapperPointer caw;
				CacheletWrapperPointer cachelet;
				AttachedDataWrapperPointer adw;
				boolean isStale = ShcItemHdrHelper.CCITEMSTALE(ShcItemHdrPointer.cast(ShcItemHelper.ITEMEND(it)));
				
				if (isStale) {
					totalStaleBytes += ShcItemHdrHelper.CCITEMLEN(ShcItemHdrPointer.cast(ShcItemHelper.ITEMEND(it))).longValue();
					++numStale;
				}
				
				if (itemType.eq(TYPE_ORPHAN)) {
					rcMetaLen += OrphanWrapper.SIZEOF + ShcItem.SIZEOF + ShcItemHdr.SIZEOF;
					romClass = OrphanWrapperHelper.romClass(OrphanWrapperPointer.cast(ShcItemHelper.ITEMDATA(it)), cacheHeaderPtr);
					romClassName = romClass.className();
					if (romClassName.isNull()) {
						CommandUtils.dbgPrint(out, "-- could not read romClassName, OW ShcItem %s romClass %s --\n", it.getHexAddress(), romClass.getHexAddress());
						return;
					}
					romClassList.add(romClass);
					if (searchAddress.isNull() || romClass.eq(searchAddress)) {
						if ((statTypes & ORPHAN_STATS) != 0) {
							String className = J9UTF8Helper.stringValue(romClassName);
							if (matchClassName(searchName, className, prefix)) {
								entryFound = true;
								CommandUtils.dbgPrint(out, "%d: %s ORPHAN: %s at !j9romclass %s\n", it.jvmID().longValue(), UDATA.cast(it).getHexValue(), className, romClass.getHexAddress());
							}
						}
					}
					++numOrphans;
				} else if (itemType.eq(TYPE_ROMCLASS) || itemType.eq(TYPE_SCOPED_ROMCLASS)) {
					rcw = ROMClassWrapperPointer.cast(ShcItemHelper.ITEMDATA(it));
					rcMetaLen += ShcItem.SIZEOF + ShcItemHdr.SIZEOF;
					if (itemType.eq(TYPE_ROMCLASS)) {
						rcMetaLen += ROMClassWrapper.SIZEOF;
					} else {
						rcMetaLen += ScopedROMClassWrapper.SIZEOF;
						srcw = ScopedROMClassWrapperPointer.cast(rcw);
						rcPartition = J9UTF8Pointer.cast(ScopedROMClassWrapperHelper.RCWPARTITION(srcw, cacheHeaderPtr));
						rcModContext = J9UTF8Pointer.cast(ScopedROMClassWrapperHelper.RCWMODCONTEXT(srcw, cacheHeaderPtr));
					}
					romClass = J9ROMClassPointer.cast(ROMClassWrapperHelper.RCWROMCLASS(rcw, cacheHeaderPtr));
					romClassName = romClass.className();
					if (romClassName.isNull()) {
						CommandUtils.dbgPrint(out, "-- could not read romClassName, RC ShcItem %s romClass %s --\n", it.getHexAddress(), romClass.getHexAddress());
						return;
					}
					romClassList.add(romClass);
					if (searchAddress.isNull() || (romClass.eq(searchAddress))) {
						cpw = ClasspathWrapperPointer.cast(ROMClassWrapperHelper.RCWCLASSPATH(ROMClassWrapperPointer.cast(ShcItemHelper.ITEMDATA(it)), cacheHeaderPtr));
						if (((statTypes & ROMCLASS_STATS) != 0)
							|| (showAllStaleFlag && isStale)
						) {
							String className = J9UTF8Helper.stringValue(romClassName);
							if (matchClassName(searchName, className, prefix)) {
								entryFound = true;
								CommandUtils.dbgPrint(out, "%d: %s ROMCLASS: %s at !j9romclass %s ", it.jvmID().longValue(), it.getHexAddress(), className, romClass.getHexAddress());
								if (isStale) {
									if (!romClass.isNull()) {
										totalStaleBytes += romClass.romSize().longValue();
									}
									CommandUtils.dbgPrint(out, "!STALE!");
								}
								CommandUtils.dbgPrint(out, "\n");
	
								cpi = ClasspathItemPointer.cast(ClasspathWrapperHelper.CPWDATA(cpw));
								cpiType = new UDATA(cpi.type());
								if (cpiType.eq(CP_TYPE_CLASSPATH)) {
									CommandUtils.dbgPrint(out, "\tIndex %d in !shrc classpath %s\n", rcw.cpeIndex().longValue(), cpw.getHexAddress());
								} else if (cpiType.eq(CP_TYPE_URL)) {
									CommandUtils.dbgPrint(out, "\tURL !shrc classpath %s\n", cpw.getHexAddress());
								} else if (cpiType.eq(CP_TYPE_TOKEN)) {
									CommandUtils.dbgPrint(out, "\tToken !shrc classpath %s\n", cpw.getHexAddress());
								}
	
								if (itemType.eq(TYPE_SCOPED_ROMCLASS)) {
									if (rcPartition.notNull() && rcModContext.isNull()) {
										CommandUtils.dbgPrint(out, "\tPartition !j9utf8 %s %s\n", rcPartition.getHexAddress(), dbgShrcPrintableString(rcPartition));
									} else if (rcModContext.notNull() && rcPartition.isNull()) {
										CommandUtils.dbgPrint(out, "\tModContext !j9utf8 %s %s\n", rcModContext.getHexAddress(), dbgShrcPrintableString(rcModContext));
									} else if (rcModContext.notNull() && rcPartition.notNull()) {
										CommandUtils.dbgPrint(out, "\tPartition !j9utf8 %s %s in ModContext !j9utf8 %s %s\n", rcPartition.getHexAddress(), dbgShrcPrintableString(rcPartition), rcModContext.getHexAddress(), dbgShrcPrintableString(rcModContext));
									}
								}
							}
						}
					}
					++numRC;
	
				} else if (itemType.eq(TYPE_CLASSPATH)) {
					cpw = ClasspathWrapperPointer.cast(ShcItemHelper.ITEMDATA(it));
					cpi = ClasspathItemPointer.cast(ClasspathWrapperHelper.CPWDATA(cpw));
	
					if ((statTypes & CLASSPATH_STATS) != 0) {
						entryFound = true;
						dbgShrcPrintClasspath(out, cpw);
					}
	
					cpiType = new UDATA(cpi.type());
					if (cpiType.eq(CP_TYPE_CLASSPATH)) {
						++numCP;
					} else if (cpiType.eq(CP_TYPE_URL)) {
						++numURL;
					} else if (cpiType.eq(CP_TYPE_TOKEN)) {
						++numToken;
					}
				} else if (itemType.eq(TYPE_COMPILED_METHOD) || itemType.eq(TYPE_INVALIDATED_COMPILED_METHOD)) {
					cmw = CompiledMethodWrapperPointer.cast(ShcItemHelper.ITEMDATA(it));
					romMethod = J9ROMMethodPointer.cast(CompiledMethodWrapperHelper.CMWROMMETHOD(cmw, cacheHeaderPtr));
					dataLen = new UDATA(cmw.dataLength());
					codeLen = new UDATA(cmw.codeLength());
					aotDataLen += dataLen.longValue();
					aotCodeLen += codeLen.longValue();
					aotMetaLen += CompiledMethodWrapper.SIZEOF + ShcItem.SIZEOF + ShcItemHdr.SIZEOF;
					if ((((statTypes & AOT_STATS) != 0) && itemType.eq(TYPE_COMPILED_METHOD))
						|| (((statTypes & INV_AOT_STATS) != 0) && itemType.eq(TYPE_INVALIDATED_COMPILED_METHOD))
						|| (showAllStaleFlag && isStale)
					) {
						if (searchAddress.isNull() || (romMethod.eq(searchAddress))) {
							String methodName = J9ROMMethodHelper.getName(romMethod) + J9ROMMethodHelper.getSignature(romMethod);
							/* ignore the leading . */
							if (matchRomMethodName(searchName, J9ROMMethodHelper.getName(romMethod), J9ROMMethodHelper.getSignature(romMethod), prefix)) {
								entryFound = true;
								CommandUtils.dbgPrint(out, "%d: %s AOT data !j9x %s,%s code !j9x %s,%s ", it.jvmID().longValue(), it.getHexAddress(), CompiledMethodWrapperHelper.CMWDATA(cmw).getHexAddress(), dataLen.getHexValue(), CompiledMethodWrapperHelper.CMWCODE(cmw).getHexAddress(), codeLen.getHexValue());
								if (isStale) {
									CommandUtils.dbgPrint(out, "!STALE!");
								}
								if (itemType.eq(TYPE_INVALIDATED_COMPILED_METHOD)) {
									CommandUtils.dbgPrint(out, "INVALIDATED");
								}
								CommandUtils.dbgPrint(out, "\n\t%s !j9rommethod %s\n", methodName, romMethod.getHexAddress());
	
								ListIterator<J9ROMClassPointer> iter = romClassList.listIterator(romClassList.size());
								while (iter.hasPrevious()) {
									J9ROMClassPointer lastRomClass = iter.previous();
									if (romMethod.gt(lastRomClass) && romMethod.lt(lastRomClass.addOffset(lastRomClass.romSize()))) {
										CommandUtils.dbgPrint(out, "\t%s !j9romclass %s\n", J9UTF8Helper.stringValue(lastRomClass.className()), lastRomClass.getHexAddress());
										break;
									}
								}
							}
						}
					}
					++numAOT;
				} else if (itemType.eq(TYPE_ATTACHED_DATA)) {
					adw = AttachedDataWrapperPointer.cast(ShcItemHelper.ITEMDATA(it));
					romMethod = J9ROMMethodPointer.cast(AttachedDataWrapperHelper.ADWCACHEOFFSET(adw, cacheHeaderPtr));
					dataLen = new UDATA(adw.dataLength());
					int adType = adw.type().intValue();
					boolean jitHint = (J9SHR_ATTACHED_DATA_TYPE_JITHINT == adType);
					int adUpdateCount = adw.updateCount().intValue();
					int adCorrupt = adw.corrupt().intValue();
					
					if (jitHint) {
						jitHintDataLen += dataLen.longValue();
						jitHintMetaLen += AttachedDataWrapper.SIZEOF + ShcItem.SIZEOF + ShcItemHdr.SIZEOF;
					} else {
						jitProfileDataLen += dataLen.longValue();
						jitProfileMetaLen += AttachedDataWrapper.SIZEOF + ShcItem.SIZEOF + ShcItemHdr.SIZEOF;
					}
					if ((jitHint && (0 != (statTypes & JITHINT_STATS)))
						|| (!jitHint && (0 != (statTypes & JITPROFILE_STATS)))
						|| (showAllStaleFlag && isStale)
					) {
						if ((!findCorrupt && searchAddress.isNull() || (romMethod.eq(searchAddress)))
								|| (findCorrupt && adCorrupt != -1)) {
							String methodName = J9ROMMethodHelper.getName(romMethod) + J9ROMMethodHelper.getSignature(romMethod);
							if (matchRomMethodName(searchName, J9ROMMethodHelper.getName(romMethod), J9ROMMethodHelper.getSignature(romMethod), prefix)) {
								entryFound = true;
								CommandUtils.dbgPrint(out, "%d: %s %s data !j9x %s,%s  type %d updates %d corrupt %d", 
										it.jvmID().longValue(), it.getHexAddress(), (jitHint? "JITHINT": "JITPROFILE"), AttachedDataWrapperHelper.ADWDATA(adw).getHexAddress(), dataLen.getHexValue(), adType, adUpdateCount, adCorrupt);
								if (isStale) {
									CommandUtils.dbgPrint(out, "!STALE!");
								}
								CommandUtils.dbgPrint(out, "\n\t%s !j9rommethod %s\n", methodName, romMethod.getHexAddress());
	
								ListIterator<J9ROMClassPointer> iter = romClassList.listIterator(romClassList.size());
								while (iter.hasPrevious()) {
									J9ROMClassPointer lastRomClass = iter.previous();
									if (romMethod.gt(lastRomClass) && romMethod.lt(lastRomClass.addOffset(lastRomClass.romSize()))) {
										CommandUtils.dbgPrint(out, "\t%s !j9romclass %s\n", J9UTF8Helper.stringValue(lastRomClass.className()), lastRomClass.getHexAddress());
										break;
									}
								}
							}
						}
					}
					if (jitHint) {
						++numJITHint; 
					} else {
						++numJITProfile; 
					}
				} else if (itemType.eq(TYPE_SCOPE)) {
					utf8 = J9UTF8Pointer.cast(ShcItemHelper.ITEMDATA(it));
					scopeMetaLen += ShcItem.SIZEOF + ShcItemHdr.SIZEOF;
					scopeDataLen += J9UTF8.SIZEOF + utf8.length().longValue();
					if ((statTypes & SCOPE_STATS) != 0) {
						entryFound = true;
						CommandUtils.dbgPrint(out, "%d: %s SCOPE !j9utf8 %s %s\n", it.jvmID().longValue(), it.getHexAddress(), utf8.getHexAddress(), J9UTF8Helper.stringValue(utf8));
					}
					++numScope;
				} else if (itemType.eq(TYPE_BYTE_DATA)) {
					bdw = ByteDataWrapperPointer.cast(ShcItemHelper.ITEMDATA(it));
					byteMetaLen += ShcItem.SIZEOF + ShcItemHdr.SIZEOF + ByteDataWrapper.SIZEOF;
					rwOffset = new UDATA(ByteDataWrapperHelper.BDWEXTBLOCK(bdw, cacheHeaderPtr));
					len = new UDATA(ByteDataWrapperHelper.BDWLEN(bdw));
					byteDataType = new UDATA(ByteDataWrapperHelper.BDWTYPE(bdw));
					if (byteDataType.longValue() <= J9SHR_DATA_TYPE_MAX) {
						numByteOfType[(int) byteDataType.longValue()] += len.longValue();
					} else {
						numByteOfType[(int) J9SHR_DATA_TYPE_UNKNOWN] += len.longValue();
					}
					String byteDataTypeString = getType(byteDataType);
					boolean shouldPrintDbg = ((statTypes & BYTE_STATS) != 0) || byteDataTypeMatchesStatTypes(byteDataTypeString, statTypes);
					if (rwOffset.eq(0)) {
						byteDataLen += len.longValue();
						if (shouldPrintDbg) {
							entryFound = true;
							CommandUtils.dbgPrint(out, "%d: %s %s BYTEDATA !j9x %s,%s", it.jvmID().longValue(), it.getHexAddress(), byteDataTypeString, ByteDataWrapperHelper.getDataFromByteDataWrapper(bdw, cacheHeaderPtr).getHexAddress(), len.getHexValue());
						}
					} else {
						byteDataRWLen += len.longValue();
						if (shouldPrintDbg) {
							entryFound = true;
							CommandUtils.dbgPrint(out, "%d: %s BYTEDATA RW !j9x %s,%s", it.jvmID().longValue(), it.getHexAddress(), ByteDataWrapperHelper.getDataFromByteDataWrapper(bdw, cacheHeaderPtr).getHexAddress(), len.getHexValue());
						}
					}
					if ((shouldPrintDbg) || (showAllStaleFlag && isStale)) {
						if (isStale) {
							CommandUtils.dbgPrint(out, "!STALE!");
						}
						
						UDATA inPrivateUse = new UDATA(ByteDataWrapperHelper.BDWINPRIVATEUSE(bdw));
						UDATA privateOwnerID = new UDATA(ByteDataWrapperHelper.BDWPRIVATEOWNERID(bdw));
						utf8 = J9UTF8Pointer.cast(ByteDataWrapperHelper.BDWTOKEN(bdw, cacheHeaderPtr));
						if (utf8.notNull()) {
							CommandUtils.dbgPrint(out, "\n\tkey: !j9utf8 %s %s\n", utf8.getHexAddress(), J9UTF8Helper.stringValue(utf8));
						}
						if (!inPrivateUse.eq(0) || !privateOwnerID.eq(0)) {
							CommandUtils.dbgPrint(out, "\n\tinPrivateUse %d privateOwnerID %d\n", inPrivateUse.longValue(), privateOwnerID.longValue());
						}
					}
					++numByte;
				} else if (itemType.eq(TYPE_UNINDEXED_BYTE_DATA)) {
					len = new UDATA(it.dataLen());
					unindexedByteMetaLen += ShcItem.SIZEOF + ShcItemHdr.SIZEOF;
					unindexedByteDataLen += len.longValue();
					if ((statTypes & UNINDEXED_BYTE_STATS) != 0) {
						entryFound = true;
						CommandUtils.dbgPrint(out, "%d: %s UNINDEXEDBYTEDATA !j9x %s,%s\n", it.jvmID().longValue(), it.getHexAddress(), ShcItemHelper.ITEMDATA(it).getHexAddress(), len.getHexValue());
					}
					++numUnindexed;
				} else if (itemType.eq(TYPE_CACHELET)) {
					J9SharedCacheHeaderPointer header;
					UDATA cacheletRomClassStartAddress, cacheletSegmentPtr;
					
					cachelet = CacheletWrapperPointer.cast(ShcItemHelper.ITEMDATA(it));
					header = J9SharedCacheHeaderPointer.cast(CacheletWrapperHelper.CLETDATA(cachelet));
					cacheletRomClassStartAddress = UDATA.cast(header).add(header.readWriteBytes());
					cacheletSegmentPtr = UDATA.cast(header).add(header.segmentSRP());
					totalROMClassBytes += cacheletSegmentPtr.sub(cacheletRomClassStartAddress).longValue();
					
					cacheletMetaLen += it.dataLen().longValue();
					if ((statTypes & CACHELET_STATS) != 0) {
						entryFound = true;
						dbgShrcPrintCachelet(out, cachelet);
					}
					if (cachelet.numSegments().eq(0)) {
						++numCacheletsNoSegments;
					}
					++numCachelets;
				} else if (itemType.eq(TYPE_PREREQ_CACHE)) { 
					utf8 = J9UTF8Pointer.cast(ShcItemHelper.ITEMDATA(it));
					scopeMetaLen += ShcItem.SIZEOF + ShcItemHdr.SIZEOF;
					scopeDataLen += J9UTF8.SIZEOF + utf8.length().longValue();
					if ((statTypes & PREREQ_CACHE_STATS) != 0) {
						entryFound = true;
						CommandUtils.dbgPrint(out, "%d: %s PREREQ CACHE UNIQUE ID !j9utf8 %s %s\n", it.jvmID().longValue(), it.getHexAddress(), utf8.getHexAddress(), J9UTF8Helper.stringValue(utf8));
					}
				} else {
					continue;
				}
			}
			if ((statTypes & FIND_METHOD) != 0) {
				Iterator<J9ROMClassPointer> lastRomClassIterator = romClassList.iterator();
				J9ROMClassPointer lastRomClass = J9ROMClassPointer.NULL;
				while (lastRomClassIterator.hasNext()) {
					lastRomClass = lastRomClassIterator.next();
					if (searchAddress.gt(lastRomClass) && searchAddress.lt(lastRomClass.addOffset(lastRomClass.romSize()))) {
						entryFound = true;
						break;
					}
				}
				if (entryFound) {
					CommandUtils.dbgPrint(out, "!j9rommethod %s found in %s !j9romclass %s\n", J9ROMMethodPointer.cast(searchAddress).getHexAddress(), J9UTF8Helper.stringValue(lastRomClass.className()), lastRomClass.getHexAddress());
				}
			}

			if (cacheHeader[i].containsCachelets().eq(0)) {
				totalROMClassBytes += segmentPtr[i].sub(romclassStartAddress[i]).longValue();
			}
			J9SharedCacheHeaderInfo helper = new J9SharedCacheHeaderInfo(cacheHeader[i]);
			debugLNTUsed = helper.getDebugLNTUsed();
			debugLVTUsed = helper.getDebugLVTUsed();
			lntBytes += debugLNTUsed.longValue();
			lvtBytes += debugLVTUsed.longValue();
		}

		if ((statTypes & FIND_METHOD) != 0) {
			if (!entryFound) {
				CommandUtils.dbgPrint(out, "!j9rommethod %s not found in cache\n", J9ROMMethodPointer.cast(searchAddress).getHexAddress());
				if (topLayer <= 0) {
					if (searchAddress.lt(VoidPointer.cast(romclassStartAddress[0]))) {
						CommandUtils.dbgPrint(out, "\taddress is < cache romclassStartAddress %s\n", romclassStartAddress[0].getHexValue());
					}
					if (searchAddress.gt(VoidPointer.cast(segmentPtr[0]))) {
						CommandUtils.dbgPrint(out, "\taddress is > cache romclass segment %s\n", segmentPtr[0].getHexValue());
					}
				}
			}
		} else if (!entryFound) {
			CommandUtils.dbgPrint(out, "No entry found in the cache\n");
		}

		if (searchAddress.isNull() && searchName == null) {
			CommandUtils.dbgPrint(out, "\nCache contains %d classes, %d orphans, %d classpaths, %d URLs, %d tokens\n", numRC, numOrphans, numCP, numURL, numToken);
			CommandUtils.dbgPrint(out, "%d AOT, %d SCOPES, %d BYTE data, %d UNINDEXED DATA, %d CHARARRAY, %d stale\n", numAOT, numScope, numByte, numUnindexed, numChararray, numStale);
			CommandUtils.dbgPrint(out, "stale bytes %d\n", totalStaleBytes);
			
			CommandUtils.dbgPrint(out, "%d JITPROFILE, %d JITHINT\n", numJITProfile, numJITHint);
			CommandUtils.dbgPrint(out, "AOT data length %d code length %d metadata %d total %d\n", aotDataLen, aotCodeLen, aotMetaLen, aotDataLen + aotCodeLen, aotMetaLen);
			CommandUtils.dbgPrint(out, "JITPROFILE data length %d metadata %d \n", jitProfileDataLen, jitProfileMetaLen);
			CommandUtils.dbgPrint(out, "JITHINT data length %d metadata %d \n", jitHintDataLen, jitHintMetaLen);
			
			CommandUtils.dbgPrint(out, "ROMClass data %d metadata %d\n", totalROMClassBytes, rcMetaLen);
			CommandUtils.dbgPrint(out, "SCOPE data %d metadata %d total %d\n", scopeDataLen, scopeMetaLen, scopeMetaLen + scopeDataLen);
			CommandUtils.dbgPrint(out, "BYTE data %d metadata %d rwarea %d\n", byteDataLen, byteMetaLen, byteDataRWLen);
			CommandUtils.dbgPrint(out, "UNINDEXEDBYTE data %d metadata %d\n", unindexedByteDataLen, unindexedByteMetaLen);
			CommandUtils.dbgPrint(out, "CHARARRAY data %d metadata %d\n", chararrayDataLen, chararrayMetaLen);
			CommandUtils.dbgPrint(out, "BYTEDATA Summary\n");
			CommandUtils.dbgPrint(out, "\tUNKNOWN %d  HELPER %d  POOL %d  AOTHEADER %d\n", numByteOfType[(int) J9SHR_DATA_TYPE_UNKNOWN], numByteOfType[(int) J9SHR_DATA_TYPE_HELPER], numByteOfType[(int) J9SHR_DATA_TYPE_POOL], numByteOfType[(int) J9SHR_DATA_TYPE_AOTHEADER]);
			if (J9SHR_DATA_TYPE_STARTUP_HINTS < 0) {
				CommandUtils.dbgPrint(out, "\tJCL %d  VM %d  ROMSTRING %d  ZIPCACHE %d\n", numByteOfType[(int) J9SHR_DATA_TYPE_JCL], numByteOfType[(int) J9SHR_DATA_TYPE_VM], numByteOfType[(int) J9SHR_DATA_TYPE_ROMSTRING], numByteOfType[(int) J9SHR_DATA_TYPE_ZIPCACHE]);
			} else {
				CommandUtils.dbgPrint(out, "\tJCL %d  VM %d  ROMSTRING %d  ZIPCACHE %d  STARTUPHINTS %d\n", numByteOfType[(int) J9SHR_DATA_TYPE_JCL], numByteOfType[(int) J9SHR_DATA_TYPE_VM], numByteOfType[(int) J9SHR_DATA_TYPE_ROMSTRING], numByteOfType[(int) J9SHR_DATA_TYPE_ZIPCACHE], numByteOfType[(int) J9SHR_DATA_TYPE_STARTUP_HINTS]);
			}
			if (J9SHR_DATA_TYPE_CRVSNIPPET < 0) {
				CommandUtils.dbgPrint(out, "\tJITHINT %d  AOTCLASSCHAIN %d AOTTHUNK %d\n", numByteOfType[(int) J9SHR_DATA_TYPE_JITHINT], numByteOfType[(int) J9SHR_DATA_TYPE_AOTCLASSCHAIN], numByteOfType[(int) J9SHR_DATA_TYPE_AOTTHUNK]);
			} else {
				CommandUtils.dbgPrint(out, "\tJITHINT %d  AOTCLASSCHAIN %d AOTTHUNK %d CRVSNIPPET %d\n", numByteOfType[(int) J9SHR_DATA_TYPE_JITHINT], numByteOfType[(int) J9SHR_DATA_TYPE_AOTCLASSCHAIN], numByteOfType[(int) J9SHR_DATA_TYPE_AOTTHUNK], numByteOfType[(int) J9SHR_DATA_TYPE_CRVSNIPPET]);
			}
			if (cacheletMetaLen > 0) {
				CommandUtils.dbgPrint(out, "CACHELET count %d (without segments %d) metadata %d\n", numCachelets, numCacheletsNoSegments, cacheletMetaLen);
			}
			/* Display debug area info */
			CommandUtils.dbgPrint(out, "DEBUG Area Summary\n");
			CommandUtils.dbgPrint(out, "\tLineNumberTable bytes    : %d\n", lntBytes);
			CommandUtils.dbgPrint(out, "\tLocalVariableTable bytes : %d\n", lvtBytes);
		}
	}

	ShrcConfig dbgShrcReadConfig(J9SharedClassConfigPointer sharedClassConfig, PrintStream out) throws CorruptDataException {
		int topLayer = dbgShrcCacheTopLayer(out, sharedClassConfig);
		J9SharedCacheHeaderPointer[] cacheStartAddress = null;
		UDATA[] romclassStartAddress = null;
		UDATA[] segmentPtr = null;
		if (topLayer >= 0) {
			cacheStartAddress = new J9SharedCacheHeaderPointer[topLayer + 1];
			romclassStartAddress = new UDATA[topLayer + 1];
			segmentPtr = new UDATA[topLayer + 1];
		} else {
			cacheStartAddress = new J9SharedCacheHeaderPointer[1];
			romclassStartAddress = new UDATA[1];
			segmentPtr = new UDATA[1];
		}

		SH_CacheMapPointer cacheMap = sharedClassConfig.sharedClassCache();
		J9SharedClassCacheDescriptorPointer cacheDescriptor = sharedClassConfig.cacheDescriptorList();
		
		if (topLayer > 0) {
			/* get cacheDescriptor of layer 0 cache */
			cacheDescriptor = cacheDescriptor.previous();
		}
		int layer = 0;
		do {
			/* TRY TO GET : cacheStartAddress */
			/* 1st try : get cacheStartAddress from sharedClassConfig->cacheDescriptorList->cacheStartAddress */
			if (null != cacheDescriptor) {
				cacheStartAddress[layer] = cacheDescriptor.cacheStartAddress();
			}
			
			/* If cacheStartAddress could not get on first try, then
			 * 2nd try : get cacheStartAddress from sharedClassConfig->sharedClassCache->_cc->_theca
			 * If cacheStartAddress still could not get on second try, then
			 * 3rd try : get cacheStartAddress from sharedClassConfig->sharedClassCache->_cc->_oscache->_dataStart
			 * 
			 * If cacheStartAddress could not get on third try, then return since shared cache is not initialized enough.
			 */
			if (cacheStartAddress[layer].isNull()) {
				
				if (cacheMap.notNull()) {
					/*  _ccHead -------------> ccNext ---------> ccNext --------> ........---------> ccTail
					 * (top layer)         (middle layer)     (middle layer)      ........         (layer 0)
					 */
					SH_CompositeCacheImplPointer compositeCacheImpl = cacheMap._ccTail();
					for (int tmplayer = 0; tmplayer < layer; tmplayer++) {
						compositeCacheImpl = compositeCacheImpl._previous();
					}
					if (compositeCacheImpl.notNull()) {
						cacheStartAddress[layer] = compositeCacheImpl._theca();
						if (cacheStartAddress[layer].isNull()) {
							SH_OSCachePointer osCache = compositeCacheImpl._oscache();
							if (osCache.notNull()) {
								cacheStartAddress[layer] = J9SharedCacheHeaderPointer.cast(osCache._dataStart().longValue());
							}
						}
					}
				}
			}

			if (cacheStartAddress[layer].isNull()) {
				CommandUtils.dbgPrint(out, "cacheStartAddress is zero for layer %d\n", layer);
				return null;
			}

			/* 
			 * TRY TO GET romclassStartAddress
			 * 1st try : Get it from cacheDescriptor
			 */
			if (cacheDescriptor.notNull()) {
				romclassStartAddress[layer] = UDATA.cast(cacheDescriptor.romclassStartAddress());
			}
			
			/*
			 * If romclassStartAddress could not get on first try, 
			 * 2nd try : Calculate it manually by adding readWriteBytes to cacheStartAddress.
			 */
			if ((0 == romclassStartAddress[layer].longValue()) && (0 != cacheStartAddress[layer].readWriteBytes().longValue())) {
				romclassStartAddress[layer] = new UDATA(cacheStartAddress[layer].addOffset(cacheStartAddress[layer].readWriteBytes()).longValue());
			}
			
			/*
			 * If romclassStartAddress could not get on the second try, 
			 * then print info message and continue running.
			 */
			if (0 == romclassStartAddress[layer].longValue()) {
				CommandUtils.dbgPrint(out, "romclassStartAddress is zero for layer %d\n", layer);
			}
			
			/*
			 * TRY TO GET segmentPtr
			 * 1st try : Try to get it by resolving segmentSRP from cacheStartAddress
			 */
			UDATA segmentSRP = cacheStartAddress[layer].segmentSRP();
			if (0 != segmentSRP.longValue()) {
				segmentPtr[layer] = UDATA.cast(cacheStartAddress[layer]).add(segmentSRP);
			}

			/*
			 * If segmentPtr could not get on the second try, 
			 * then print info message and continue running.
			 * 
			 */
			if (0 == segmentPtr[layer].longValue()) {
				CommandUtils.dbgPrint(out, "segmentPtr is zero for layer %d\n", layer);
			}
			if (topLayer > 0) {
				cacheDescriptor = cacheDescriptor.previous();
			}
			layer += 1;
		} while (layer <= topLayer);
		
		return new ShrcConfig(cacheStartAddress, romclassStartAddress, segmentPtr);
	}

	static class ShrcConfig {
		private final J9SharedCacheHeaderPointer[] cacheStartAddress;
		private final UDATA[] romclassStartAddress;
		private final UDATA[] segmentPtr;

		public ShrcConfig(J9SharedCacheHeaderPointer[] cacheStartAddress, UDATA[] romclassStartAddress, UDATA[] segmentPtr) {
			this.cacheStartAddress = cacheStartAddress;
			this.romclassStartAddress = romclassStartAddress;
			this.segmentPtr = segmentPtr;
		}

		public J9SharedCacheHeaderPointer[] getCacheStartAddress() {
			return cacheStartAddress;
		}

		public UDATA[] getRomclassStartAddress() {
			return romclassStartAddress;
		}

		public UDATA[] getSegmentPtr() {
			return segmentPtr;
		}
	}
	
	U8Pointer[] getSharedCacheMetadataStart(J9JavaVMPointer vm, PrintStream out) throws CorruptDataException {
		J9SharedClassConfigPointer sharedConfig = vm.sharedClassConfig();
		if (sharedConfig.isNull()) {
			CommandUtils.dbgPrint(out, "SharedClassConfig can not be found.\n");
			return null;
		}
		
		ShrcConfig config = dbgShrcReadConfig(sharedConfig, out);
		J9SharedCacheHeaderPointer[] header = config.getCacheStartAddress();

		U8Pointer[] ret = new U8Pointer[header.length];
		for (int i = 0; i < header.length; i++) {
			J9SharedCacheHeaderInfo helper = new J9SharedCacheHeaderInfo(header[i]);
			ret[i] = U8Pointer.cast(helper.getMetaDataEnd());
		}
		return ret;
	}
	
	U8Pointer[] getSharedCacheMetadataEnd(J9JavaVMPointer vm, PrintStream out) throws CorruptDataException {
		J9SharedClassConfigPointer sharedConfig = vm.sharedClassConfig();
		if (sharedConfig.isNull()) {
			CommandUtils.dbgPrint(out, "SharedClassConfig can not be found.\n");
			return null;
		}
		
		ShrcConfig config = dbgShrcReadConfig(sharedConfig, out);
		J9SharedCacheHeaderPointer[] header = config.getCacheStartAddress();

		U8Pointer[] ret = new U8Pointer[header.length];
		for (int i = 0; i < header.length; i++) {
			J9SharedCacheHeaderInfo helper = new J9SharedCacheHeaderInfo(header[i]);
			ret[i] = U8Pointer.cast(helper.getMetaDataStart()).sub(ShcItemHdr.SIZEOF);
		}
		return ret;
	}

	SharedCacheMetadata[] dbgReadSharedCacheMetadata(J9JavaVMPointer vm, PrintStream out) throws CorruptDataException {
		U8Pointer heapTop[] = getSharedCacheMetadataEnd(vm, out);
		U8Pointer heapBase[] = getSharedCacheMetadataStart(vm, out);
		
		SharedCacheMetadata[] metadata = new SharedCacheMetadata[heapTop.length];
		for (int i = 0; i < heapTop.length; i++) {
			if (heapTop[i].isNull() || heapBase[i].isNull()) {
				CommandUtils.dbgPrint(out, "SharedCache is not initialized enough to find the metadata area of the cache\n");
				continue;
			}
			metadata[i] = dbgReadSharedCacheMetadata(vm, heapBase[i], heapTop[i], out);
		}
		return metadata;
	}

	SharedCacheMetadata dbgReadSharedCacheMetadata(J9JavaVMPointer vm, U8Pointer metaStart, U8Pointer metaEnd, PrintStream out) throws CorruptDataException {
		SharedCacheMetadata metadata =  new SharedCacheMetadata(metaStart, metaEnd);
		if (metadata.getLength().lt(0)) {
			return null;
		} else {
			return metadata;
		}			
	}
	
	private String getType(UDATA byteDataType) {
		long type = byteDataType.longValue();
		
		if (type == J9SHR_DATA_TYPE_HELPER) {
			return "HELPER";
		} else if (type == J9SHR_DATA_TYPE_POOL) {
			return "POOL";
		} else if (type == J9SHR_DATA_TYPE_AOTHEADER) {
			return "AOTHEADER";
		} else if (type == J9SHR_DATA_TYPE_JCL) {
			return "JCL";
		} else if (type == J9SHR_DATA_TYPE_VM) {
			return "VM";
		} else if (type == J9SHR_DATA_TYPE_ROMSTRING) {
			return "ROMSTRING";
		} else if (type == J9SHR_DATA_TYPE_CACHELET) {
			return "CACHELET";
		} else if (type == J9SHR_DATA_TYPE_ZIPCACHE) {
			return "ZIPCACHE"; 
		} else if (type == J9SHR_DATA_TYPE_STARTUP_HINTS) {
			return "STARTUPHINT";
		} else if (type == J9SHR_DATA_TYPE_JITHINT) {
			return "JITHINT";
		} else if (type == J9SHR_DATA_TYPE_AOTCLASSCHAIN) {
			return "AOTCLASSCHAIN";
		} else if (type == J9SHR_DATA_TYPE_AOTTHUNK) {
			return "AOTTHUNK";
		} else if (type == J9SHR_DATA_TYPE_CRVSNIPPET) {
			return "CRVSNIPPET";
		} else {
			return "UNKNOWN(" + type + ")";
		}
	}

	private boolean byteDataTypeMatchesStatTypes(String dataType, long statTypes) {
		if (((dataType.equals("STARTUPHINT")) && (0 != (statTypes & STARTUPHINT_STATS)))
			|| ((dataType.equals("CRVSNIPPET")) && (0 != (statTypes & CRV_SNIPPET_STATS)))
		) {
			return true;
		} else {
			return false;
		}
	}

	private String dbgShrcPrintableString(J9UTF8Pointer utf8) throws CorruptDataException {
		String stringData = J9UTF8Helper.stringValue(utf8);

		// char *ptr = stringData;
		// while (*ptr) {
		// if ((*ptr < 0x20) || (*ptr > 0x7e)) {
		// break;
		// }
		// ptr++;
		// }
		// if (*ptr || (ptr - stringData > 256)) {
		// return "";
		// }

		return stringData;
	}

	private boolean matchClassName(String searchName, String className, boolean prefix) {
		if (searchName == null || className == null) {
			return true;
		}

		if (prefix) {
			return className.startsWith(searchName);
		}

		return searchName.equals(className);
	}

	private boolean matchRomMethodName(String searchName, String methodName, String methodSignature, boolean prefix) {
		if (searchName == null || methodName == null || methodSignature == null) {
			return true;
		}

		String methodNameAndSig = methodName + methodSignature;
		
		if (prefix) {
			return methodNameAndSig.startsWith(searchName);
		}

		if (searchName.equals(methodName)) {
			return true;
		}
		
		return searchName.equals(methodNameAndSig);
	}

	private void dbgShrcPrintCachelet(PrintStream out, CacheletWrapperPointer cw) throws CorruptDataException {
		ShcItemPointer item = ShcItemPointer.cast(UDATA.cast(cw).sub(ShcItem.SIZEOF));
		CommandUtils.dbgPrint(out, "%d: %s CACHELET !shrc cachelet %s\n", item.jvmID().longValue(), item.getHexAddress(), cw.getHexAddress());
		UDATA numHints = cw.numHints();
		U8Pointer hints = U8Pointer.cast(CacheletWrapperHelper.CLETHINTS(cw));
		UDATA numSegments = cw.numSegments();
		CommandUtils.dbgPrint(out, "  numHints: %d at: !j9x %s\n", numHints.longValue(), hints.getHexAddress());
		CommandUtils.dbgPrint(out, "  numSegments: %d segmentStartOffset: %s lastSegmentAlloc: %s\n", numSegments.longValue(), cw.segmentStartOffset().getHexValue(), cw.lastSegmentAlloc().getHexValue());

		if (numSegments.gt(0)) {

			UDATAPointer segHints = UDATAPointer.NULL;
			U8Pointer cursor = hints;

			/* scan past the hints */
			for (int i = 0; numHints.gt(i); ++i) {
				CacheletHintsPointer hint = CacheletHintsPointer.cast(cursor);
				UDATA length = hint.length();
				UDATA dataType = hint.dataType();
				CommandUtils.dbgPrint(out, "  %s hints at: !j9x %s,%s\n", dataType.getHexValue(), UDATA.cast(hint).add(CacheletHints.SIZEOF).getHexValue(), length.getHexValue());
				cursor = cursor.add(length).add(CacheletHints.SIZEOF);
			}
			segHints = UDATAPointer.cast(cursor);

			CommandUtils.dbgPrint(out, "  segment hints at: !j9x %s,%s\n  ", segHints.getHexAddress(), numSegments.mult(UDATA.SIZEOF).getHexValue());
			for (int i = 0; numSegments.gt(i); i++) {
				CommandUtils.dbgPrint(out, "  %s", segHints.at(i).getHexValue());
			}
			CommandUtils.dbgPrint(out, "\n");
		}

		J9SharedCacheHeaderPointer header = J9SharedCacheHeaderPointer.cast(CacheletWrapperHelper.CLETDATA(cw));
		CommandUtils.dbgPrint(out, "  ");
		dbgShrcHeaderOperations(out, header, VoidPointer.NULL, 1, -1);

	}

	class Touple<V1, V2> {
		V1 v1;
		V2 v2;

		public Touple(V1 v1, V2 v2) {
			this.v1 = v1;
			this.v2 = v2;
		}

		public V1 getV1() {
			return v1;
		}

		public V2 getV2() {
			return v2;
		}
	}

	/**
	 * A helper class to retrieve cache header information
	 * 
	 * @param header - the generated J9SharedCacheHeaderPointer class used to determine 
	 * information about the various segments of the cache
	 * 
	 */
	static class J9SharedCacheHeaderInfo {
		private J9SharedCacheHeaderPointer header;
		
		J9SharedCacheHeaderInfo(J9SharedCacheHeaderPointer header){
			this.header = header;
		}
		
		public J9SharedCacheHeaderPointer getHeader() {
			return header;
		}

		public void setHeader(J9SharedCacheHeaderPointer header) {
			this.header = header;
		}

		public UDATA getHeaderStart() {
			return UDATA.cast(header);
		}
		
		public U32 getTotalBytes() throws CorruptDataException {
			return new U32(this.header.totalBytes());
		}
		
		public U32 getSoftMaxBytes() throws CorruptDataException {
			return new U32(this.header.softMaxBytes());
		}
		
		public U32 getReadWriteBytes() throws CorruptDataException {
			return new U32(this.header.readWriteBytes());
		}
		
		public UDATA getReadWritePtr() throws CorruptDataException {
			UDATA readWriteSRP = this.header.readWriteSRP();
			return UDATA.cast(this.header).add(readWriteSRP);
		}
		
		public UDATA getReadWriteStart() {
			return UDATA.cast(this.header).add(J9SharedCacheHeader.SIZEOF);
		}
		
		public UDATA getRomClassesStart() throws CorruptDataException {
			return UDATA.cast(header).add(getReadWriteBytes());
		}
		
		public UDATA getRomClassesEnd() throws CorruptDataException {
			return getSegmentPtr();
		}
		
		public UDATA getSegmentPtr() throws CorruptDataException {
			UDATA segmentSRP = this.header.segmentSRP();
			return UDATA.cast(this.header).add(segmentSRP);
		}
		
		public UDATA getUpdatePtr() throws CorruptDataException {
			UDATA updateSRP = this.header.updateSRP();
			return UDATA.cast(this.header).add(updateSRP);
		}
		
		public UDATA getDebugAreaSize() throws CorruptDataException {
			return new UDATA(this.header.debugRegionSize().longValue());
		}
		
		public UDATA getMetaDataStart() throws CorruptDataException {
			return UDATA.cast(this.header).add(getTotalBytes()).sub(getDebugAreaSize());
		}
		
		public UDATA getMetaDataEnd() throws CorruptDataException {
			return getUpdatePtr();
		}
		
		public UDATA getDebugAreaStart() throws CorruptDataException {
			return UDATA.cast(this.header).add(getTotalBytes()).sub(getDebugAreaSize());
		}
		
		public UDATA getDebugAreaEnd() throws CorruptDataException {
			return getDebugAreaStart().add(getDebugAreaSize());
		}
		
		public UDATA getLineNumberAreaStart() throws CorruptDataException {
			return getDebugAreaStart();
		}
		
		public UDATA getLineNumberAreaEnd() throws CorruptDataException {
			/*
			 * Although lineNumberTableNextSRP and localVariableTableNextSRP are SRPs, 
			 * they are defined as UDATA in J9SharedCacheHeader.
			 * Therefore these SRPs needs to be resolved manually here instead of using SelfRelativePointer class.
			 */ 
			return new UDATA(this.header.lineNumberTableNextSRPEA().longValue() + this.header.lineNumberTableNextSRP().longValue());
		}
		
		public UDATA getLocalVariableAreaStart() throws CorruptDataException {
			return getDebugAreaEnd();
		}
		
		public UDATA getLocalVariableAreaEnd() throws CorruptDataException {
			return new UDATA(this.header.localVariableTableNextSRPEA().longValue() + this.header.localVariableTableNextSRP().longValue());
		}
		
		public UDATA getDebugLVTUsed() throws CorruptDataException {
			return getDebugAreaEnd().sub(getLocalVariableAreaEnd());
		}
		
		public UDATA getDebugLNTUsed() throws CorruptDataException {
			return getLineNumberAreaEnd().sub(getDebugAreaStart());
		}
		
		public I32 getMinAOT() throws CorruptDataException {
			return new I32(this.header.minAOT());
		}
		
		public I32 getMinJIT() throws CorruptDataException {
			return new I32(this.header.minJIT());
		}
		
		public UDATA getAotBytes() throws CorruptDataException {
			return this.header.aotBytes();
		}
		
		public UDATA getJitBytes() throws CorruptDataException {
			return this.header.jitBytes();
		}
		
		public UDATA getCacheFullFlags() throws CorruptDataException {
			return this.header.cacheFullFlags();
		}
		
		/* The equivalent of SH_CompositeCacheImpl::getFreeBlockBytes() */
		public I32 getFreeBlockBytes() throws CorruptDataException {
			long retVal;
			long minAOT = getMinAOT().longValue();
			long minJIT = getMinJIT().longValue();
			long aotBytes = getAotBytes().longValue();
			long jitBytes = getJitBytes().longValue();
			UDATA updatePtr = getUpdatePtr();
			UDATA segmentPtr = getSegmentPtr();
			long freeBytes = updatePtr.sub(segmentPtr).longValue();
			
			if (((-1 == minAOT) && (-1 == minJIT)) ||
				((-1 == minAOT) && (minJIT <= jitBytes)) ||
				((-1 == minJIT) && (minAOT <= aotBytes)) ||
				((minAOT <= aotBytes) && (minJIT <= jitBytes))
			) {
				/* if no reserved space for AOT and JIT or
				 *  no reserved space for AOT and jitBytes is equal to or crossed reserved space for JIT
				 *	no reserved space for JIT and aotBytes is equal to or crossed reserved space for AOT
				 *	then free block bytes = freeBytes as calculated above
				 */
				retVal = freeBytes;
			} else if (minJIT > jitBytes && ((-1 == minAOT) || (minAOT <= aotBytes))) {
				/* if jitBytes within the reserved space for JIT but no reserved space for AOT
				 * or aotBytes is  equal to or crossed reserved space for AOT
				 * then free block bytes =  freebytes - bytes not yet used in reserved space of JIT
				 */
				retVal = freeBytes - (minJIT - jitBytes);
			} else if ((minAOT > aotBytes) && ((-1 == minJIT) || minJIT <= jitBytes)) {
				/*if aotBytes within the reserved space for AOT but no reserved space for JIT
				 * or jitBytes is  equal to or crossed reserved space for JIT
				 * then free block bytes =  freebytes - bytes not yet used in reserved space of AOT
				 */
				retVal = freeBytes - (minAOT - aotBytes);
			} else	{
				 /* We are here if both jitBytes and aotBytes are within the their respective reserved space
				 *  free block bytes = freebytes - bytes not yet used in reserved space of AOT -
				 *       						   bytes not yet used in reserved space of JIT
				 */
				retVal = freeBytes - (minJIT - jitBytes) - (minAOT - aotBytes);
			}
			/* When creating the cache with -Xscminaot/-Xscminjit > real cache size, minAOT/minJIT will be set to the same size to the real cache size by ensureCorrectCacheSizes(),
			 * retVal calculated here can be < 0 in this case.
			 */
			retVal =  (retVal > 0) ? retVal : 0;
			return new I32(retVal);
		}
		
		public U32 getUsedBytes() throws CorruptDataException {
			long totalSize = ShrCCommand.cacheTotalSize; 
			/* this.header.totalBytes() does not include OSCache_mmap_header/OSCache_sysv_header, so use ShrCCommand.cacheTotalSize */
			long freeBlockBytes = getFreeBlockBytes().longValue();
			long freeDebugSize = getDebugAreaSize().longValue() - getDebugLNTUsed().longValue() - getDebugLVTUsed().longValue();
			
			if (0 == totalSize) {
				return null;
			} else {
				return new U32(totalSize - freeBlockBytes - freeDebugSize);
			}
		}

		public UDATA getFreeAvailableBytes() throws CorruptDataException {
			long ret = 0;
			long totalSize = ShrCCommand.cacheTotalSize;
			long freeBlockBytes = getFreeBlockBytes().longValue();
			U32 softMaxBytes = getSoftMaxBytes();
			U32 usedBytes = getUsedBytes();
			
			if (null != usedBytes) {

				if (softMaxBytes.eq(U32.MAX)) {
					ret = totalSize - usedBytes.longValue();
				} else {	
					ret = softMaxBytes.sub(usedBytes).longValue();
				}

				ret = (freeBlockBytes < ret) ? freeBlockBytes : ret;
				return new UDATA(ret);
			} else {
				return null;
			}
		}
	}

	/*
	 * @param cacheletIndex -1: no cachelets, 0: cache, > 0: a cachelet
	 */
	private Touple<Boolean, Long> dbgShrcHeaderOperations(PrintStream out, J9SharedCacheHeaderPointer header, VoidPointer address, int cacheletIndex, int layer) throws CorruptDataException {
		U32 totalBytes, readWriteBytes;
		UDATA readWritePtr, segmentPtr, updatePtr;
		UDATA readWriteStartAddress, romclassStartAddress, metadataStartAddress;
		UDATA debugAreaSize;
		UDATA debugAreaStart;
		UDATA debugAreaEnd;
		UDATA debugAreaUsed;
		UDATA debugAreaPercentUsed;
		UDATA debugLNTUsed;
		UDATA debugLVTUsed;
		UDATA debugLNTNextAdd, debugLVTNextAdd;
		U32 softMaxBytes;
		UDATA getFreeAvailableBytes;
		
		String indent;
		long freeBytes = 0;

		J9SharedCacheHeaderInfo helper = new J9SharedCacheHeaderInfo(header);
		
		totalBytes = helper.getTotalBytes();
		softMaxBytes = helper.getSoftMaxBytes();
		readWriteBytes = helper.getReadWriteBytes();
		segmentPtr = helper.getSegmentPtr();
		updatePtr = helper.getUpdatePtr();
		readWritePtr = helper.getReadWritePtr();
		readWriteStartAddress = helper.getReadWriteStart();
		romclassStartAddress = helper.getRomClassesStart();
		//Get the debug area size, then calculate the metadata and debug area offset
		debugAreaSize = helper.getDebugAreaSize();
		metadataStartAddress = helper.getMetaDataStart();
		debugAreaStart = helper.getDebugAreaStart();
		debugAreaEnd = helper.getDebugAreaEnd();
	
		/* Calculate the amount of LNT and LVT attribute data stored in the cache
		 * Although lineNumberTableNextSRP and localVariableTableNextSRP are SRPs, 
		 * they are defined as UDATA in J9SharedCacheHeader.
		 * Therefore these SRPs needs to be resolved manually here instead of using SelfRelativePointer class.
		 */		
		debugLNTNextAdd = helper.getLineNumberAreaEnd(); //new UDATA(header.lineNumberTableNextSRPEA().longValue() + header.lineNumberTableNextSRP().longValue());
		debugLVTNextAdd = helper.getLocalVariableAreaEnd();//new UDATA(header.localVariableTableNextSRPEA().longValue() + header.localVariableTableNextSRP().longValue());
		debugLNTUsed = helper.getDebugLNTUsed();
		debugLVTUsed = helper.getDebugLVTUsed();
		
		//Calculate the used portion of the debug area 		
		debugAreaUsed = debugLNTUsed.add(debugLVTUsed);
		if (0 != debugAreaSize.longValue()) {
			debugAreaPercentUsed = new UDATA((debugAreaUsed.longValue() * 100) / debugAreaSize.longValue());
		} else {
			debugAreaPercentUsed = new UDATA(100); 
		}
		
		getFreeAvailableBytes = helper.getFreeAvailableBytes();
		if (null != getFreeAvailableBytes) {
			freeBytes = getFreeAvailableBytes.longValue();
		}


		if ((-1 == cacheletIndex)
			|| address.isNull()
		) {
			CommandUtils.dbgPrint(out, "!j9sharedcacheheader %s", header.getHexAddress());
			if (layer >= 0) {
				CommandUtils.dbgPrint(out, "    (layer %d)\n", layer);
			} else {
				CommandUtils.dbgPrint(out, "\n");
			}
			indent = cacheletIndex >= 0 ? "    " : "";
			if (cacheletIndex > 0) {
				CommandUtils.dbgPrint(out, "    ccInitComplete: 0x%x free bytes: %d cacheFullFlags: %d\n", header.ccInitComplete().longValue(), updatePtr.sub(segmentPtr).longValue(), header.cacheFullFlags().longValue());
			} else {
				if (cacheletIndex == -1) {
					CommandUtils.dbgPrint(out, "%scache size       : %d\n", indent, totalBytes.sub(readWriteBytes).longValue());
					if (null != getFreeAvailableBytes) {
						CommandUtils.dbgPrint(out, "%sfree bytes       : %d\n", indent, freeBytes);
					}
					if (softMaxBytes.eq(U32.MAX)) {
						CommandUtils.dbgPrint(out, "%ssoftmx bytes     : %d\n", indent, -1);
					} else {
						CommandUtils.dbgPrint(out, "%ssoftmx bytes     : %d", indent, softMaxBytes.longValue());
						try {
							Field field = ShCFlags.class.getField("J9SHR_AVAILABLE_SPACE_FULL");
							Long flagValue = (Long)field.get(null);

							if (helper.getCacheFullFlags().anyBitsIn(flagValue)) {
								CommandUtils.dbgPrint(out, " (J9SHR_AVAILABLE_SPACE_FULL is set)");						
							}
						} catch (Exception e) {
							e.printStackTrace();
						}
						CommandUtils.dbgPrint(out, "\n");
					}
				}
				CommandUtils.dbgPrint(out, "%sread write area  : %s - %s size %d used %d\n", indent, readWriteStartAddress.getHexValue(), readWritePtr.getHexValue(), readWriteBytes.longValue(), readWritePtr.sub(readWriteStartAddress).longValue());
			}
			CommandUtils.dbgPrint(out, "%ssegment area     : %s - %s size %d\n", indent, romclassStartAddress.getHexValue(), segmentPtr.getHexValue(), segmentPtr.sub(romclassStartAddress).longValue());
			CommandUtils.dbgPrint(out, "%smetadata area    : %s - %s size %d\n", indent, updatePtr.getHexValue(), metadataStartAddress.getHexValue(), metadataStartAddress.sub(updatePtr).longValue());
			CommandUtils.dbgPrint(out, "%sclass debug area : %s - %s size %d used %d (%% used %d)\n", indent, debugAreaStart.getHexValue(), debugAreaEnd.getHexValue(), debugAreaSize.longValue(), debugAreaUsed.longValue(), debugAreaPercentUsed.longValue());
		}

		if (address.isNull()) {
			return new Touple<Boolean, Long>(false, freeBytes);
		}

		if (address.gte(VoidPointer.cast(header)) && address.lt(header.addOffset(J9SharedCacheHeader.SIZEOF))) {
			CommandUtils.dbgPrint(out, "\n0x%x is in the cache%s header", address.getAddress(), cacheletIndex > 0 ? "let" : "");
		} else if (address.gte(VoidPointer.cast(readWriteStartAddress)) && address.lt(VoidPointer.cast(readWritePtr))) {
			CommandUtils.dbgPrint(out, "\n0x%x is in the read write area", address.getAddress());
		} else if (address.gte(VoidPointer.cast(readWritePtr)) && address.lt(VoidPointer.cast(romclassStartAddress))) {
			CommandUtils.dbgPrint(out, "\n0x%x is in the unused part of the read write area", address.getAddress());
		} else if (address.gte(VoidPointer.cast(romclassStartAddress)) && address.lt(VoidPointer.cast(segmentPtr))) {
			CommandUtils.dbgPrint(out, "\n0x%x is in the rom class segment area", address.getAddress());
		} else if (address.gte(VoidPointer.cast(segmentPtr)) && address.lt(VoidPointer.cast(updatePtr))) {
			CommandUtils.dbgPrint(out, "\n0x%x is in unused area between class segments and metadata", address.getAddress());
		} else if (address.gte(VoidPointer.cast(updatePtr)) && address.lt(VoidPointer.cast(metadataStartAddress))) {
			CommandUtils.dbgPrint(out, "\n0x%x is in the metadata area", address.getAddress());
		} else if (!debugAreaSize.eq(0) && address.gte(VoidPointer.cast(metadataStartAddress)) && address.lt(VoidPointer.cast(debugAreaEnd))) {
			if (address.lt(VoidPointer.cast(debugLNTNextAdd))) {
				CommandUtils.dbgPrint(out,"\n0x%x is in the line number table of the class debug area", address.getAddress());	
			} else if (address.gt(VoidPointer.cast(debugLVTNextAdd))) {
				CommandUtils.dbgPrint(out,"\n0x%x is in the local variable table of the class debug area", address.getAddress());
			} else {
				CommandUtils.dbgPrint(out,"\n0x%x is in the unused part of the class debug area", address.getAddress());
			}				
		} else {
			return new Touple<Boolean, Long>(false, freeBytes);
		}
		if (cacheletIndex > 0) {
			CommandUtils.dbgPrint(out, " of cachelet %d\n", cacheletIndex);
		} else if (cacheletIndex == 0) {
			CommandUtils.dbgPrint(out, " of the main cache\n");
		} else if (layer >= 0) {
			CommandUtils.dbgPrint(out, " of layer %d cache\n", layer);
		} else {
			CommandUtils.dbgPrint(out, "\n");
		}
		return new Touple<Boolean, Long>(true, freeBytes);
	}

	private void dbgShrcPrintClasspath(PrintStream out, ClasspathWrapperPointer cpw) throws CorruptDataException {
		ClasspathItemPointer cpi = ClasspathItemPointer.cast(ClasspathWrapperHelper.CPWDATA(cpw));
		UDATA cpiType = new UDATA(cpi.type());
		ShcItemPointer item = ShcItemPointer.cast(UDATA.cast(cpw).sub(ShcItem.SIZEOF));
		U16 jvmID = item.jvmID();
		I16 staleFromIndex = cpw.staleFromIndex();

		CommandUtils.dbgPrint(out, "%d: ", jvmID.longValue());
		if (cpiType.eq(CP_TYPE_CLASSPATH)) {
			CommandUtils.dbgPrint(out, "%s CLASSPATH", UDATA.cast(cpw).getHexValue());
		} else if (cpiType.eq(CP_TYPE_URL)) {
			CommandUtils.dbgPrint(out, "%s URL", UDATA.cast(cpw).getHexValue());
		} else if (cpiType.eq(CP_TYPE_TOKEN)) {
			CommandUtils.dbgPrint(out, "%s TOKEN", UDATA.cast(cpw).getHexValue());
		}
		if (!staleFromIndex.eq(CPW_NOT_STALE)) {
			CommandUtils.dbgPrint(out, " staleFromIndex %d", staleFromIndex.longValue());
		}
		CommandUtils.dbgPrint(out, "\n");

		IDATA itemsAdded = cpi.itemsAdded();
		IDATAPointer cpeiArrayPtr = ClasspathItemHelper.CPEI_ARRAY_PTR_FROM_CPI(cpi);

		for (int i = 0; itemsAdded.gt(i); i++) {
			ClasspathEntryItemPointer cpei = ClasspathEntryItemPointer.cast(UDATA.cast(cpi).add(cpeiArrayPtr.at(i)));
			UDATA cpeiPathLen =  cpei.pathLen();
			U8Pointer cpeiPathPointer = ClasspathEntryItemHelper.CPEIPATH(cpei);
			String cpeiPath = cpeiPathPointer.getCStringAtOffset(0, cpeiPathLen.longValue());
			CommandUtils.dbgPrint(out, "   %d)\t%s: %s \ttimestamp: %s%s\n", i, getProtocolName(cpei.protocol()), cpeiPath, cpei.timestamp().getHexValue(), isStaleCPEntry(cpei) ? " !STALE" : "");
		}
	}
	
	private boolean isStaleCPEntry(ClasspathEntryItemPointer cpei) throws CorruptDataException {
		return (cpei.flags().longValue() & SharedconstsConstants.MARKED_STALE_FLAG) != 0;
	}
	
	private String getProtocolName(UDATA protocol) {
		long protocolValue = protocol.longValue();
		if (protocolValue == SharedconstsConstants.PROTO_JAR) {
			return "JAR";
		} else if (protocolValue == SharedconstsConstants.PROTO_DIR) {
			return "DIR";
		} else if (protocolValue == SharedconstsConstants.PROTO_TOKEN) {
			return "TOKEN";
		} else if (protocolValue == SharedconstsConstants.PROTO_UNKNOWN) {
			return "UNKNOWN";
		}
		return protocol.getHexValue();
	}
	
	
	/* return ADDRESS_NOT_FOUND_IN_CACHE if the address is not in the shared cache.
	 * If the address is in the share cache: 1. for old cache without layer number, return -1. 2. For cache with layer number, return the layer in which the address is in 
	 */
	private void dbgShrcInCache(PrintStream out, J9JavaVMPointer vm, J9SharedClassConfigPointer sharedClassConfig, VoidPointer address) throws CorruptDataException {
		boolean found = false;

		ShrcConfig dbgShrcReadConfig = dbgShrcReadConfig(sharedClassConfig, out);
		J9SharedCacheHeaderPointer[] cacheStartAddressArray = dbgShrcReadConfig.getCacheStartAddress();
		J9SharedCacheHeaderPointer cacheStartAddress = cacheStartAddressArray[0];

		UDATA containsCachelets = cacheStartAddress.containsCachelets();
		if (!containsCachelets.eq(0)) {
			int index = 1;
			long totalFreeBytes = 0;
			dbgShrcHeaderOperations(out, J9SharedCacheHeaderPointer.cast(cacheStartAddress), VoidPointer.NULL, 0, -1);

			SharedClassMetadataIterator iterator = new SharedClassMetadataIterator(vm, TYPE_CACHELET, true, out);
			while (iterator.hasNext()) {
				ShcItemPointer it = iterator.next();
				CacheletWrapperPointer cw = CacheletWrapperPointer.cast(ShcItemHelper.ITEMDATA(it));
				J9SharedCacheHeaderPointer header = J9SharedCacheHeaderPointer.cast(CacheletWrapperHelper.CLETDATA(cw));
				CommandUtils.dbgPrint(out, "Cachelet %d !shrc cachelet %s ", index, cw.getHexAddress());
				Touple<Boolean, Long> dbgShrcHeaderOperations = dbgShrcHeaderOperations(out, header, VoidPointer.NULL, index, -1);
				totalFreeBytes += dbgShrcHeaderOperations.getV2();
				index++;
			}
			CommandUtils.dbgPrint(out, "total cache size : %d\n", (cacheStartAddress.totalBytes().sub((int) J9SharedCacheHeader.SIZEOF)).longValue());
			CommandUtils.dbgPrint(out, "total free bytes : %d\n", totalFreeBytes);
		} else {
			int topLayer = dbgShrcCacheTopLayer(out, sharedClassConfig);
			int layer = (-1 == topLayer) ? -1 : 0;
			do {
				found = dbgShrcHeaderOperations(out, (layer >= 0) ? cacheStartAddressArray[layer] : cacheStartAddress, address, -1, layer).getV1();
				if (found) {
					break;
				} else if (-1 == layer) {
					break;
				} else {
					layer += 1;
				}
			} while (layer < cacheStartAddressArray.length);	
		}

		if (address.isNull()) {
			return;
		}

		if (!containsCachelets.eq(0)) {
			int index = 1;
			SharedClassMetadataIterator iterator = new SharedClassMetadataIterator(vm, TYPE_CACHELET, true, out);
			while (iterator.hasNext()) {
				ShcItemPointer it = iterator.next();

				CacheletWrapperPointer cw = CacheletWrapperPointer.cast(ShcItemHelper.ITEMDATA(it));
				J9SharedCacheHeaderPointer header = J9SharedCacheHeaderPointer.cast(CacheletWrapperHelper.CLETDATA(cw));
				found = dbgShrcHeaderOperations(out, header, address, index++, -1).getV1();
				if (found) {
					break;
				}
			}
			if (!found) {
				found = dbgShrcHeaderOperations(out, cacheStartAddress, address, 0, -1).getV1();
			}
		}

		if (!found) {
			CommandUtils.dbgPrint(out, "\n%s is not in the shared cache\n", U8Pointer.cast(address).getHexAddress());
		}
	}
	
	private void initTotalCacheSize(PrintStream out, J9SharedClassConfigPointer sharedClassConfig) throws CorruptDataException {
		if (0 == cacheTotalSize) {
			SH_OSCachePointer osCache = getOSCache(out, sharedClassConfig);

			if (osCache.notNull()) {
				cacheTotalSize = osCache._cacheSize().longValue();
			}
		}
	}

	class SharedCacheMetadata {
		private final U8Pointer heapBase;
		private final U8Pointer heapTop;
		private final UDATA length;

		public SharedCacheMetadata(U8Pointer heapBase, U8Pointer heapTop) {
			this.heapBase = heapBase;
			this.heapTop = heapTop;
			this.length = new UDATA(heapTop.sub(heapBase));
		}

		public U8Pointer getHeapBase() {
			return heapBase;
		}

		public U8Pointer getHeapTop() {
			return heapTop;
		}

		public UDATA getLength() {
			return length;
		}

		public ShcItemHdrPointer getFirstEntry() {
			return ShcItemHdrPointer.cast(heapTop);
		}
	}

	/**
	 * Walks a cache If limitDataType is zero, all entries are returned. To
	 * limit to a specific data type, set limitDataType to one of the values in
	 * shcdatatypes.h If includeStale is zero, stale entries are included in the
	 * walk. Otherwise, stale entries are skipped.
	 * 
	 * Note that in the out-of-process version, the metadata area is dbgMalloc'd
	 * by this function. This memory is freed when nextDo reaches the end of the
	 * metadata area.
	 * 
	 * Note also that state->reloc returns the difference in bytes between the
	 * dbgMalloc'd memory address and the dump address. To get the dump address,
	 * add state->reloc to any address returned.
	 * 
	 * @param[in] vm A javavm
	 * @param[in] state A walk state struct
	 * @param[in] limitDataType Non-zero if the data type returned should be
	 *            limited
	 * @param[in] includeStale Non-zero if stale classes should be included
	 * 
	 * @return The first item in the cache corresponding to the limits specified
	 */
	class SharedClassMetadataIterator implements Iterator<ShcItemPointer> {

		private ShcItemPointer next;
		private U8Pointer metaStartInCache;
		private U8Pointer metaStart;
		private U8Pointer savedMetaStart;
		private ShcItemHdrPointer entry;
		private ShcItemHdrPointer savedEntry;
		private U16 limitDataType;
		private boolean includeStale;
		private PrintStream out;
		
		private void init(J9JavaVMPointer vm, SharedCacheMetadata cacheMetadata, long limitDataType, boolean includeStale, PrintStream out) throws CorruptDataException {
			this.next = ShcItemPointer.NULL;
			this.metaStartInCache = cacheMetadata.getHeapBase();
			this.metaStart = cacheMetadata.getHeapBase();
			this.savedMetaStart = cacheMetadata.getHeapBase();
			this.entry = cacheMetadata.getFirstEntry();
			this.savedEntry = ShcItemHdrPointer.NULL;
			this.limitDataType = new U16(limitDataType);
			this.includeStale = includeStale;
			this.out = out;
		}

		public SharedClassMetadataIterator(J9JavaVMPointer vm, long limitDataType, boolean includeStale, PrintStream out) throws CorruptDataException {
			SharedCacheMetadata[] cacheMetadata = dbgReadSharedCacheMetadata(vm, out);
			SharedCacheMetadata cacheTopLayerMetadata = cacheMetadata[cacheMetadata.length - 1];
			init(vm, cacheTopLayerMetadata, limitDataType, includeStale, out);
		}
		
		public SharedClassMetadataIterator(J9JavaVMPointer vm, U8Pointer metaRegionStart, U8Pointer metaRegionEnd, long limitDataType, boolean includeStale, PrintStream out) throws CorruptDataException {
			SharedCacheMetadata cacheMetadata = dbgReadSharedCacheMetadata(vm, metaRegionStart, metaRegionEnd, out);
			init(vm, cacheMetadata, limitDataType, includeStale, out);
		}

		public boolean hasNext() {
			if (entry.notNull() && next.isNull()) {
				next = internalNext();
			}
			return next.notNull();
		}
		
		public ShcItemPointer next() {
			if (entry.notNull() && next.isNull()) {
				next = internalNext();
			}
			ShcItemPointer returnValue = next;
			next = ShcItemPointer.NULL;
			return returnValue;
		}

		/**
		 * Walks a cache in or out of process.
		 * 
		 * @return The next item in the cache corresponding to the limits
		 *         specified
		 */
		private ShcItemPointer internalNext() {
			try {
				ShcItemPointer current = ShcItemPointer.NULL;
				ShcItemPointer walk;
				ShcItemHdrPointer nextEntry = entry;
				do {
					entry = nextEntry;
					// some sanity check 
                   	long itemLen = ShcItemHdrHelper.CCITEMLEN(entry).longValue();
                   	if (itemLen == 0) {
                   		CommandUtils.dbgPrint(out, "\nCache header !shcitemhdr %s is corrupt because itemLen is 0\n", entry.getHexAddress());
						nextEntry = ShcItemHdrPointer.NULL;
                   		break;
                    }
					nextEntry = ShcItemHdrHelper.CCITEMNEXT(entry);
					if (UDATA.cast(nextEntry).lt(UDATA.cast(metaStartInCache.sub(ShcItemHdr.SIZEOF)))) {
                		CommandUtils.dbgPrint(out, "\nCache header !shcitemhdr %s is corrupt because itemLen %d is bigger than size of metadata area\n", 
                				entry.getHexAddress(), itemLen);
                		nextEntry = ShcItemHdrPointer.NULL;
						break;
					}
					if (UDATA.cast(nextEntry).lt(UDATA.cast(metaStart.sub(ShcItemHdr.SIZEOF)))) {
						nextEntry = ShcItemHdrPointer.NULL;
					}
					if (UDATA.cast(nextEntry).lte(UDATA.cast(metaStart))) {
						if (savedEntry.notNull()) {
							metaStart = savedMetaStart;
							nextEntry = savedEntry;
							savedEntry = ShcItemHdrPointer.NULL;
							if (UDATA.cast(nextEntry).lte(UDATA.cast(metaStart))) {
								/*
								 * Once we've finished walking the cache, set to
								 * NULL
								 */
								nextEntry = ShcItemHdrPointer.NULL;
							}
						} else {
							/*
							 * Once we've finished walking the cache, set to
							 * NULL
							 */
							nextEntry = ShcItemHdrPointer.NULL;
						}
					}
					walk = ShcItemPointer.cast(ShcItemHdrHelper.CCITEM(entry));
					U16 type = walk.dataType();

					if (type.eq(TYPE_CACHELET)) {
						CacheletWrapperPointer cw = CacheletWrapperPointer.cast(ShcItemHelper.ITEMDATA(walk));
						J9SharedCacheHeaderPointer header = J9SharedCacheHeaderPointer.cast(CacheletWrapperHelper.CLETDATA(cw));
						UDATA totalBytes = header.totalBytes();
						UDATA updatePtr = UDATA.cast(header).add(header.updateSRP());
						UDATA metadataEnd = UDATA.cast(header).add(totalBytes);

						/* skip empty cachelets */

						if (metadataEnd.sub(ShcItemHdr.SIZEOF).gt(updatePtr)) {
							savedEntry = nextEntry;
							metaStart = U8Pointer.cast(updatePtr);
							nextEntry = ShcItemHdrPointer.cast(metadataEnd.sub(ShcItemHdr.SIZEOF));
						}
					}

					if (limitDataType.eq(0) || (type.eq(limitDataType)) && (includeStale || !ShcItemHdrHelper.CCITEMSTALE(entry))) {
						current = walk;
						break;
					}
				} while (nextEntry.notNull());

				entry = nextEntry;
				return current;

			} catch (CorruptDataException e) {
				e.printStackTrace();
				return null;
			}
		}
		public void remove() {
			// do nothing
		}
	}
}
