<!--
Copyright IBM Corp. and others 2022

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->

# Overview
Debugging AOT can get quite tricky because of the fact that OpenJ9
uses [Dynamic AOT](README.md). Not only do we have to deal with the inherent
intermittency of the just-in-time nature of compilation, AOT also has
the added complexity of validation and relocation. This means that an
AOT bug could occur in the run the problem method was compiled, but more
often, it occurs in the subsequent runs which do not contain any of the
information in the original environment. This doc outlines the tools
and strategies one can employ when facing an AOT bug.

# Diagnostic Information
Since AOT involves a two step process, there are two different sources
of diagnostic information:

1. Compilation Tracing during an AOT Compile
2. RT Log Tracing during an AOT Load

If one has both, along with a system core dump, it significantly
simplifies the debugging effort.

## Compiler Trace Options for the Compilation Run
```
-Xaot:{<METHOD_NAME>}(traceFull,traceRelocatableDataCG,traceRelocatableDataDetailsCG,log=<PATH_TO_LOG>/log)
```
`traceRelocatableDataCG` and `traceRelocatableDataDetailsCG` print out
relocation related information; `traceFull` is mainly for context; less
verbose options might also suffice.

### Example 1
Generate a log by doing:
```
java -Xshareclasses:name=doc '-Xaot:{java/util/concurrent/ConcurrentHashMap.tabAt([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node;}(traceFull,traceRelocatableDataCG,traceRelocatableDataDetailsCG,log=log)' -version
```
The `log` file will contain:
```
<relocatableDataCG>
Code start = 14de6fc0, Method start pc = 14de7000, Method start pc offset = 0x40
java/util/concurrent/ConcurrentHashMap.tabAt([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node;


Relocation Record Generation Info
Type                                File                             Line  Offset(M) Offset(PC) Node
TR_RamMethod (21)                   /root/hostdir/openj9-openjdk-jdk11/omr/compiler/x/codegen/X86BinaryEncoding.cpp  2717      0012       ffffffd2 00007F470F0048B0
TargetAddress1:0000000000000000,  TargetAddress2:0000000000000000
TR_HCR (41)                         /root/hostdir/openj9-openjdk-jdk11/omr/compiler/x/codegen/X86BinaryEncoding.cpp  2830      0012       ffffffd2 00007F470F0048B0
TargetAddress1:00000000000AAAE0,  TargetAddress2:0000000000000000
TR_HelperAddress (1)                /root/hostdir/openj9-openjdk-jdk11/omr/compiler/x/codegen/X86BinaryEncoding.cpp  1088      0024       ffffffe4 00007F470F0048B0
TargetAddress1:00007F470F13D220,  TargetAddress2:0000000000000000
...
TR_InlinedVirtualMethod (55)        /root/hostdir/openj9-openjdk-jdk11/openj9/runtime/compiler/codegen/J9CodeGenerator.cpp  2774      eb219040       eb219000 0000000000000000
TargetAddress1:00007F470F23B8F0,  TargetAddress2:0000000000000000
TR_InlinedVirtualMethodWithNopGuard (34) /root/hostdir/openj9-openjdk-jdk11/openj9/runtime/compiler/codegen/J9CodeGenerator.cpp  2774      007e       003e 0000000000000000
TargetAddress1:00007F470F23B980,  TargetAddress2:0000000000000000
Size of relocation data in AOT object is 894 bytes
Size field in relocation data is 902 bytes

Address           Size Type                           Width EIP Index Offsets
         eb4b0a0  50   TR_InlinedVirtualMethodWithNopGuard (34)2     Abs       007e
Inlined Method: Inlined site index = 0, Constant pool = dd4ffa18, cpIndex = 1c, romClassOffsetInSharedCache=0000000000042F70, destinationAddress = 00007F4714DE7100
         eb4b0d2  44   TR_InlinedVirtualMethod (55)   4     Abs       eb219040
 Removed Guard inlined method: Inlined site index = 1, Constant pool = dd47a870, cpIndex = 78, romClassOffsetInSharedCache=0000000000042F70
...
         eb4b3f6  10   TR_HelperAddress (1)           2     Rel 20    0024
Helper method address of j2iTransition(20)
         eb4b400  18   TR_HCR (41)                    2     Abs       0012 HCR: patch location offset = 00000000000AAAE0
         eb4b412  12   TR_RamMethod (21)              2     Abs       0012 0242
</relocatableDataCG>
```

#### Header
```
Code start = 14de6fc0, Method start pc = 14de7000, Method start pc offset = 0x40
java/util/concurrent/ConcurrentHashMap.tabAt([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node;
```
This section contains information about the method being compiled.
It is worth noting that `Code start` here does not mean the address
of the method when it is loaded into the Code Cache; this is the
address in the temporarily allocated memory in the Code Cache used
for the purpose of compilation. At the end of compilation, the method
is stored into the SCC and loaded at a later point.

#### Relocation Record Generation Info
```
Type                                File                             Line  Offset(M) Offset(PC) Node
TR_InlinedVirtualMethodWithNopGuard (34) /root/hostdir/openj9-openjdk-jdk11/openj9/runtime/compiler/codegen/J9CodeGenerator.cpp  2774      007e       003e 0000000000000000
TargetAddress1:00007F470F23B980,  TargetAddress2:0000000000000000

```
This section contains information about the abstract notion of the
relocation record, namely the `TR::ExternalRelocationRecord`. Objects
of this type are iterated over, and used to generate the actual
relocation records that are stored into the SCC, described by the
`TR_RelocationRecordBinaryTemplate` family of structs.

* Type: `TR_InlinedVirtualMethodWithNopGuard` is the relocation type,
specifically the `TR_ExternalRelocationTargetKind` enum; `34` is the
value of the enum.
* File & Line: The cpp file and line number where this relocation is
generated.
* Offset (M): The offset from the start of the allocated memory for
this method this relocation applies to.
* Offset (PC): The offset from the Start PC of this method this
relocation applies to.
* Node: The `TR::Node` that is associated with this relocation record;
this is used to connect the relocation record with the IL. In the case
of validation or inlined site records, this may be NULL; however for the
former the node does not matter and for the latter, the inlined site
index can be determined from the next section below.
* Target Addresses: The two values passed into the constructor of the
`TR::ExternalRelocationRecord` is printed out; these values are used to
generate the values in the actual relocation record that gets stored into
the SCC.

#### Binary Data Info
```
Address           Size Type                           Width EIP Index Offsets
         eb4b0a0  50   TR_InlinedVirtualMethodWithNopGuard (34)2     Abs       007e
Inlined Method: Inlined site index = 0, Constant pool = dd4ffa18, cpIndex = 1c, romClassOffsetInSharedCache=0000000000042F70, destinationAddress = 00007F4714DE7100
```
This section contains information about the actual relocation record
that is stored into the SCC. This data is described by the
`TR_RelocationRecordBinaryTemplate` family of structs.
These structs are variable length; they are followed by a variable
number of 2 or 4 byte offsets.

* Address: `eb4b0a0` is the start address of the Binary Template Struct
associated with `TR_InlinedVirtualMethodWithNopGuard`;
it is only valid during the compilation as the entire buffer
is stored into the SCC.
* Size: The size of the entire variable length record; i.e. the size
of the Binary Template struct plus the total size of the variable
number of offsets that follow.
* Type: The `TR_ExternalRelocationTargetKind` representing the type of
relocation this is.
* Width: The size of the offsets in bytes; either 2 or 4. This value
depends on if all offsets need to be encoded in 4 bytes or if 2 bytes
are sufficient.
* EIP: Indicates if the relocation is relative to the Instruction
Pointer or not.
* Index: If the relocation type is `TR_HelperAddress` or
`TR_AbsoluteHelperAddress`, this is the helper ID. Otherwise nothing
is printed.
* Offsets: The list of offsets in the code the relocation record applies
to; the offset here is the `Offset (M)` from above.

Also printed out on the next line are the various fields in the Binary
Template; this will vary depending on the relocation kind.

## RT Log Options for the Load Run
```
-Xaot:aotrtDebugLevel=<AOTRT_LOG_LEVEL>,rtLog=<PATH_TO_RTLOG>/rtLog
```
`aotrtDebugLevel` is used to determine the level of verbosity; for
debugging AOT, it's probably best to use a high number like 10.

### Example 2
Generate a rtLog; this was run after Example 1 above.
```
java -Xshareclasses:name=doc '-Xaot:aotrtDebugLevel=10,rtLog=rtLog' -version
```
From OpenJ9 v0.31.0, an rtLog will be opened for each Compilation
Thread. Prior to this, the log should be generated with
`-XcompilationThreads1` to prevent overlapping data from different
threads. The `rtLog` file will contain:
```
relocateAOTCodeAndData jitConfig=00007F56D40BD9B0 aotDataCache=0000000000000000 aotMccCodeCache=00007F56D40C4BD0 method=00000000000AAAE0 tempDataStart=00007F56828012C4 exceptionTable=00007F56A26DE338 oldDataStart=00007F470EB4B030 codeStart=00007F56A8BB6880 oldCodeStart=00007F4714DE6FC0 classReloAmount=0000000000000001 cacheEntry=00007F56828012C4
relocateAOTCodeAndData: method java/util/concurrent/ConcurrentHashMap.tabAt([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node;
<relocatableDataMetaDataRT>
java/util/concurrent/ConcurrentHashMap.tabAt([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node;
startPC     endPC       size    gcStackAtlas  bodyInfo
0x7f4714de70000x7f4714de727d18a     0x7f470eb4b5220x7f470eb4b5c0
inlinedCalls
0x7f470eb4b500
</relocatableDataMetaDataRT>
relocateAOTCodeAndData: jitConfig=d40bd9b0 aotDataCache=0 aotMccCodeCache=d40c4bd0 method=aaae0 tempDataStart=828012c4 exceptionTable=a26de338
                        oldDataStart=eb4b030 codeStart=a8bb6880 oldCodeStart=14de6fc0 classReloAmount=1 cacheEntry=828012c4
                        tempDataStart: 00007F56828012C4, _aotMethodHeaderEntry: 00007F56828012CC, header offset: 8, binaryReloRecords: 00007F568280132C
TR_InlinedVirtualMethodWithNopGuard 00007F5682801334
        size 32 type 34 flags 0 reloFlags 3
        inlined site index 0000000000000000
        constant pool 00007F46DD4FFA18
        cpIndex 000000000000001C
        romClassOffsetInSharedCache 42f70 jdk/internal/misc/Unsafe
        destinationAddress 00007F4714DE7100
        validateSameClasses: caller method 00000000000AAAE0
        inlinedSiteValid: caller method java/util/concurrent/ConcurrentHashMap.tabAt([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node;
        inlinedSiteValid: cp 00000000000A9720
        getMethodFromCP: found virtual method 0000000000050E58
        inlinedSiteValid: compileRomClass 00007F567147A7B8 currentRomClass 00007F567147A7B8
        inlinedSiteValid: inlined method jdk/internal/misc/Unsafe.getObjectAcquire(Ljava/lang/Object;J)Ljava/lang/Object;
        fixInlinedSiteInfo: [00007F56A26DE410] set to 0000000000050E58, virtual guard address 00007F56B8BC62D0
        preparePrivateData: ramMethod 0000000000050E58 inlinedSiteIsValid 1
        preparePrivateData: guard backup destination 00007F56A8BB69C0
        reloLocation: from 00007F5682801364 at 00007F56A8BB68FE (offset 7e)
                applyRelocation: activating inlined method
TR_InlinedVirtualMethod 00007F5682801366
        size 2c type 55 flags 80 reloFlags 3
        Flag: Wide offsets
        inlined site index 0000000000000001
        constant pool 00007F46DD47A870
        cpIndex 0000000000000078
        romClassOffsetInSharedCache 42f70 jdk/internal/misc/Unsafe
        validateSameClasses: caller method 0000000000050E58
        inlinedSiteValid: caller method jdk/internal/misc/Unsafe.getObjectAcquire(Ljava/lang/Object;J)Ljava/lang/Object;
        inlinedSiteValid: cp 0000000000053EB0
        getMethodFromCP: found virtual method 000000000004FCD8
        inlinedSiteValid: compileRomClass 00007F567147A7B8 currentRomClass 00007F567147A7B8
        inlinedSiteValid: inlined method jdk/internal/misc/Unsafe.getObjectVolatile(Ljava/lang/Object;J)Ljava/lang/Object;
        fixInlinedSiteInfo: [00007F56A26DE421] set to 000000000004FCD8, virtual guard address 00007F56A8BB69C0
        preparePrivateData: ramMethod 000000000004FCD8 inlinedSiteIsValid 1
        reloLocation: from 00007F568280138E at 00007F5693DCF8C0 (offset eb219040)
                applyRelocation: activating inlined method
...
TR_HelperAddress 00007F568280168A
        size a type 1 flags 40 reloFlags 0
        Flag: EIP relative
        helper 20 j2iTransition
        preparePrivateData: helperAddress 00007F56C68654A0
        reloLocation: from 00007F5682801692 at 00007F56A8BB68A4 (offset 24)
                applyRelocation: baseLocation 00007F56A8BB68A8 helperAddress 00007F56C68654A0 helperOffset 1dcaebf8
TR_HCR 00007F5682801694
        size 12 type 41 flags 0 reloFlags 0
        offset aaae0
        action: hcrEnabled 1
        reloLocation: from 00007F56828016A4 at 00007F56A8BB6892 (offset 12)
TR_RamMethod 00007F56828016A6
        size c type 21 flags 0 reloFlags 0
        reloLocation: from 00007F56828016AE at 00007F56A8BB6892 (offset 12)
                applyRelocation: method pointer 00000000000AAAE0
        reloLocation: from 00007F56828016B0 at 00007F56A8BB6AC2 (offset 242)
                applyRelocation: method pointer 00000000000AAAE0
relocateAOTCodeAndData: return code 0
java/util/concurrent/ConcurrentHashMap.tabAt([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node; <0x7f56a8bb68c0-0x7f56a8bb6b3d>  Time: 942 usec
```
#### SCC Entry Information
```
relocateAOTCodeAndData jitConfig=00007F56D40BD9B0 aotDataCache=0000000000000000 aotMccCodeCache=00007F56D40C4BD0 method=00000000000AAAE0 tempDataStart=00007F56828012C4 exceptionTable=00007F56A26DE338 oldDataStart=00007F470EB4B030 codeStart=00007F56A8BB6880 oldCodeStart=00007F4714DE6FC0 classReloAmount=0000000000000001 cacheEntry=00007F56828012C4
relocateAOTCodeAndData: method java/util/concurrent/ConcurrentHashMap.tabAt([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node;
```
This section contains information about the method being loaded from
the SCC.

#### Relocated Exception Table Info
```
<relocatableDataMetaDataRT>
java/util/concurrent/ConcurrentHashMap.tabAt([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node;
startPC     endPC       size    gcStackAtlas  bodyInfo
0x7f4714de70000x7f4714de727d18a     0x7f470eb4b5220x7f470eb4b5c0
inlinedCalls
0x7f470eb4b500
</relocatableDataMetaDataRT>
```
This section prints out some of the fields in the relocated
`J9JITExceptionTable` struct that will be associated with the loaded
method.

#### Pre Relocation Information
```
relocateAOTCodeAndData: jitConfig=d40bd9b0 aotDataCache=0 aotMccCodeCache=d40c4bd0 method=aaae0 tempDataStart=828012c4 exceptionTable=a26de338
                        oldDataStart=eb4b030 codeStart=a8bb6880 oldCodeStart=14de6fc0 classReloAmount=1 cacheEntry=828012c4
                        tempDataStart: 00007F56828012C4, _aotMethodHeaderEntry: 00007F56828012CC, header offset: 8, binaryReloRecords: 00007F568280132C
```
This section contains much of the same information as before, but some
additional information about the state of the relocation infrastructure
prior to relocation.

#### Validation / Relocation Trace
```
TR_InlinedVirtualMethodWithNopGuard 00007F5682801334
        size 32 type 34 flags 0 reloFlags 3
        inlined site index 0000000000000000
        constant pool 00007F46DD4FFA18
        cpIndex 000000000000001C
        romClassOffsetInSharedCache 42f70 jdk/internal/misc/Unsafe
        destinationAddress 00007F4714DE7100
        validateSameClasses: caller method 00000000000AAAE0
        inlinedSiteValid: caller method java/util/concurrent/ConcurrentHashMap.tabAt([Ljava/util/concurrent/ConcurrentHashMap$Node;I)Ljava/util/concurrent/ConcurrentHashMap$Node;
        inlinedSiteValid: cp 00000000000A9720
        getMethodFromCP: found virtual method 0000000000050E58
        inlinedSiteValid: compileRomClass 00007F567147A7B8 currentRomClass 00007F567147A7B8
        inlinedSiteValid: inlined method jdk/internal/misc/Unsafe.getObjectAcquire(Ljava/lang/Object;J)Ljava/lang/Object;
        fixInlinedSiteInfo: [00007F56A26DE410] set to 0000000000050E58, virtual guard address 00007F56B8BC62D0
        preparePrivateData: ramMethod 0000000000050E58 inlinedSiteIsValid 1
        preparePrivateData: guard backup destination 00007F56A8BB69C0
        reloLocation: from 00007F5682801364 at 00007F56A8BB68FE (offset 7e)
                applyRelocation: activating inlined method
```
This section will vary depending on the relocation relocation. It is
worth noting that the data printed out at the start of this section
matches what was printed in the compile run. Also worth noting is
the relocation line:
```
        reloLocation: from 00007F5682801364 at 00007F56A8BB68FE (offset 7e)
```
Again, the offset here is the `Offset (M)`. If a relocation record
applies to multiple offsets, then there will be multiple lines printed.
`00007F5682801364` is the instance of the `TR_RelocationRecord` type
(or subtype); `00007F56A8BB68FE` is the address in the code the
relocation is applied to. Not all relocations involve updating the
code; some will use the relocated address to do other tasks such as
registering Runtime Assumptions.

# Common reasons for AOT bugs

### Missing or incorrect relocation
When adding a new feature, or enabling a disabled code path, it is
important to think about AOT. If the compiler generates or emits
something that is **not** position **independent**, then the compiled
code may not work in another JVM instance. Depending on the position
dependent code, the error may manifest itself in a way that is not
trivial to understand. An example of this might be if the compiler
materializes a `J9Class` pointer using some means other than the
existing front end queries that do ensure that a relocation record
is generated, and emits it into the code. Even when the method is
loaded from the SCC in the current instance some time later, there
won't be a problem because the class pointer will be valid. However,
in another JVM instance, that same class may be located at a
different address.

### Missing or incorrect Validation
In a similar vein, if the compiler generates or emits something based
on the environment in one JVM instance, that assumption may not hold
in another JVM instance. Bugs due to missing validations are notoriously
difficult to pin down. An example of this might be if the compiler
makes an assumption about the relationship between two classes.
Because of the dynamic nature of Java, that relationship is not
guaranteed to hold in another JVM instance. As before, when the method is
loaded in the current instance there won't be a problem.

### Invalid code
This can happen when one tries to enable code that was disabled under
AOT. Because of the requirement that all interactions between the
compiler and the runtime have to be recorded in some manner, AOT
compilations often have assumptions that lean towards being more
conservative. When trying to allow certain optimizations, if not all
areas are updated to have a consistent set of assumptions, it can lead
to problems. Often, bugs of this nature are easier to deal with as
they will also occur in the compile run.

# Debugging Strategies
Much of the strategies one uses to narrow down the problematic method in
a JIT compile apply here. However, there are some additional options
to know about
- `limit=`, `exclude=`, `limitFile=` apply only to compilations. It is
worth noting that this will limit both JIT **and** AOT compilations
regardless of whether it's set on `-Xjit` or `-Xaot`.
- `loadLimit=`, `loadExclude=`, `loadLimitFile=` apply only to loads;
these can only be set on `-Xaot`.

Once the problem method is found, one should try and see if the problem
narrows down to one of the three common problems above.

A missing or incorrect relocation is relatively straightforward if one
can find a location in the code that has an invalid address;
cross-referencing with the RT Log can confirm whether the relocation
exists or was applied properly. An example of such a bug can be found
[here](https://github.com/eclipse-openj9/openj9/issues/14300). Similarly,
dealing with invalid code is also often straightforward,
especially if the problem occurs in the compile run, as it boils down to
the usual strategies required to debug problems in a JIT compilation.
As this often occurs development, there is no existing issue to point
to.

A missing or incorrect validation is unfortunately rarely straightforward.
Often it involves diving deep into the core dump and the trace logs to
understand exactly where things go wrong. Once a likely culprit is
found, the logs can then be cross-referenced to verify whether
a validation existed or was incorrect. An example of such a bug can be
found [here](https://github.com/eclipse-openj9/openj9/issues/9710).

# Other related information in the RTLog
The RT Log also contains information printed by the `TR_J9SharedCache`
object, which is used by the compiler to interface with the SCC. If the
RT Log is enabled in the compile run, it will contain information
such as:
```
TR_J9SC:rememberClass class 000000000004E700 romClass 00007F607D477448 com/ibm/jit/JITHelpers
TR_J9SC:        key created: 00003c89
TR_J9SC:        creating chain now: 1 + 1 + 1 superclasses + 0 interfaces
TR_J9SC:                Chain 00007F60B5BB94B0 store chainLength 24
TR_J9SC:                Chain 00007F60B5BB94B8 storing romclass 00007F607D477448 (com/ibm/jit/JITHelpers) offset 247952
TR_J9SC:                writeClassesToChain:
TR_J9SC:                Chain 00007F60B5BB94C0 storing romclass 00007F607D459000 (java/lang/Object) offset 0
TR_J9SC:                writeInterfacesToChain:
TR_J9SC:                fillInClassChain returning true
TR_J9SC:        stored data, chain at 00007F608E8060D0
```
This logs the process of creating and storing the Class Chain to the
SCC, which is required as part of any validation record for a
`J9Class`. In the load run, as part of a validation record, the
`TR_J9SharedCache` will print out:
```
TR_J9SC:classMatchesCachedVersion class 000000000004E700 com/ibm/jit/JITHelpers
TR_J9SC:        no chain specific, so looking up for key 00003c89
TR_J9SC:        found chain: 00007F608E8060D0 with length 24
TR_J9SC:                Examining romclass 00007F607D477448 (com/ibm/jit/JITHelpers) offset 247952, comparing to 247952
TR_J9SC:                Examining romclass 00007F607D459000 (java/lang/Object) offset 0, comparing to 0
TR_J9SC:        Match!  return true
```
