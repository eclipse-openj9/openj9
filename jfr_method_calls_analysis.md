# JFR Method Calls Analysis

**Source:** `~/jfr/txt` on unlocked1.fyre.ibm.com
**Date:** 2026-04-16
**Total Method Calls:** 6,928
**Unique Methods:** 42

## Summary Statistics

The JFR trace shows method calls exclusively from the `jdk/jfr/internal/JVM` class. The most frequently called methods are:

1. **`getJVM()Ljdk/jfr/internal/JVM;`** - 3,428 calls (49.5%)
2. **`getTypeId(Ljava/lang/Class;)J`** - 3,044 calls (43.9%)
3. **`setEnabled(JZ)V`** - 290 calls (4.2%)

## Top 10 Most Called Methods

| Rank | Method | Call Count | Percentage |
|------|--------|------------|------------|
| 1 | `jdk/jfr/internal/JVM.getJVM()Ljdk/jfr/internal/JVM;` | 3,428 | 49.5% |
| 2 | `jdk/jfr/internal/JVM.getTypeId(Ljava/lang/Class;)J` | 3,044 | 43.9% |
| 3 | `jdk/jfr/internal/JVM.setEnabled(JZ)V` | 290 | 4.2% |
| 4 | `jdk/jfr/internal/JVM.getHandler(Ljava/lang/Class;)Ljava/lang/Object;` | 34 | 0.5% |
| 5 | `jdk/jfr/internal/JVM.isRecording()Z` | 26 | 0.4% |
| 6 | `jdk/jfr/internal/JVM.setHandler(Ljava/lang/Class;Ljdk/jfr/internal/handlers/EventHandler;)Z` | 24 | 0.3% |
| 7 | `jdk/jfr/internal/JVM.subscribeLogLevel(Ljdk/jfr/internal/LogTag;I)V` | 14 | 0.2% |
| 8 | `jdk/jfr/internal/JVM.getTypeId(Ljava/lang/String;)J` | 12 | 0.2% |
| 9 | `jdk/jfr/internal/JVM.log(IILjava/lang/String;)V` | 6 | 0.1% |
| 10 | `jdk/jfr/internal/JVM.setMethodSamplingPeriod(JJ)V` | 4 | 0.1% |

## All Unique Methods (Alphabetically)

1. `jdk/jfr/internal/JVM.<clinit>()V`
2. `jdk/jfr/internal/JVM.<init>()V`
3. `jdk/jfr/internal/JVM.beginRecording()V`
4. `jdk/jfr/internal/JVM.createJFR(Z)Z`
5. `jdk/jfr/internal/JVM.createNativeJFR()V`
6. `jdk/jfr/internal/JVM.destroyJFR()Z`
7. `jdk/jfr/internal/JVM.destroyNativeJFR()Z`
8. `jdk/jfr/internal/JVM.emitOldObjectSamples(JZZ)V`
9. `jdk/jfr/internal/JVM.endRecording()V`
10. `jdk/jfr/internal/JVM.getAllEventClasses()Ljava/util/List;`
11. `jdk/jfr/internal/JVM.getAllowedToDoEventRetransforms()Z`
12. `jdk/jfr/internal/JVM.getChunkStartNanos()J`
13. `jdk/jfr/internal/JVM.getHandler(Ljava/lang/Class;)Ljava/lang/Object;`
14. `jdk/jfr/internal/JVM.getJVM()Ljdk/jfr/internal/JVM;`
15. `jdk/jfr/internal/JVM.getPid()Ljava/lang/String;`
16. `jdk/jfr/internal/JVM.getTimeConversionFactor()D`
17. `jdk/jfr/internal/JVM.getTypeId(Ljava/lang/Class;)J`
18. `jdk/jfr/internal/JVM.getTypeId(Ljava/lang/String;)J`
19. `jdk/jfr/internal/JVM.getUnloadedEventClassCount()J`
20. `jdk/jfr/internal/JVM.hasNativeJFR()Z`
21. `jdk/jfr/internal/JVM.isAvailable()Z`
22. `jdk/jfr/internal/JVM.isRecording()Z`
23. `jdk/jfr/internal/JVM.log(IILjava/lang/String;)V`
24. `jdk/jfr/internal/JVM.markChunkFinal()V`
25. `jdk/jfr/internal/JVM.registerNatives()V`
26. `jdk/jfr/internal/JVM.retransformClasses([Ljava/lang/Class;)V`
27. `jdk/jfr/internal/JVM.setEnabled(JZ)V`
28. `jdk/jfr/internal/JVM.setFileNotification(J)V`
29. `jdk/jfr/internal/JVM.setForceInstrumentation(Z)V`
30. `jdk/jfr/internal/JVM.setGlobalBufferCount(J)V`
31. `jdk/jfr/internal/JVM.setGlobalBufferSize(J)V`
32. `jdk/jfr/internal/JVM.setHandler(Ljava/lang/Class;Ljdk/jfr/internal/handlers/EventHandler;)Z`
33. `jdk/jfr/internal/JVM.setMemorySize(J)V`
34. `jdk/jfr/internal/JVM.setMethodSamplingPeriod(JJ)V`
35. `jdk/jfr/internal/JVM.setOutput(Ljava/lang/String;)V`
36. `jdk/jfr/internal/JVM.setRepositoryLocation(Ljava/lang/String;)V`
37. `jdk/jfr/internal/JVM.setSampleThreads(Z)V`
38. `jdk/jfr/internal/JVM.setStackDepth(I)V`
39. `jdk/jfr/internal/JVM.setThreadBufferSize(J)V`
40. `jdk/jfr/internal/JVM.shouldRotateDisk()Z`
41. `jdk/jfr/internal/JVM.storeMetadataDescriptor([B)V`
42. `jdk/jfr/internal/JVM.subscribeLogLevel(Ljdk/jfr/internal/LogTag;I)V`

## Complete Ordered List of Method Calls

Below is the list of all 42 unique methods in the order they were first called.
The number indicates the line number in the original trace where each method was first invoked.
1. jdk/jfr/internal/JVM.<clinit>()V
2. jdk/jfr/internal/JVM.<init>()V
3. jdk/jfr/internal/JVM.registerNatives()V
4. jdk/jfr/internal/JVM.subscribeLogLevel(Ljdk/jfr/internal/LogTag;I)V
18. jdk/jfr/internal/JVM.getJVM()Ljdk/jfr/internal/JVM;
19. jdk/jfr/internal/JVM.setFileNotification(J)V
20. jdk/jfr/internal/JVM.setMemorySize(J)V
21. jdk/jfr/internal/JVM.setGlobalBufferSize(J)V
22. jdk/jfr/internal/JVM.setGlobalBufferCount(J)V
23. jdk/jfr/internal/JVM.setSampleThreads(Z)V
24. jdk/jfr/internal/JVM.setStackDepth(I)V
25. jdk/jfr/internal/JVM.setThreadBufferSize(J)V
27. jdk/jfr/internal/JVM.isAvailable()Z
29. jdk/jfr/internal/JVM.setForceInstrumentation(Z)V
33. jdk/jfr/internal/JVM.getPid()Ljava/lang/String;
34. jdk/jfr/internal/JVM.createNativeJFR()V
35. jdk/jfr/internal/JVM.createJFR(Z)Z
37. jdk/jfr/internal/JVM.getTypeId(Ljava/lang/String;)J
64. jdk/jfr/internal/JVM.getTypeId(Ljava/lang/Class;)J
4294. jdk/jfr/internal/JVM.log(IILjava/lang/String;)V
4302. jdk/jfr/internal/JVM.getHandler(Ljava/lang/Class;)Ljava/lang/Object;
4372. jdk/jfr/internal/JVM.setHandler(Ljava/lang/Class;Ljdk/jfr/internal/handlers/EventHandler;)Z
4373. jdk/jfr/internal/JVM.isRecording()Z
6295. jdk/jfr/internal/JVM.retransformClasses([Ljava/lang/Class;)V
6300. jdk/jfr/internal/JVM.hasNativeJFR()Z
6301. jdk/jfr/internal/JVM.shouldRotateDisk()Z
6305. jdk/jfr/internal/JVM.setRepositoryLocation(Ljava/lang/String;)V
6306. jdk/jfr/internal/JVM.storeMetadataDescriptor([B)V
6307. jdk/jfr/internal/JVM.setOutput(Ljava/lang/String;)V
6309. jdk/jfr/internal/JVM.getChunkStartNanos()J
6310. jdk/jfr/internal/JVM.getUnloadedEventClassCount()J
6311. jdk/jfr/internal/JVM.getAllEventClasses()Ljava/util/List;
6313. jdk/jfr/internal/JVM.beginRecording()V
6320. jdk/jfr/internal/JVM.setEnabled(JZ)V
6534. jdk/jfr/internal/JVM.setMethodSamplingPeriod(JJ)V
6614. jdk/jfr/internal/JVM.getAllowedToDoEventRetransforms()Z
6619. jdk/jfr/internal/JVM.getTimeConversionFactor()D
6621. jdk/jfr/internal/JVM.emitOldObjectSamples(JZZ)V
6624. jdk/jfr/internal/JVM.markChunkFinal()V
6629. jdk/jfr/internal/JVM.endRecording()V
6927. jdk/jfr/internal/JVM.destroyNativeJFR()Z
6928. jdk/jfr/internal/JVM.destroyJFR()Z
