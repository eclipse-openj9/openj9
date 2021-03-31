<!--
Copyright (c) 2021, 2021 IBM Corp. and others

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

# Mainline and out of line code
The mainline path handles the common operations in a straight line to avoid branching or using branch prediction resources. Arranging the commonly run code on the mainline also helps reserve instruction cache. The work can be done in the same cache line by falling through. There is no need to jump to another spot to bring in an entire new cache line to the cache.

Executing the common code on the mainline is not only adopted in IL but also in the code gen, such as the non-trivial logics: write barrier, instanceof, etc. The instructions that are believed to run commonly are arranged on the mainline path. Branching is chosen for the less common code.

One example in the code gen is the bound check. It does the comparison between arraylength and index. If it fails, an exception is going to be raised.  Then it will branch to an out of line, or a snippet of code that jumps into the VM to throw the exception that needs to be thrown. Because the path that jumps to throw the exception is expected to be very rarely called, the snippet code is put at the end of the code section, or at the end of the generated code for that method, aka out of line, so that it does not interfere with the code that we believe will actually run.

Another example is block ordering and extension of basic blocks in IL. Both uses the same logic to arrange the blocks. For example there is a block ending with `if`. One path is marked as cold. Another path is not marked as cold. Block ordering takes the cold path out of the mainline, puts it at the end of the method, and keeps the non-cold path as the fall through. This way it avoids consuming branch prediction and instruction cache resources.


# Global register dependency

`GlRegDeps` at its core is really a code gen register dependency. It associates virtual registers with real registers, or pairs virtual registers with real registers.

Register dependencies enforce constraints. It says that this value has to be in this register at this time. Global register dependency is fundamentally about conveying particular values in particular registers across the block boundaries. The values and registers are expressed by `GlRegDeps` from one basic block to another.

Most common control flow opcodes are: `if`, `goto`, `return`, `athrow`, `switch`. They should be the last tree before the `BBEnd`. They should have their own `GlRegDeps` for the branch target if required. The `GlRegDeps` under `BBEnd` is for the fall through case.

If a value that is expected to be in a register is not in the register, or the register is not preserved if it is a call, it leads to register shuffling. The local register allocator has to shuffle it to a different register to make sure it has the right value in `GlRegDeps`. Register shuffling could come with a cost.

The local register allocator and the code gen use the reference count for `GlRegDeps` to determine when a register is no longer needed. It needs to be set up correctly, otherwise it could result in registers being overwritten with some other values prematurely.

The `GlRegDeps` nodes should have been evaluated before they get to `GlRegDeps`. The `GlRegDeps` job is to take values that are already in virtual registers and simply map them to real registers. `GlRegDeps` is only about generating the register dependency. It should not be about generating any other instructions.

The following example of `GlRegDeps` will cause issues. It does the load operation (`n106n`) and the add operation (`n110n`) in the middle of laying down register dependencies. These actual tree evaluations interfere with the register dependencies which would end up interfereing with branching.

```
n804n       GlRegDeps ()                                                                      [0x7f9d0f08eb00] bci=[-1,111,600] rc=1 vc=1350 vn=- li=13 udi=- nc=3 flg=0x20
n805n         PassThrough rcx                                                                 [0x7f9d0f08eb50] bci=[-1,117,600] rc=1 vc=1350 vn=- li=13 udi=- nc=1
n111n           aload  tmp<auto slot 4>[#358  Auto] [flags 0x4007 0x0 ]                       [0x7f9d0f081270] bci=[-1,117,600] rc=6 vc=1350 vn=- li=13 udi=- nc=0
n806n         PassThrough r8                                                                  [0x7f9d0f08eba0] bci=[-1,116,600] rc=1 vc=1350 vn=- li=13 udi=- nc=1
n110n           iadd (cannotOverflow )                                                        [0x7f9d0f081220] bci=[-1,116,600] rc=5 vc=1350 vn=- li=13 udi=- nc=2 flg=0x1000
n107n             ==>iloadi
n109n             iconst -1 (X!=0 X<=0 )                                                      [0x7f9d0f0811d0] bci=[-1,115,600] rc=1 vc=1350 vn=- li=13 udi=- nc=0 flg=0x204
n807n         PassThrough rdi                                                                 [0x7f9d0f08ebf0] bci=[-1,111,600] rc=1 vc=1350 vn=- li=13 udi=- nc=1
n106n           ==>aload
```

These nodes should be anchored under `xRegStore` earlier in the block. By anchoring these nodes earlier in the block, it ensures that they get evaluated earlier than `GlRegDeps` so that it does not interfere with the control flow. For example the following `n190n` for AND operation is anchored under `iRegStore` (`n558n`).
```
n541n     treetop                                                                             [0x7fec3b0b98d0] bci=[1,29,-] rc=0 vc=0 vn=- li=- udi=- nc=1
n190n       iand (X>=0 cannotOverflow )                                                       [0x7fec3b0b2b20] bci=[1,29,-] rc=5 vc=1095 vn=- li=- udi=- nc=2 flg=0x1100
n185n         iadd (X>=0 cannotOverflow )                                                     [0x7fec3b0b2990] bci=[1,21,-] rc=1 vc=1095 vn=- li=- udi=- nc=2 flg=0x1100
n183n           iload  n<auto slot 5>[#370  Auto] [flags 0x3 0x0 ] (X>=0 cannotOverflow )     [0x7fec3b0b28f0] bci=[1,18,-] rc=1 vc=1095 vn=- li=- udi=34 nc=0 flg=0x1100
n184n           iconst -1 (X!=0 X<=0 )                                                        [0x7fec3b0b2940] bci=[1,20,-] rc=1 vc=1095 vn=- li=- udi=- nc=0 flg=0x204
n505n         ==>iRegLoad
n542n     treetop                                                                             [0x7fec3b0b9920] bci=[1,17,-] rc=0 vc=0 vn=- li=- udi=- nc=1
n506n       ==>aRegLoad
n558n     iRegStore edx                                                                       [0x7fec3b0b9e20] bci=[1,29,-] rc=0 vc=0 vn=- li=- udi=- nc=1
n190n       ==>iand
n582n     ificmpne --> block_57 BBStart at n553n ()                                           [0x7fec3b0ba5a0] bci=[1,30,-] rc=0 vc=0 vn=- li=- udi=- nc=3 flg=0x20
n580n       iand                                                                              [0x7fec3b0ba500] bci=[1,30,-] rc=1 vc=0 vn=- li=- udi=- nc=2
n578n         iloadi  <isClassFlags>[#287  Shadow +36] [flags 0x603 0x0 ]                     [0x7fec3b0ba460] bci=[1,30,-] rc=1 vc=0 vn=- li=- udi=- nc=1
n577n           aloadi  <componentClass>[#282  Shadow +88] [flags 0x10607 0x0 ]               [0x7fec3b0ba410] bci=[1,30,-] rc=1 vc=0 vn=- li=- udi=- nc=1
n576n             aloadi  <vft-symbol>[#288  Shadow] [flags 0x18607 0x0 ]                     [0x7fec3b0ba3c0] bci=[1,30,-] rc=1 vc=0 vn=- li=- udi=- nc=1
n506n               ==>aRegLoad
n579n         iconst 1024                                                                     [0x7fec3b0ba4b0] bci=[1,30,-] rc=1 vc=0 vn=- li=- udi=- nc=0
n581n       iconst 0                                                                          [0x7fec3b0ba550] bci=[1,30,-] rc=1 vc=0 vn=- li=- udi=- nc=0
n583n       GlRegDeps ()                                                                      [0x7fec3b0ba5f0] bci=[1,30,-] rc=1 vc=0 vn=- li=- udi=- nc=3 flg=0x20
n506n         ==>aRegLoad
n505n         ==>iRegLoad
n584n         PassThrough rdx                                                                 [0x7fec3b0ba640] bci=[1,29,-] rc=1 vc=0 vn=- li=- udi=- nc=1
n190n           ==>iand
n572n     BBEnd </block_33>                                                                   [0x7fec3b0ba280] bci=[1,30,-] rc=0 vc=0 vn=- li=- udi=- nc=0
```

The `xRegStore` serves the purpose of anchoring those children that are honored and ensuring that they get evaluated before the `GlRegDeps` gets encountered. However, the `xRegStore` does not make sure that the value is in that real register at the point of `xRegStore`.

`xRegStore` is not responsible for putting the value into the register. It is not actually going to do anything at that point other than evaluates the child and associates it with a virtual register. The actual moving off that value into the right real register is dictated by `GlRegDeps` at the end of block. The local register allocator in the code gen happens backwards. It walks backwards through the instructions. It will see `GlRegDeps` first, or the result of `GlRegDeps` first. It associates the virtual register with the real register the first time.

The purpose of `xRegStore` and `GlRegDeps` is only to specify which value should be in which real register at those points while transitioning from one block to another. It does not mandate the value has to be in this real register at this point. It ensures the right values are in the right real registers at those transition points.


# Check IL Rules
The IL rules can be checked by enabling `TR_EnableParanoidOptCheck` which does IL validation, tree verification, block and CFG verification at the end of the optimization to catch structural problems at the compile time.
