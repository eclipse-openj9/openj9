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
package j9vm.test.ddrext;

public class Constants {
	
	public static final String HEXADDRESS_HEADER = "0x";
	public static final int HEXADECIMAL_RADIX = 16;
	public static final int DECIMAL_RADIX = 10;
	
	// properties file
	public static final String DDREXT_PROPERTIES = "ddrext.properties";
	
	public static final String DDREXT_EXCLUDES = "ddrext_excludes.xml";
	
	public static final String TESTDOMAIN_NATIVE = "native";
	
	public static final String TESTDOMAIN_REGULAR = "regular"; 

	public static final String DDREXT_EXCLUDES_PROPERTY_NAME = "name";
	
	public static final String DDREXT_EXCLUDES_PROPERTY_DOMAIN = "domain";
	
	public static final String DDREXT_EXCLUDES_PROPERTY_EXCLUDE = "exclude";
	
	// Common Error Messages
	public static final String UNRECOGNIZED_CMD = "Unrecognised command";

	/*
	 * This comma separated list of tokens are common failure keys that will be
	 * searched in all command outputs during validation
	 */
	public static final String COMMON_FAILURE_KEYS = "DDRInteractiveCommandException,<FAULT>,com.ibm.j9ddr.corereaders.memory.MemoryFault,unable to read,could not read,dump event";

	public static final String CORE_GEN_VM_XDUMP_OPTIONS = " -Xdump:system:defaults:file=";
	public static final String CORE_GEN_VM_OPTIONS = " -Xshareclasses -Xjit:count=0 -Xaot:forceaot,count=0";
	public static final String CORE_GEN_CLASS = "j9vm.test.ddrext.util.CoreDumpUtil";
	public static final String CORE_DUMP_FOLDER = "CoreForDDRTest";

	public static final String FS = System.getProperty("file.separator");
	public static final String NL = System.getProperty("line.separator");

	public static final String RT_PREFIX = "ddrjunit_rt";

	/*
	 * Following are success / failure criteria for ddr junit tests. Note
	 * regarding validation keys and wild cards: The keys support Java regular
	 * expression. Any occurrence of the following characters in the keys will
	 * be treated as Regex meta-characters : ^ $ + * ? . | ( ) { } [ ] \ To
	 * search for these meta-characters 'literally', please precede them with a
	 * backslash (\\)
	 */

	/* Constants related to thread & stack tests */
	public static final String THREAD_CMD = "threads";
	// public static final String THREAD_SUCCESS_KEYS =
	// "stack,j9vmthread,j9thread";
	public static final String THREAD_SUCCESS_KEYS = "!stack 0x.*,!j9vmthread 0x.*";
	public static final String THREAD_FAILURE_KEY = "";

	public static final String THREAD_HELP_SUCCESS_KEY = "list all threads in the VM,list stacks for all threads in the VM";
	public static final String THREAD_STACK_SUCCESS_KEY = "java/lang/Thread,com/ibm/dtfj/tck/harness/Configure\\$DumpThread\\.generateDump\\(\\),!j9method 0x.*,!stack 0x.*";
	public static final String THREAD_FLAGS_SUCCESS_KEY = "main,!j9vmthread 0x.*,publicFlags";
	public static final String THREAD_DEBUGEVENTDATA_SUCCESS_KEYS = "!j9vmthread 0x.*";
	public static final String THREAD_MONITORS_SUCCESS_KEY = "itemCount=[1-9][0-9]*,!j9thread 0x.*,!j9threadmonitor 0x.*";
	public static final String THREAD_TRACE_SUCCESS_KEY = "main,!stack 0x.*,!j9vmthread 0x.*";
	public static final String THREAD_SEARCH_SUCCESS_KEY = "j9vmthread,!j9thread";

	public static final String LOCALMAP_CMD = "localmap";
	public static final String LOCALMAP_SUCCESS_KEY = "j9method";
	public static final String LOCALMAP_FAILURE_KEY = "Not found";

	public static final String STACK_CMD = "stack";
	public static final String STACK_SUCCESS_KEYS = "!j9method 0x.*";
	public static final String STACK_FAILURE_KEY = "";

	public static final String STACKSLOTS_CMD = "stackslots";
	public static final String STACKSLOTS_SUCCESS_KEY = "BEGIN STACK WALK,flags,walkThread,PC,A0";
	public static final String STACKSLOTS_FAILURE_KEY = "";

	public static final String J9VMTHREAD_CMD = "j9vmthread";
	// make sure j9javavm's address is not 0
	public static final String J9VMTHREAD_SUCCESS_KEYS = "J9VMThread,J9Method,!j9javavm 0x0*[1-9a-fA-F][0-9a-fA-F]*";
	public static final String J9VMTHREAD_FAILURE_KEY = "";
	
	public static final String J9POOL_CMD = "j9pool";
	public static final String J9POOLPUDDLE_CMD = "j9poolpuddle"; 
	public static final String J9POOLPUDDLELIST_CMD = "j9poolpuddlelist";
	
	public static final String WALKJ9POOL_CMD = "walkj9pool";
	public static final String WALKJ9POOL_SUCCESS_KEYS = "J9Pool at";
	public static final String WALKJ9POOL_FAILURE_KEY = "Either address is not a valid pool address or pool itself is corrupted";
	

	public static final String J9THREAD_CMD = "j9thread";
	public static final String J9THREAD_SUCCESS_KEYS = "J9ThreadTracing,J9ThreadMonitor";
	public static final String J9THREAD_FAILURE_KEY = "";

	/* Constants related to jit stack tests */
	public static final String JITSTACK_CMD = "jitstack";
	public static final String JITSTACKSLOTS_CMD = "jitstackslots";
	public static final String JITMETADATAFROMPC = "jitmetadatafrompc";
	public static final String JIT_CMD_SUCCESS_KEY = "JIT frame,I2J values";
	public static final String JIT_CMD_FAILURE_KEY = "STACK_CORRUPT,corruptData,CorruptDataException";
	public static final String JITMETADATAFROMPC_SUCCESS_KEY = "className,methodName";
	public static final String JITMETADATAFROMPC_FAILURE_KEY = "";

	/* Constants for shared classes tests */
	public static final String SHRC_CMD = "shrc";
	public static final String SHRC_SUCCESS_KEY = "!shrc stats,!shrc allstats,!shrc rcstats,!shrc cpstats,!shrc aotstats,!shrc orphanstats,!shrc jitpfor <address>,!shrc write <dir> \\[<name>\\]";
	public static final String SHRC_FAILURE_KEY = "no shared cache";

	public static final String SHRC_STATS = "stats";
	public static final String SHRC_STATS_SUCCESS_KEY = "JITPROFILE data length [1-9][0-9]* metadata [1-9][0-9]*,ROMClass data [1-9][0-9]* metadata [1-9][0-9]*,cache size       : [1-9][0-9]*,free bytes,read write area,segment area,metadata area,class debug area";
	public static final String SHRC_STATS_FAILURE_KEY = "no shared cache";

	public static final String SHRC_ALLSTATS = "allstats";
	public static final String SHRC_ALLSTATS_SUCCESS_KEY = "vm.jar,charsets.jar,CLASSPATH,java/lang/Object,JITPROFILE data length [1-9][0-9]* metadata [1-9][0-9]*,ROMClass data [1-9][0-9]* metadata [1-9][0-9]*";
	public static final String SHRC_ALLSTATS_SUCCESS_KEY_JAVA9 = "lib[[/\\\\]]modules,CLASSPATH,java/lang/Object,JITPROFILE data length [1-9][0-9]* metadata [1-9][0-9]*,ROMClass data [1-9][0-9]* metadata [1-9][0-9]*";
	public static final String SHRC_ALLSTATS_FAILURE_KEY = "no shared cache";

	public static final String SHRC_RCSTATS = "rcstats";
	public static final String SHRC_RCSTATS_SUCCESS_KEY = "java/lang/Object,java/lang/Thread,JITPROFILE data length [1-9][0-9]* metadata [1-9][0-9]*,ROMClass data [1-9][0-9]* metadata [1-9][0-9]*";
	public static final String SHRC_RCSTATS_FAILURE_KEY = "no shared cache";

	public static final String SHRC_CPSTATS = "cpstats";
	public static final String SHRC_CPSTATS_SUCCESS_KEY = "vm.jar,CLASSPATH,JITPROFILE data length [1-9][0-9]* metadata [1-9][0-9]*";
	public static final String SHRC_CPSTATS_SUCCESS_KEY_JAVA9 = "lib[[/\\\\]]modules,CLASSPATH,JITPROFILE data length [1-9][0-9]* metadata [1-9][0-9]*";
	public static final String SHRC_CPSTATS_FAILURE_KEY = "no shared cache";

	public static final String SHRC_AOTSTATS = "aotstats";
	public static final String SHRC_AOTSTATS_SUCCESS_KEY = "Ljava/lang/Object,AOT data !j9x 0x.*,AOT data length [1-9][0-9]*";
	public static final String SHRC_AOTSTATS_FAILURE_KEY = "no shared cache";

	public static final String SHRC_ORPHANSTATS = "orphanstats";
	// may need expansion - seems like the orphan output is very much tied with the dump produced
	public static final String SHRC_ORPHANSTATS_SUCCESS_KEY = "ORPHAN,j9romclass";

	public static final String SHRC_ORPHANSTATS_FAILURE_KEY = "no shared cache";

	public static final String SHRC_SCOPESTATS = "scopestats";
	// may need expansion - seems like the orphan output is very much tied with the dump produced
	public static final String SHRC_SCOPESTATS_SUCCESS_KEY = "SCOPE,(rt|vm).jar,j9utf8,BYTEDATA Summary,ROMClass";
	// may need expansion - seems like the orphan output is very much tied with the dump produced
	public static final String SHRC_SCOPESTATS_SUCCESS_KEY_JAVA9 = "SCOPE,lib[[/\\\\]]modules,j9utf8,BYTEDATA Summary,ROMClass";
	public static final String SHRC_SCOPESTATS_FAILURE_KEY = "no shared cache";

	public static final String SHRC_BYTESTATS = "bytestats";
	public static final String SHRC_BYTESTATS_SUCCESS_KEY = "(rt|vm).jar";
	public static final String SHRC_BYTESTATS_SUCCESS_KEY_JAVA9 = "BYTEDATA !j9x";
	public static final String SHRC_BYTESTATS_FAILURE_KEY = "UNKNOWN\\(,no shared cache";

	// To check the startuphint
	public static final String SHRC_STARTUPHINT = "startuphint";
	public static final String SHRC_STARTUPHINT_SUCCESS_KEY = "STARTUPHINT BYTEDATA";
	public static final String SHRC_STARTUPHINT_FAILURE_KEY = "STARTUPHINTS 0";

	public static final String SHRC_CRVSNIPPETSTATS = "crvsnippetstats";
	public static final String SHRC_CRVSNIPPETSTATS_SUCCESS_KEY = "CRVSNIPPET BYTEDATA";
	public static final String SHRC_CRVSNIPPETSTATS_FAILURE_KEY = "CRVSNIPPET 0";

	public static final String SHRC_UBYTESTATS = "ubytestats";

	// currently our dump does not contain any data for this
	public static final String SHRC_UBYTESTATS_SUCCESS_KEY = "UNINDEXEDBYTE";
	public static final String SHRC_UBYTESTATS_FAILURE_KEY = "no shared cache";

	public static final String SHRC_CLSTATS = "clstats";
	// In case of non-real-time platforms we should not see any cachelet info
	public static final String SHRC_CLSTATS_SUCCESS_KEY_NONRTP = "No entry found in the cache";
	public static final String SHRC_CLSTATS_FAILURE_KEY_NONRTP = "no shared cache";
	// In case of real-time platforms we should see cachelet info
	public static final String SHRC_CLSTATS_SUCCESS_KEY_RTP = "CACHELET count,CACHELET,!shrc cachelet";
	public static final String SHRC_CLSTATS_FAILURE_KEY_RTP = "No entry found in the cache,no shared cache";

	public static final String SHRC_CACHELET = "cachelet";
	public static final String SHRC_CACHELET_ADDR_NONRTP = "100";
	public static final String SHRC_CACHELET_SUCCESS_KEY_NONRTP = "Memory Fault";
	public static final String SHRC_CACHELET_FAILURE_KEY_NONRTP = "no shared cache";
	public static final String SHRC_CACHELET_SUCCESS_KEY_RTP = "CACHELET,!shrc cachelet,segment area,metadata area, class debug area";
	public static final String SHRC_CACHELET_FAILURE_KEY_RTP = "Memory Fault,no shared cache";

	public static final String SHRC_CP = "classpath";
	public static final String SHRC_CP_SUCCESS_KEY = "vm.jar";
	public static final String SHRC_CP_SUCCESS_KEY_JAVA9 = "lib[[/\\\\]]modules";
	public static final String SHRC_CP_FAILIRE_KEY = "no shared cache";

	public static final String SHRC_FINDCLASS = "findclass";
	public static final String SHRC_FINDCLASS_NAME = "java/lang/Object";
	public static final String SHRC_FINDCLASS_SUCCESS_KEY = "ROMCLASS: java/lang/Object";
	public static final String SHRC_FINDCLASS_FAILURE_KEY = "no shared cache";

	public static final String SHRC_FINDCLASSP = "findclassp";
	public static final String SHRC_FINDCLASSP_PATTERN = "java/lang/O";
	public static final String SHRC_FINDCLASSP_SUCCESS_KEY = "java/lang/Object,java/lang/OutOfMemoryError";
	public static final String SHRC_FINDCLASSP_FAILURE_KEY = "no shared cache";

	public static final String SHRC_FINDAOT = "findaot";
	public static final String SHRC_FINDAOT_METHODNAME = "getSecurityManager";
	public static final String SHRC_FINDAOT_NO_ENTRY_KEY = "No entry found in the cache";
	public static final String SHRC_FINDAOT_SUCCESS_KEY = "java/lang/System,getSecurityManager";
	public static final String SHRC_FINDAOT_FAILURE_KEY = "no shared cache";

	public static final String SHRC_FINDAOTP = "findaotp";
	public static final String SHRC_FINDAOTP_PATTERN = "getSecurity";
	public static final String SHRC_FINDAOTP_NO_ENTRY_KEY = "No entry found in the cache";
	public static final String SHRC_FINDAOTP_SUCCESS_KEY = "java/lang/System,getSecurityManager";
	public static final String SHRC_FINDAOTP_FAILURE_KEY = "no shared cache";

	public static final String SHRC_AOTFOR = "aotfor";
	public static final String SHRC_AOTFOR_SUCCESS_KEY = "java/lang/System,getSecurityManager";
	public static final String SHRC_AOTFOR_FAILURE_KEY = "no shared cache";

	public static final String SHRC_RCFOR = "rcfor";
	public static final String SHRC_RCFOR_SUCCESS_KEY = "ROMCLASS,java/lang/System";
	public static final String SHRC_RCFOR_FAILURE_KEY = "no shared cache";

	public static final String SHRC_METHOD = "method";
	public static final String SHRC_METHOD_SUCCESS_KEY = "found in java/lang/System";
	public static final String SHRC_METHOD_FAILURE_KEY = "not found in cache,no shared cache";

	public static final String SHRC_INCACHE = "incache";
	public static final String SHRC_INCACHE_SUCCESS_KEY = "is in the cache header";
	public static final String SHRC_INCACHE_FAILURE_KEY = "no shared cache";

	/* JIT profile information */
	public static final String SHRC_JITPSTATS = "jitpstats";
	public static final String SHRC_JITPSTATS_SUCCESS_KEY = "JITPROFILE data length [1-9][0-9]* metadata [1-9][0-9]*,!j9x,java/lang/String,!j9romclass";
	public static final String SHRC_JITPSTATS_FAILURE_KEY = "no shared cache,JITPROFILE data length 0 metadata 0";
	public static final String SHRC_JITP_SUCCESS_KEY = "JITPROFILE data !j9x";	
	
	public static final String SHRC_FINDJITP = "findjitp";
	public static final String SHRC_FINDJITP_METHODNAME = "toString";
	public static final String SHRC_FINDJITP_FAILURE_KEY = "No entry found in the cache";

	public static final String SHRC_FINDJITPP = "findjitpp";
	public static final String SHRC_FINDJITPP_METHODPREFIX = "to";
	public static final String SHRC_FINDJITPP_FAILURE_KEY = "No entry found in the cache";

	public static final String SHRC_JITPFOR = "jitpfor";
	public static final String SHRC_JITPFOR_METHODNAME = "toString";
	public static final String SHRC_JITPFOR_FAILURE_KEY = "No entry found in the cache";
	
	/* JIT hint information */
	public static final String SHRC_JITHSTATS = "jithstats";
	public static final String SHRC_JITHSTATS_SUCCESS_KEY = "JITHINT data length [1-9][0-9]* metadata [1-9][0-9]*,!j9x,java/util/HashMap,!j9romclass";
	public static final String SHRC_JITHSTATS_FAILURE_KEY = "no shared cache,JITPROFILE data length 0 metadata 0";

	public static final String SHRC_FINDJITH = "findjith";
	public static final String SHRC_FINDJITH_METHODNAME = "<init>";
	public static final String SHRC_FINDJITH_SUCCESS_KEY = "<init>\\(.*\\),JITHINT data !j9x,java/.*";
	public static final String SHRC_FINDJITH_FAILURE_KEY = "No entry found in the cache";

	public static final String SHRC_FINDJITHP = "findjithp";
	public static final String SHRC_FINDJITHP_METHODPREFIX = "<init>";
	public static final String SHRC_FINDJITHP_SUCCESS_KEY = "<init>\\(.*\\),JITHINT data !j9x,java/.*";
	public static final String SHRC_FINDJITHP_FAILURE_KEY = "No entry found in the cache";

	public static final String SHRC_JITHFOR = "jithfor";
	public static final String SHRC_JITHFOR_METHODNAME = "<init>";
	public static final String SHRC_JITHFOR_SUCCESS_KEY = "<init>\\(.*\\),JITHINT data !j9x,(java|sun)/.*";
	public static final String SHRC_JITHFOR_FAILURE_KEY = "No entry found in the cache";

	public static final String SHRC_RTFLAGS = "rtflags";
	public static final String SHRC_RTFLAGS_SUCCESS_KEY = "Printing the shared classes runtime flags 0x.*";
	public static final String SHRC_RTFLAGS_FAILURE_KEY = "Type !shrc to see all the valid options,no shared cache";

	public static final String SHRC_WRITE_CACHE = "write";
	public static final String SHRC_WRITE_CACHE_SUCCESS_KEY = "Writing cache to,Cache name is";
	public static final String SHRC_WRITE_CACHE_FAILURE_KEY = null; // need to
																	// determine
	/* The following are constants related to Class related extensions */
	public static final String ALL_CLASSES_CMD = "allclasses";
	public static final String ALL_CLASSES_SUCCESS_KEY = "j9vm/test/corehelper/CoreGen,j9vm/test/corehelper/SimpleThread";
	public static final String ALL_CLASSESS_FAILURE_KEY = "";

	public static final String J9CLASSSHAPE_CMD = "j9classshape";
	public static final String J9CLASSSHAPE_TEST_CLASS = "j9vm/test/corehelper/SimpleThread";
	public static final String J9CLASSSHAPE_SUCCESS_KEY = "_object,"+J9CLASSSHAPE_TEST_CLASS;

	public static final String J9VTABLES_CMD = "j9vtables";
	public static final String J9VTABLES_SUCCESS_KEY = "j9class,j9method,"+J9CLASSSHAPE_TEST_CLASS;

	public static final String J9STATICS_CMD = "j9statics";
	public static final String J9STATICS_SUCCESS_KEY = "j9romstaticfieldshape," + J9CLASSSHAPE_TEST_CLASS;

	public static final String CL_FOR_NAME_CMD = "classforname";
	public static final String CL_FOR_NAME_CLASS = "java/lang/Object";
	public static final String CL_FOR_NAME_SUCCESS_KEY = "!j9class,Found 1 class\\(es\\) named java/lang/Object";
	public static final String CL_FOR_NAME_FAILURE_KEY = "Found 0 class(es) named java/lang/Object";
	public static final String CL_FOR_NAME_CLASS_WC = "\\*Object\\*";
	public static final String CL_FOR_NAME_SUCCESS_KEY_WC = "!j9class,java/lang/reflect/AccessibleObject";
	public static final String CL_FOR_NAME_FAILURE_KEY_WC = "Found 0 class\\(es\\) named \\*Object\\*";

	public static final String ROM_CLASS_SUMMARY_CMD = "romclasssummary";
	public static final String ROM_CLASS_SUMMARY_SUCCESS_KEY = "classes, using:,romHeader,constantPool,fields,methods,method,methodAnnotation,"
			+ "cpShapeDescription,enclosingObject,optionalInfo,UTF8,Padding,variableInfos";
	public static final String ROM_CLASS_SUMMARY_FAILURE_KEY = "";

	public static final String RAM_CLASS_SUMMARY_CMD = "ramclasssummary";
	public static final String RAM_CLASS_SUMMARY_SUCCESS_KEY = "jitVTables,ramHeader,vTable,RAM methods,Constant Pool,J9CPTYPE_CLASS,J9CPTYPE_FLOAT,J9CPTYPE_INSTANCE_FIELD|J9CPTYPE_FIELD,J9CPTYPE_INT,"
			+ "J9CPTYPE_INTERFACE_METHOD,J9CPTYPE_SPECIAL_METHOD|J9CPTYPE_INSTANCE_METHOD,J9CPTYPE_STATIC_FIELD|J9CPTYPE_FIELD,J9CPTYPE_STATIC_METHOD,J9CPTYPE_STRING,J9CPTYPE_UNUSED,J9CPTYPE_VIRTUAL_METHOD|J9CPTYPE_INSTANCE_METHOD,Ram static,Superclasses,iTable";
	public static final String RAM_CLASS_SUMMARY_FAILURE_KEY = "";

	public static final String CL_LOADERS_SUMMARY_CMD = "classloaderssummary";
	public static final String CL_LOADERS_SUMMARY_SUCCESS_KEY = "Number of Classloaders: [0-9]+,\\*System\\*";
	public static final String CL_LOADERS_SUMMARY_FAILURE_KEY = "";

	public static final String DUMP_ALL_ROM_CLASS_LINEAR_CMD = "dumpallromclasslinear";
	public static final String DUMP_ALL_ROM_CLASS_LINEAR_NESTING_THRESHOLD = "1";
	public static final String DUMP_ALL_ROM_CLASS_LINEAR_SUCCESS_KEY = "java/lang/Object,!dumpromclasslinear,ROM Class,romHeader";
	public static final String DUMP_ALL_ROM_CLASS_LINEAR_FAILURE_KEY = "";

	public static final String DUMP_ROM_CLASS_LINEAR_CMD = "dumpromclasslinear";
	public static final String DUMP_ROM_CLASS_LINEAR_SUCCESS_KEY = "romHeader,constantPool,methods,cpShapeDescription,optionalInfo";
	public static final String DUMP_ROM_CLASS_LINEAR_FAILURE_KEY = "";

	public static final String DUMP_ROM_CLASS_CMD = "dumpromclass";
	public static final String DUMP_ROM_CLASS_SUCCESS_KEY = "ROM Size,Class Name: java/lang/Object,Superclass Name,Source File Name,Methods,Interfaces \\(0\\),Fields \\(0\\),CP Shape Description,Methods \\(13\\)";
	public static final String DUMP_ROM_CLASS_FAILURE_KEY = "";

	public static final String DUMP_ROM_CLASS_NAME_CMD = "name:";
	public static final String DUMP_ROM_CLASS_NAME = "java/lang/Object";
	public static final String DUMP_ROM_CLASS_NAME_SUCCESS_KEY = "ROM Size,Class Name: java/lang/Object,Superclass Name,Source File Name: Object.java,Methods,Interfaces \\(0\\),Fields \\(0\\),CP Shape Description,Methods \\(13\\),Found 1 class\\(es\\) with name java/lang/Object";
	public static final String DUMP_ROM_CLASS_NAME_FAILURE_KEY = "Found 0 class\\(es\\) with name java/lang/Object";

	public static final String DUMP_ROM_CLASS_NAME_WC = "*Object*";
	public static final String DUMP_ROM_CLASS_NAME_WC_SUCCESS_KEY = "ROM Size,Class Name: java/lang/Object,Superclass Name,Source File Name: Object.java,Methods,Interfaces \\(0\\),Fields \\(0\\),CP Shape Description,Methods \\(13\\),Found [1-9][0-9]* class\\(es\\) with name \\*Object\\*";
	public static final String DUMP_ROM_CLASS_NAME_WC_FAILURE_KEY = "Found 0 class\\(es\\) with name \\*Object\\*";

	public static final String DUMP_ROM_CLASS_MAPS_CMD = "maps";
	public static final String DUMP_ROM_CLASS_MAPS_SUCCESS_KEY = "ROM Size,Class Name: java/lang/Object,Superclass Name,Source File Name: Object.java,Interfaces \\(0\\),Fields \\(0\\),CP Shape Description,Methods \\(13\\),lmap,dmap,smap";
	public static final String DUMP_ROM_CLASS_MAPS_FAILURE_KEY = "";

	public static final String DUMP_ROM_CLASS_NAME_MAPS_SUCCESS_KEY = "ROM Size,Class Name: java/lang/Object,Superclass Name,Source File Name: Object.java,Interfaces \\(0\\),Fields \\(0\\),CP Shape Description,Methods \\(13\\),lmap,dmap,smap,Found 1 class\\(es\\) with name java/lang/Object";
	public static final String DUMP_ROM_CLASS_NAME_MAPS_FAILURE_KEY = "Found 0 class\\(es\\) with name java/lang/Object";

	public static final String DUMP_ROM_CLASS_INVALID_ADDR_SUCCESS_KEY = "Problem running command";//"DDRInteractiveCommandException";
	public static final String DUMP_ROM_CLASS_INVALID_ADDR_FAILURE_KEY = "Class Name,Superclass Name,Source File Name,Methods,Interfaces";

	public static final String DUMP_ROM_CLASS_INVALID_NAME = "ABCDEFGH";
	public static final String DUMP_ROM_CLASS_INVALID_NAME_SUCCESS_KEY = "Found 0 class\\(es\\) with name ABCDEFGH";
	public static final String DUMP_ROM_CLASS_INVALID_NAME_FAILURE_KEY = "";

	public static final String QUERY_ROM_CLASS_CMD = "queryromclass";
	// may add more queries later like /romHeader/className,/methods,
	public static final String QUERY_ROM_CLASS_QUERY = "/romHeader";
	public static final String QUERY_ROM_CLASS_SUCCESS_KEY = "romSize,className,Section Start: romHeader,Section End: romHeader,optionalInfo";
	public static final String QUERY_ROM_CLASS_FAILURE_KEY = "matched nothing";

	public static final String ANALYSE_ROM_CLASS_UTF8_CMD = "analyseromClassutf8";
	public static final String ANALYSE_ROM_CLASS_UTF8_SUCCESS_KEY = "Global Unique UTF-8,Global Duplicated UTF-8,Distribution of Global Duplicated UTF-8";
	public static final String ANALYSE_ROM_CLASS_UTF8_FAILURE_KEY = "";

	public static final String DUMP_ALL_RAM_CLASS_LINEAR_CMD = "dumpallramclasslinear";
	public static final String DUMP_ALL_RAM_CLASS_LINEAR_NESTING_THRESHOLD = "1";
	public static final String DUMP_ALL_RAM_CLASS_LINEAR_SUCCESS_KEY = "java/lang/Object,!dumpramclasslinear,RAM Class,ramHeader,RAM methods";
	public static final String DUMP_ALL_RAM_CLASS_LINEAR_FAILURE_KEY = "";

	public static final String DUMP_RAM_CLASS_LINEAR_CMD = "dumpramclasslinear";
	public static final String DUMP_RAM_CLASS_LINEAR_SUCCESS_KEY = "RAM methods,jitVTables,ramHeader,vTable,Constant Pool";
	public static final String DUMP_RAM_CLASS_LINEAR_FAILURE_KEY = "";

	public static final String DUMP_ALL_REGIONS_CMD = "dumpallregions";
	public static final String DUMP_ALL_REGIONS_SUCCESS_KEY = "region,start,end";
	public static final String DUMP_ALL_REGIONS_FAILURE_KEY = "";

	public static final String DUMP_ALL_SEGMENTS_CMD = "dumpallsegments";
	public static final String DUMP_ALL_SEGMENTS_COMMON_SUCCESS_KEY = "memorySegments.+!j9memorysegmentlist 0x[0-9a-f]+,segment.+start.+alloc.+end.+type.+size.+,([0-9a-f]+ +){5}[0-9a-f]+,classMemorySegments,jit code segments,jit data segments,";
	public static final String DUMP_ALL_SEGMENTS_64BITCORE_SUCCESS_KEY = "segment.+start.+warmAlloc.+coldAlloc.+end.+size.+";
	public static final String DUMP_ALL_SEGMENTS_32BITCORE_SUCCESS_KEY = "segment.+start.+warm.+cold.+end.+size.+";
	// dump
	public static final String DUMP_ALL_SEGMENTS_FAILURE_KEY = "";

	public static final String DUMP_SEGMENTS_IN_LIST_CMD = "dumpsegmentsinlist";
	public static final String DUMP_SEGMENTS_LIST_NAME = "classMemorySegments";
	public static final String DUMP_SEGMENTS_IN_LIST_SUCCESS_KEY = "segment,start,alloc,end,type,size";
	public static final String DUMP_SEGMENTS_IN_LIST_FAILURE_KEY = "";

	/* Below are constants for callsite related ddr extensions */
	public static final String FINDALLCALLSITES_CMD = "findallcallsites";
	public static final String FINDALLCALLSITES_SUCCESS_KEYS = "jvminit.c";
	public static final String FINDALLCALLSITES_FAILURE_KEY = "";

	public static final String FINDCALLSITE_CMD = "findcallsite";
	public static final String FINDCALLSITE_SUCCESS_KEYS = "j9x";
	public static final String FINDCALLSITE_FAILURE_KEY = "";

	public static final String FINDFREEDCALLSITE_CMD = "findfreedcallsite";
	public static final String FINDFREEDCALLSITE_SUCCESS_KEYS = "j9x";
	public static final String FINDFREEDCALLSITE_FAILURE_KEY = "";

	public static final String PRINTALLCALLSITES_CMD = "printallcallsites";
	public static final String PRINTALLCALLSITES_SUCCESS_KEYS = "jvminit.c";
	public static final String PRINTALLCALLSITES_FAILURE_KEY = "";

	public static final String FINDFREEDCALLSITES_CMD = "findfreedcallsites";
	public static final String FINDFREEDCALLSITES_SUCCESS_KEYS = "Searching for all freed memory block callsites...";
	public static final String FINDFREEDCALLSITES_FAILURE_KEY = "";

	public static final String PRINTFREEDCALLSITES_CMD = "printfreedcallsites";
	public static final String PRINTFREEDCALLSITES_SUCCESS_KEYS = "Searching for all freed memory block callsites...";
	public static final String PRINTFREEDCALLSITES_FAILURE_KEY = "";

	/* constants for ddr extensions in general category */
	public static final String VMCHECK_CMD = "vmcheck";
	public static final String VMCHECK_SUCCESS_KEY = "<vm check: Checking threads>,<vm check: Checking classes>,<vm check: Checking ROM classes>,<vm check: Checking methods>,<vm check: Checking ROM intern string nodes>";
	public static final String VMCHECK_FAILURE_KEY = null; // not sure

	public static final String WALKINTERNTABLE_CMD = "!walkinterntable";
	public static final String WALKINTERNTABLE_MENU_SUCCESS_KEY = "1,2,3,4,5,6,Walkinterntable Sub Menu Options :,To Print Shared Table Structural Info,To Print Local Table Structural Info,To Walk Shared Intern Table \\(From: Most recently used To: Least recently used\\),To Walk Local Intern Table \\(From: Most recently used To: Least recently used\\),To Walk Both Shared&Local Intern Table \\(From: Most recently used To; Least recently used\\),To go into !walkinterntable sub menu";
	public static final String WALKINTERNTABLE_OPT1_SUCCESS_KEY = "J9SharedInvariantInternTable,Total Shared Weight,J9SRPHashTable,J9SRPHashTableInternal";
	public static final String WALKINTERNTABLE_OPT2_SUCCESS_KEY = "J9DbgStringInternTable";
	public static final String WALKINTERNTABLE_OPT3_SUCCESS_KEY = "WALKING SHARED INTERN SRP HASHTABLE,Total Weight,Shared Table Entry,java/lang/Object,java/lang/reflect/Method,WALKING SHARED INTERN SRP HASHTABLE COMPLETED";
	public static final String WALKINTERNTABLE_OPT4_SUCCESS_KEY = "WALKING LOCAL INTERN HASHTABLE,Local Table Entry,Total Weight,WALKING LOCAL INTERN HASHTABLE COMPLETED";
	public static final String WALKINTERNTABLE_OPT5_SUCCESS_KEY = WALKINTERNTABLE_OPT3_SUCCESS_KEY
			+ "," + WALKINTERNTABLE_OPT4_SUCCESS_KEY;

	public static final String WHATISSETDEPTH_CMD = "whatissetdepth";
	public static final String WHATISSETDEPTH_DEPTHVALUE = "2";
	public static final String WHATISSETDEPTH_SUCCESS_KEY = "Max depth set to "
			+ WHATISSETDEPTH_DEPTHVALUE;

	public static final String WHATIS_CMD = "whatis";
	public static final String WHATIS_SUCCESS_KEY_FOR_CLASS = "!j9class,Match found";
	public static final String WHATIS_FAILURE_KEY = "Bad or missing search value,Skip count reset to 0";
	public static final String WHATIS_SUCCESS_KEY_FOR_METHOD = "!void,Match found,!j9vmthread";

	public static final String METHODFORNAME_CMD = "methodforname";
	public static final String METHODFORNAME_METHOD = "sleep";
	public static final String METHODFORNAME_SUCCESS_KEY = "java/lang/Thread.sleep,!j9method";
	public static final String METHODFORNAME_FAILURE_KEY = "Found 0 method\\(s\\)";

	public static final String BYTECODES_CMD = "bytecodes";
	public static final String BYTECODES_SUCCESS_KEY = "Name: "
			+ METHODFORNAME_METHOD + ",Signature:,Access Flags";
	public static final String BYTECODES_FAILURE_KEY = "bad or missing ram method addr";

	public static final String VMCONSTANTPOOL_CMD = "vmconstantpool";
	public static final String VMCONSTANTPOOL_SUCCESS_KEY = "J9ROMFieldRef,Unused,j9romclassref";
	public static final String VMCONSTANTPOOL_FAILURE_KEY = null;

	public static final String SNAPTRACE_CMD = "snaptrace";
	public static final String SNAPTRACE_SUCCESS_KEY = "Writing snap trace to,firstBuffer,nextBuffer";
	public static final String SNAPTRACE_FAILURE_KEY = null;

	public static final String J9EXTENDEDMETHODFLAGINFO_CMD = "j9extendedmethodflaginfo";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY1 = "J9_RAS_METHOD_SEEN";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY2 = "J9_RAS_METHOD_TRACING";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY3 = "J9_RAS_METHOD_SEEN,J9_RAS_METHOD_TRACING";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY4 = "J9_RAS_METHOD_TRACE_ARGS";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY5 = "J9_RAS_METHOD_SEEN,J9_RAS_METHOD_TRACE_ARGS";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY6 = "J9_RAS_METHOD_TRACING,J9_RAS_METHOD_TRACE_ARGS";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY7 = "J9_RAS_METHOD_SEEN,J9_RAS_METHOD_TRACING,J9_RAS_METHOD_TRACE_ARGS";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY8 = "J9_RAS_METHOD_TRIGGERING";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY9 = "J9_RAS_METHOD_SEEN,J9_RAS_METHOD_TRIGGERING";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY10 = "J9_RAS_METHOD_TRACING,J9_RAS_METHOD_TRIGGERING";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY11 = "J9_RAS_METHOD_SEEN,J9_RAS_METHOD_TRACING,J9_RAS_METHOD_TRIGGERING";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY12 = "J9_RAS_METHOD_TRACE_ARGS,J9_RAS_METHOD_TRIGGERING";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY13 = "J9_RAS_METHOD_SEEN,J9_RAS_METHOD_TRACE_ARGS,J9_RAS_METHOD_TRIGGERING";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY14 = "J9_RAS_METHOD_TRACING,J9_RAS_METHOD_TRACE_ARGS,J9_RAS_METHOD_TRIGGERING";
	public static final String J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY15 = "J9_RAS_METHOD_SEEN,J9_RAS_METHOD_TRACING,J9_RAS_METHOD_TRACE_ARGS,J9_RAS_METHOD_TRIGGERING";
	public static final String J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY = "J9_JVMTI_METHOD_SELECTIVE_ENTRY_EXIT";

	public static final String CONTEXT_CMD = "context";
	public static final String CONTEXT_SUCCESS_KEY = "j9javavm";
	public static final String CONTEXT_FAILURE_KEY = null;

	/* Constants for Find related extensions */
	public static final String FINDMETHODFROMPC_CMD = "findmethodfrompc";
	public static final String FINDMETHODFROMPC_SUCCESS_KEY = METHODFORNAME_METHOD
			+ ",Searching for PC,j9method,Bytecode PC offset";
	public static final String FINDMETHODFROMPC_FAILURE_KEY = "Not found";

	/* Below are constants for general find related ddr extensions */
	public static final String FIND_CMD = "find";
	public static final String FIND_SUCCESS_KEY = "Match found at";
	public static final String FIND_FAILURE_KEY = "Search pattern too long, Search term not found";

	public static final String FINDNEXT_CMD = "findnext";
	public static final String FINDNEXT_SUCCESS_KEY = "Match found at";
	public static final String FINDNEXT_FAILURE_KEY = null;

	public static final String FINDHEADER_CMD = "findheader";
	public static final String FINDHEADER_SUCCESS_KEY = "Found memory allocation header";
	public static final String FINDHEADER_FAILURE_KEY = "No memory allocation header found";

	public static final String FINDVM_CMD = "findvm";
	public static final String FINDVM_SUCCESS_KEY = "!j9javavm";
	public static final String FINDVM_FAILURE_KEY = null;

	public static final String FINDPATTERN_CMD = "findpattern";
	public static final String FINDPATTERN_SUCCESS_KEY = "Result = 0x";
	public static final String FINDPATTERN_FAILURE_KEY = null;

	public static final String FINDSTACKVALUE_CMD = "findstackvalue";
	public static final String FINDSTACKVALUE_SUCCESS_KEY = "!j9vmthread, Found at";
	public static final String FINDSTACKVALUE_FAILURE_KEY = "Problem running command";

	public static final String RANGES_CMD = "ranges";
	public static final String RANGES_SUCCESS_KEY = "Base, Top, Size";
	public static final String RANGES_FAILURE_KEY = null;

	public static final String J9X_CMD = "j9x";
	// success key is the passing in address, it is dynamic
	// public static final String J9X_SUCCESS_KEY = "";
	public static final String J9X_FAILURE_KEY = "Problem running command, Memory Fault reading";

	public static final String J9XX_CMD = "j9xx";
	// success key is the passing in address, it is dynamic
	// public static final String J9XX_SUCCESS_KEY = "";
	public static final String J9XX_FAILURE_KEY = "Problem running command, Memory Fault reading";

	public static final String FJ9OBJECT_CMD = "fj9object";
	public static final String FJ9OBJECTTOJ9OBJECT_CMD = "fj9objecttoj9object";
	public static final String FJ9OBJECT_SUCCESS_KEY = "fj9object,j9object";
	public static final String J9OBJECT_CMD = "j9object";
	public static final String J9OBJECT_SUCCESS_KEY = "struct J9Class\\* clazz,Object flags,java/lang/Object";

	public static final String PLUGINS_CMD = "plugins";
	public static final String PLUGINS_LIST_CMD = "list";
	public static final String PLUGINS_LIST_SUCCESS_KEY = "j9vm.test.ddrext.plugin.DDRPluginsTestCmd";
	public static final String PLUGINS_LIST_FAILURE_KEY = "No plugins are currently loaded";
	public static final String PLUGINS_SETPATH_CMD = "setpath";
	public static final String PLUGINS_SETPATH_SUCCESS_KEY = "Plugin search path set to : ";
	public static final String PLUGINS_SETPATH_FAILURE_KEY = null;
	public static final String PLUGINS_RELOAD_CMD = "reload";
	public static final String PLUGINS_RELOAD_SUCCESS_KEY = "Plugins reloaded";
	public static final String PLUGINS_RELOAD_FAILURE_KEY = null;
	public static final String PLUGINS_TEST_CMD = "test";
	public static final String PLUGINS_TEST_SUCCESS_KEY = "j9vm.test.ddrext.plugin.DDRPluginsTestCmd";
	public static final String PLUGINS_TEST_FAILURE_KEY = null;

	public static final String STACKMAP_CMD = "stackmap";
	public static final String STACKMAP_SUCCESS_KEYS_1 = "calculate the stack slot map for the specified PC";
	public static final String STACKMAP_FAILURE_KEYS_1 = "Stack map";

	public static final String STACKMAP_SUCCESS_KEYS_SEARCHING_FOR = "Searching for PC=";
	public static final String STACKMAP_SUCCESS_KEYS_FOUND_METHOD = "Found method";
	public static final String STACKMAP_SUCCESS_KEYS_RELATIVE_PC = "Relative PC =";
	public static final String STACKMAP_SUCCESS_KEYS_METHOD_INDEX_IS = "Method index is";
	public static final String STACKMAP_SUCCESS_KEYS_USING_ROM_METHOD = "Using ROM method";
	public static final String STACKMAP_SUCCESS_KEYS_MSTACK_MAP = "Stack map";
	public static final String STACKMAP_SUCCESS_KEYS_2 = STACKMAP_SUCCESS_KEYS_SEARCHING_FOR
			+ ","
			+ STACKMAP_SUCCESS_KEYS_FOUND_METHOD
			+ ","
			+ STACKMAP_SUCCESS_KEYS_RELATIVE_PC
			+ ","
			+ STACKMAP_SUCCESS_KEYS_METHOD_INDEX_IS
			+ ","
			+ STACKMAP_SUCCESS_KEYS_USING_ROM_METHOD
			+ ","
			+ STACKMAP_SUCCESS_KEYS_MSTACK_MAP;
	public static final String STACKMAP_FAILURE_KEYS_2 = "Not found";
	
	public static final String SETVM_CMD = "setvm";
	public static final String SETVM_SUCCESS_KEYS = "VM set to ";
	public static final String SETVM_FAILURE_KEY_1 = "Error: Specified value \\(=";
	public static final String SETVM_FAILURE_KEY_2 = "\\) is not a javaVM or vmThread pointer, VM not set";
	public static final String SETVM_FAILURE_KEY_3 = "Address value is not a number :";
	public static final String SETVM_FAILURE_KEYS = SETVM_FAILURE_KEY_1 
			+ ","
			+ SETVM_FAILURE_KEY_2 
			+ "," 
			+ SETVM_FAILURE_KEY_3;	
	
	public static final String J9JAVAVM_CMD = "j9javavm";
	
	public static final String SHOWDUMPAGENTS_CMD = "showdumpagents";
	// create dump with option:
	// -Xdump:system:defaults:file=${system.dump},request=exclusive+compact+prepwalk,
	// expect to see the info in showdumpagents. refer to tck_ddrext.xml for detail.
	public static final String SHOWDUMPAGENTS_SUCCESS_KEYS = "-Xdump,request=exclusive\\+compact\\+prepwalk,events=systhrow,"
			+ "filter=java/lang/OutOfMemoryError";
	public static final String SHOWDUMPAGENTS_FAILURE_KEYS = "No dump agent";
	
	public static final String DUMPALLCLASSLOADERS_CMD = "dumpallclassloaders";
	public static final String DUMPALLCLASSLOADERS_SUCCESS_KEYS = "ClassLoader,Address,SharedLibraries,Pool,ClassHashTable,jniIDs Pool,used,capacity,0x[0-9a-fA-F]";
	public static final String DUMPALLCLASSLOADERS_FAILURE_KEYS = "";
	
	public static final String SYM_CMD = "sym";
	public static final String SYM_TEST_METHOD = "java/lang/System.arraycopy";
	public static final String SYM_SUCCESS_KEYS = "Closest match,java_lang_System_arraycopy\\+0x.+";
	public static final String SYM_FAILURE_KEYS = "unknown location";
	
	public static final String DCLIBS_CMD = "dclibs";
	public static final String DCLIBS_SUCCESS_KEYS = "Showing library list for,(Lib.+)+";
	public static final String DCLIBS_FAILURE_KEYS = "Problem running command";
	public static final String DCLIBS_LIB_COLLENTED = "Library has been collected";
	public static final String DCLIBS_EXTRACT_SUCCESS_KEYS = "Extracting libraries to,(Extracting.+)+";
	
	public static final String J9METHOD_CMD = "j9method";
	
	public static final String J9REG_CMD = "j9reg";
	public static final String J9REG_SUCCESS_KEYS = "vmStruct,sp,arg0EA,pc,literals";
	public static final String J9REG_FAILURE_KEYS = "Problem running command";

	public static final String COREINFO_CMD = "coreinfo";
	public static final String COREINFO_SUCCESS_KEYS = "COMMANDLINE,JAVA VERSION INFO,PLATFORM INFO,Platform Name,OS Level,Processors,Architecture,How Many";
	public static final String COREINFO_FAILURE_KEYS = "Problem running command";
	
	public static final String NATIVEMEMINFO_CMD = "nativememinfo";
	public static final String NATIVEMEMINFO_SUCCESS_KEYS = "JRE:,VM:,Classes:,Memory Manager,Java Heap:,Other:,Threads:,Java Stack:, bytes, allocations";
	public static final String NATIVEMEMINFO_FAILURE_KEYS = "Problem running command";
	
	public static final String MONITORS_CMD = "monitors";
	
	public static final String TENANTREGIONS_CMD = "tenantregions";
	
	public static final String TENANTCHECK_CMD = "tenantcheck";
	
	/* Constants related to testing of deadlock detection */
	public static final String DEADLOCK_THREAD = "Thread:";
	public static final String DEADLOCK_OS_THREAD = "OS Thread:";
	public static final String DEADLOCK_BLOCKING_ON = "is blocking on:";
	public static final String DEADLOCK_OWNED_BY = "which is owned by:";
	public static final String DEADLOCK_FIRST_MON = "First Monitor lock";
	public static final String DEADLOCK_SECOND_MON = "Second Monitor lock";
	public static final String DEADLOCK_JAVA_OBJ = "java/lang/Object";
	public static final String DEADLOCK_CMD = "deadlock";
	
	/* Constants related to testing of runtime type resolution */
	public static final String DUMP_HRD_CMD = "MM_HeapRegionDescriptor";
	public static final String TYPERES_HRD_SUCCESS_KEYS = "Fields for MM_Base,Fields for MM_HeapRegionDescriptor";
	public static final String TYPERES_HRD_FAILURE_KEYS = "null,Problem running command";
	public static final String TYPERES_SUBSPACE_STRUCT_CMD = "mm_memorysubspace";
	public static final String TYEPERES_LOCK_STRUCT_CMD = "mm_lightweightnonreentrantlock";
	public static final String TYPERES_LOCK_SUCCESS_KEYS = "Fields for MM_Base,Fields for MM_LightweightNonReentrantLock";
	public static final String TYPERES_LOCK_FAILURE_KEYS = "null,Problem running command";
	
	public static final String ROMCLASS_FOR_NAME_CMD = "romclassforname";
	
}
