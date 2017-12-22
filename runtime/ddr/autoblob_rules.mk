###############################################################################
# Copyright (c) 1991, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

#Makefile containing the rules for generating DDR blobs automatically. Don't run this directly - it needs to be included in another 
#file that defines (at least) CFLAGS and JAVA

#4 stage process - similar but slightly different for C and C++
#1. Generate a C/C++ file consisting entirely of #includes
#2. Pre-process that C/C++ file to create a .i file. For C source, run a cleaning process
#3. Run the C++ parser (extract_structures) to produce an .xml file containing structure data
#4. Transform the data in the XML file, and the #defines in the original header, into C or C++ to be compiled

#VM blob generation
%blob.c: %structs.properties
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GenerateInputCFile $*structs.properties $*structs.c
#linux specific operations 
ifneq (,$(findstring linux_x86,$(PLATFORM)))
	chmod 755 ../buildtools/extract_structures/${PLATFORM}/extract_structures
	cpp -undef -nostdinc -include typestubs.h -Idummy_headers $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS)) -o $*structs.i $*structs.c
endif
#windows specific operations
ifneq (,$(findstring win_x86,$(PLATFORM)))
	-chmod 755 ../buildtools/extract_structures/${PLATFORM}/extract_structures.exe
	cl /P -u -X /FItypestubs.h -Idummy_headers $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS)) /Fo$*structs.i $*structs.c
endif
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.StripGarbage $*structs.i
	../buildtools/extract_structures/${PLATFORM}/extract_structures --c89 --no_warnings $*structs.ic
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GenerateBlobC -props $*structs.properties -cfile $*structs.ic -xmlfile $*structs.xml -outfile $@ -j9flags ../buildspecs/j9.flags $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS))
	-rm -f $*structs.c $*structs.i $*structs.ic $*structs.xml
	
#OMR blob generation
omrddrblob.c: omrddrstructs.properties
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GenerateInputCFile omrddrstructs.properties omrddrstructs.c
#linux specific operations 
ifneq (,$(findstring linux_x86,$(PLATFORM)))
	chmod 755 ../buildtools/extract_structures/${PLATFORM}/extract_structures
	cpp -undef -nostdinc -include typestubs.h -Idummy_headers $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS)) -o omrddrstructs.i omrddrstructs.c
endif
#windows specific operations
ifneq (,$(findstring win_x86,$(PLATFORM)))
	-chmod 755 ../buildtools/extract_structures/${PLATFORM}/extract_structures.exe
	cl /P -u -X /FItypestubs.h -Idummy_headers $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS)) /Foomrddrstructs.i omrddrstructs.c
endif
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.StripGarbage omrddrstructs.i
	../buildtools/extract_structures/${PLATFORM}/extract_structures --c89 --no_warnings omrddrstructs.ic
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GenerateBlobC -props omrddrstructs.properties -cfile omrddrstructs.ic -xmlfile omrddrstructs.xml -outfile $@ -j9flags ../buildspecs/j9.flags $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS))
	-rm -f omrddrstructs.c omrddrstructs.i omrddrstructs.ic omrddrstructs.xml

#JIT blob generation
jitddrblob.c: jitddrstructs.properties
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GenerateInputCFile jitddrstructs.properties jitddrstructs.c
#linux specific operations
ifneq (,$(findstring linux_x86,$(PLATFORM)))
	chmod 755 ../buildtools/extract_structures/${PLATFORM}/extract_structures
	cpp -nostdinc -include typestubs.h -Idummy_headers $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS)) -o jitddrstructs.i jitddrstructs.c
endif
#windows specific operations
ifneq (,$(findstring win_x86,$(PLATFORM)))
	-chmod 755 ../buildtools/extract_structures/${PLATFORM}/extract_structures.exe
	cl /P -X /FItypestubs.h -Idummy_headers $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS)) /Fojitddrstructs.i jitddrstructs.c
endif
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.StripGarbage jitddrstructs.i
	../buildtools/extract_structures/${PLATFORM}/extract_structures --c89 --no_warnings jitddrstructs.ic
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GenerateBlobC -props jitddrstructs.properties -cfile jitddrstructs.ic -xmlfile jitddrstructs.xml -outfile $@ -j9flags ../buildspecs/j9.flags $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS))
	-rm -f jitddrstructs.c jitddrstructs.i jitddrstructs.ic jitddrstructs.xml

#GC blob generation
%blob.cpp: %structs.properties
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GenerateInputCFile $*structs.properties $*structs.cpp
#linux specific operations 
ifneq (,$(findstring linux_x86,$(PLATFORM)))
	chmod 755 ../buildtools/extract_structures/${PLATFORM}/extract_structures
	cpp -undef -nostdinc -include $(UMA_PATH_TO_ROOT)/ddr/typestubs.h -I$(UMA_PATH_TO_ROOT)/ddr/dummy_headers -DTYPESTUB_CPLUSPLUS=1 $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS)) -o $*structs.i $*structs.cpp
endif
#windows specific operations
ifneq (,$(findstring win_x86,$(PLATFORM)))
	-chmod 755 ../buildtools/extract_structures/${PLATFORM}/extract_structures.exe
	cl /P -u -X /FI$(UMA_PATH_TO_ROOT)/ddr/typestubs.h -I$(UMA_PATH_TO_ROOT)/ddr/dummy_headers -DTYPESTUB_CPLUSPLUS=1 $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS)) -o $*structs.i $*structs.cpp
endif
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.StripGarbage $*structs.i
	../buildtools/extract_structures/${PLATFORM}/extract_structures --c++ --no_warnings $*structs.ic
	$(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GenerateBlobC -props $*structs.properties -cfile $*structs.ic -xmlfile $*structs.xml -outfile $@ -j9flags ../buildspecs/j9.flags $(shell $(JAVA) -cp ../buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.ExtractCPPFlags $(CFLAGS))
	-rm -f $*structs.cpp $*structs.i $*structs.ic $*structs.xml


