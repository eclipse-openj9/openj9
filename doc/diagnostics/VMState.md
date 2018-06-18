<!--
Copyright (c) 2018, 2018 IBM Corp. and others

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
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# VM state

The VM state is a numeric field stored in `J9VMThread->omrVMThread->vmState`
that uses 32-bits to encode the current state of the thread. See the
`J9VMSTATE_` defined constants in the [code][1]. For example, if the
thread is currently JIT compiling a method, the VM state indicates
which compilation phase the compiler is in. If the VM crashes for any
reason, the VM state is reported via standard error and in the
javacore dump (in which it is referred to as "VM flags"), and serves
as a first-order indicator of the failing component during problem
determination. 

[1]: https://github.com/eclipse/openj9/blob/master/runtime/oti/j9nonbuilder.h

## Decoding

The high order half of the 32-bit VM state indicates the component.

| Value  | Component |
| -----  | :-------: |
| 0x0000 | none |
| 0x0001 | interpreter |
| 0x0002 | garbage collector |
| 0x0003 | stack growth |
| 0x0004 | JNI code |
| 0x0005 | JIT compilation |
| 0x0006 | bytecode verifier |
| 0x0007 | runtime verifier |
| 0x0008 | shared class cache |
| 0x0011 | sniff-n-whack stack validator |

The VM includes a `-Xjit:vmstate=` command line option to decode a
VM state, see the following example. Note the "0x" and the three
leading zeros are required.

```
> java -Xjit:vmstate=0x000501ff
vmState [0x501ff]: {J9VMSTATE_JIT_CODEGEN} {inlining}
```

### JIT compiler

If the current active component is the JIT compiler, the low-order
half is divided into two bytes, one of which is 0xFF.

In the low-order half, if the lower byte is 0xFF, the current active
component is the optimizer, and the higher byte is the numeric
identifier of the current optimization (defined in an [enum][2]),
e.g. 0x000501FF means that the inliner is active.

If the higher byte is 0xFF, the current active component is the code
generator, and the lower byte is the numeric identifier of the current
code generation phase (defined in an [enum][3]), e.g. 0x0005FF05 means
that instruction selection is being performed.

If the entire low-order half of the VM state is 0xFFFF, then the
active component is likely to be in the compiler initialization or
the IL generator.

[2]: https://github.com/eclipse/omr/blob/master/compiler/optimizer/Optimizations.hpp
[3]: https://github.com/eclipse/openj9/blob/master/runtime/compiler/codegen/J9CodeGenPhaseEnum.hpp

## Finding the VM state examples

The VM state is shown as `vmState=0x0002001a` in the following GC
crash message.

```
Unhandled exception
Type=Segmentation error vmState=0x0002001a
J9Generic_Signal_Number=00000004 Signal_Number=0000000b Error_Value=00000000 Signal_Code=00000001
Handler1=0000040000E162B8 Handler2=0000040000F04338
R0=0000000000000000 R1=0000040000CDEFA0 R2=0000040001CA8B78 R3=7471B155457425F3
R4=0000000000000000 R5=0000000000000000 R6=0000040009C00008 R7=0000000000000000
R8=0000040009C00000 R9=0000000000000001 R10=0000000000000000 R11=0000000000000000
R12=0000040001BA7050 R13=0000040000CE9920 R14=000000001028E570 R15=00000000100A9200
```

In a javacore dump search for the `HFLAGS` to find the VM state.

```
2XHREGISTER      ...           
1XHEXCPMODULE  Compiling method: javax/naming/NameImpl.<init>(Ljava/util/Properties;)V
NULL           
1XHFLAGS       VM flags:0000000000051AFF
NULL    
NULL           ------------------------------------------------------------------------
0SECTION       ENVINFO subcomponent dump routine
NULL           =================================
1CIJAVAVERSION JRE 1.8.0 Linux amd64-64 (build 1.8.0-internal-openj9_2018_01_07_19_10-b00)
```
