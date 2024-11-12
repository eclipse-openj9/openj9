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

## Table of Contents
---

When a method is chosen to be compiled, it is handed to the IL(Intermediate Language) generator first.
The IL generator translates Java bytecodes to IL trees.

Next, IL trees are processed by many independent optimization passes in the Optimizer.
The optimizations first analyze the IL trees to find opportunities to optimize. Secondly,
they transform the IL trees for better performance. Each method is compiled at different optimization
levels based on its hotness such as cold, warm, etc.

Code generators translate IL trees to assembly instructions for the target architecture.
They first generate machine instructions wherein the result of each (value-producing) IL node is placed
in a virtual register. Register Assignment (RA) replaces virtual registers with real registers.
In the end code generators perform binary encoding to write the appropriate bits to the code cache buffer.

```
                                     Java Bytecode
                                           |
                    +----------------------+--------------------+
                    | Compilation Thread   |                    |
                    |           +----------v---------+          |
                    |           |          IL        |          |
 +-------------+    |           | +----------------+ |          |
 | Compilation |    |           | |  IL Generator  | |          |
 |   Control   +---->           | +----------------+ |          |
 +------^------+    |           +----------+---------+          |
        |           |                      |                    |
        |           |     +----------------v----------------+   |
 +------v-----+     |     |            Optimizer            |   |
 |            |     |     | +----------+ +----------------+ |   |
 |  Profiler  <----->     | | Analyses | |  Optimizations | |   |
 |            |     |     | +----------+ +----------------+ |   |
 +------------+     |     +----------------+----------------+   |
                    |                      |                    |
                    |  +-------------------v------------------+ |
                    |  |            Code Generators           | |
                    |  | +-----+ +---+ +---------+ +--------+ | |
                    |  | | X86 | | Z | | PowerPC | |  ARM   | | |
                    |  | +-----+ +---+ +---------+ +--------+ | |
                    |  +-------------------+------------------+ |
                    |                      |                    |
                    +----------------------+--------------------+
                                           |
+-----------------------------+      +-----+-----+
|          Runtime            |      |           |
| +----------+ +------------+ |+-----v----+  +---v--+
| | JIT Hooks| | RT Helpers | || Metadata |  | Code |
| +----------+ +------------+ |+----------+  +------+
+-----------------------------+
```

* <details><summary><b>1. Fundamental Data Structures JIT Operates on</b></summary>

  * OpenJ9 Object Model
    * [Object Lock Word](../features/ObjectLockword.md)
  * C vs Java Stack
  * Heap and Thread Local Storage
</details>

* <details><summary><b>2. IL</b></summary>

  * Overview of IL Generator (OMR)
  * Introduction on Nodes, Trees, Treetops, Blocks, CFGs, and Structures (OMR)
    * [Testarossa's Intermediate Language (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/il/IntroToTrees.md)
  * [Symbols, Symbol References, and Aliasing in the OMR compiler (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/il/SymbolsSymrefsAliasing.md)
  * Reference of IL OpCodes
    * [IL OpCodes (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/il/ILOpcodes.md)
    * [Global Register Dependency (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/il/GlRegDeps.md)
  * [Node Transmutation (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/il/Node.md)
  * [Things to Consider When Adding a New IL Opcode (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/il/ExtendingTrees.md)
  * [IL FAQs (OpenJ9)](il/IL_FAQ.md)
  * [Tril (OMR)](https://github.com/eclipse-omr/omr/tree/master/doc/compiler/tril)
</details>

* <details><summary><b>3. Optimizer</b></summary>

  * Overview of Optimizer
    * [Dynamic Optimization Design](optimizer/OptimizerDesignFeatures.md)
  * Analyses
    * [Data Flow Analysis (YouTube)](https://youtu.be/YCCdJ1Qphao)
  * [Local Optimizations](optimizer/LocalOptimizationsSummary.md)
  * [Global Optimizations](optimizer/GlobalOptimizationsSummary.md)
    * Escape Analysis
      * [Escape Analysis Optimization in OpenJ9 (YouTube)](https://youtu.be/694S8Tblfcg)
    * [Data-Flow Engine (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/DataFlowEngine.md)
  * [Control Flow Optimizations](optimizer/ControlFlowOptimizationsSummary.md)
    * [Exception Directed Optimization (EDO)](optimizer/EdoOptimization.md)
  * Inlining
    * [Overview of Inlining (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/Inliner.md)
    * [BenefitInliner (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/BenefitInliner.md)
    * [Inline Fast Path Locations (OpenJ9)](optimizer/inlineFastPathLocations.md)
  * [Loop Optimizations](optimizer/LoopOptimizationSummary.md)
    * [Introduction on Loop Optimizations (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/IntroLoopOptimizations.md)
  * Value Propagation
    * [Value Propagation (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/ValuePropagation.md)
    * [Overview of Value Propagation Compiler Optimization (YouTube)](https://youtu.be/694S8Tblfcg)
  * Hot Code Replacement (HCR)
    * [Next Gen HCR (OpenJ9)](hcr/OSR.md)
  * On Stack Replacement (OSR)
    * [Introduction to OSR (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/osr/OSR.md)
    * [OSR-based Optimizations (OpenJ9)](hcr/OSR.md)
    * [Improved JVM Debug Mode Based on OSR (OpenJ9)](https://blog.openj9.org/2019/04/30/introduction-to-full-speed-debug-base-on-osr/)
  * [Method Handles (OpenJ9)](methodHandles/MethodHandles.md)
</details>

* <details><summary><b>4. Code Generator</b></summary>

  * Overview of Code Generator
    * [Code Generators and Much More (Part I) (YouTube)](https://youtu.be/ClhkRtWFeds)
    * [Code Generators and Much More (Part II) (YouTube)](https://youtu.be/1WmQhLpyjZE)
  * Tree Evaluator
  * Register Assignment
  * X86
    * [X86 Binary Encoding Scheme (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/x/OpCodeEncoding.md)
  * PowerPC
  * s390
  * ARM
</details>

* <details><summary><b>5. Compilation Control</b></summary>

  * [Overview of Compilation Control](control/CompilationControl.md)
  * [Options Processing](control/OptionsProcessing.md)
  * [Options Processing Post Restore](control/OptionsPostRestore.md)
  * [Checkpoint Restore Coordination](control/CheckpointRestoreCoordination.md)
</details>

* <details><summary><b>6. Profiling</b></summary>

  * JProfiling
    * [JProfiling (OpenJ9)](jprofiling/JProfiling.md)
    * [JProfiling (YouTube)](https://youtu.be/SSlLZlOErvc)
  * [IProfiler (OpenJ9)](https://github.com/eclipse-openj9/openj9/issues/12509)
</details>

* <details><summary><b>7. Runtime</b></summary>

  * Overview of Runtime
  * [Code Metadata and Code Metadata Manager (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/runtime/CodeMetaData.md)
  * [Code Cache Reclamation (OpenJ9)](runtime/CodeCacheReclamation.md)
  * [Metadata Reclamation (OpenJ9)](runtime/MetadataReclamation.md)
  * [JIT Hooks (OpenJ9)](runtime/JITHooks.md)
  * [Recompilation (OpenJ9)](runtime/Recompilation.md)
  * [Exception Handling (OpenJ9)](runtime/ExceptionHandling.md)
  * [Runtime Assumption (OpenJ9)](runtime/RuntimeAssumption.md)
  * [ELF Generator (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/runtime/ELFGenerator.md#elfgenerator)
  * [Dynamic Loop Transfer (DLT) (OpenJ9)](https://github.com/eclipse-openj9/openj9/issues/12505)
  * Stack Walker
  * [J9JITExceptionTable (OpenJ9)](runtime/J9JITExceptionTable.md)
</details>

* <details><summary><b>8. Memory</b></summary>

  * [OMR Compiler Memory Manager (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/memory/MemoryManager.md)
  * [OpenJ9 Compiler Memory Manager (OpenJ9)](memory/MemoryManager.md)
  * [Allocating Memory in the Compiler (OpenJ9)](https://blog.openj9.org/2018/06/28/allocating-memory-in-the-compiler/)
</details>

* <b>9. [AOT](aot)</b>
* <b>10. [JITServer](jitserver)</b>

* <details><summary><b>11. Concepts</b></summary>

  * Extensible Classes
    * [Extensible Classes (OMR)](https://github.com/eclipse-omr/omr/tree/master/doc/compiler/extensible_classes)
    * [Extensible Classes (YouTube)](https://youtu.be/MtsOdx_1hug)
  * [Mainline and Out of Line Code (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/il/MainlineAndOutOfLineCode.md)
  * [Regular Expression (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/regular_expression/Regular_Expression.md)
  * [JIT Write Barriers (OpenJ9)](concepts/JitWriteBarriers.md)
</details>

* <details><summary><b>12. Debug</b></summary>

  * [Problem Determination Guide (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/ProblemDetermination.md)
  * [Introduction on Reading JIT Compilation Logs (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/IntroReadLogFile.md)
  * [Part 1: Diagnosing Compilation Problems Using the JIT Verbose Log (YouTube)](https://youtu.be/xG9d4VVRltc)
  * [Part 2: Diagnosing Compilation Problems Using the JIT Verbose Log (YouTube)](https://youtu.be/S4DSOuIcUo4)
  * [Command-line Options](https://www.eclipse.org/openj9/docs/cmdline_specifying/)
    * [Compiler Options (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/CompilerOptions.md)
  * [Lightning Talks - Verbose JIT Logs (YouTube)](https://youtu.be/-bQzHMisg_Y)
  * [Lightning Talks - JIT Compiler Logs (YouTube)](https://youtu.be/cwCaXQD9PgQ)
  * [Debug Counters (OMR)](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/debug/DebugCounters.md)
  * JitDump
    * [Debugging with JitDump (YouTube)](https://youtu.be/hfl6511x8LA)
    * [JIT Dump (OpenJ9)](https://github.com/eclipse-openj9/openj9/issues/12521)
</details>

* <details><summary><b>13. Testing</b></summary>

  * [OpenJ9 Test Quick Start (OpenJ9)](https://github.com/eclipse-openj9/openj9/tree/master/test)
  * [OpenJ9 Test User Guide (OpenJ9)](https://github.com/eclipse-openj9/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md)
  * [Reproducing Test Failures Locally (OpenJ9)](https://github.com/eclipse-openj9/openj9/wiki/Reproducing-Test-Failures-Locally)
  * [AQA Lightning Talk Series (OpenJ9)](https://github.com/eclipse-openj9/openj9/wiki/AQA-Lightning-Talk-Series)
  * [AQA Tests WiKi (aqa-tests)](https://github.com/adoptium/aqa-tests/wiki)
</details>

* <details><summary><b>14. Miscellaneous</b></summary>

  * [Compiler Best Practices (OpenJ9)](misc/BestPractices.md)
</details>
