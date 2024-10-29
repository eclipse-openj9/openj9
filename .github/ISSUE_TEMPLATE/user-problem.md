---
name: Report a user problem
about: Must gather information to report a problem
labels: "userRaised"
---

Java -version output
--------------------

Output from `java -version`.
If the shas are not public, include equivalent public shas.

Summary of problem
------------------

Put the problem summary here.

Diagnostic files
----------------

If you experience a crash (GPF, assert, abort, etc.) or OutOfMemory condition, please provide the diagnostic files produced (core, javacore, jitdump, Snap). The smaller files can be attached to this Issue.

The system core, which is different from the javacore and an important source of diagnostic information, should be first processed with `jpackcore` (except on Windows), see https://eclipse.dev/openj9/docs/tool_jextract/#dump-extractor-jpackcore. The system core, which is likely quite large, should be compressed (`jpackcore` does this), and made available via a file sharing service (Box, Google Drive, OneDrive, etc.). If there are privacy concerns please directly email or Slack the files or link/password to an OpenJ9 committer.

The following articles are helpful for finding core files:
- https://community.ibm.com/community/user/wasdevops/blogs/kevin-grigorenko1/2022/11/14/lessons-from-the-field-23-linux-core-dumps
- https://community.ibm.com/community/user/wasdevops/blogs/kevin-grigorenko1/2023/09/19/lessons-from-the-field-33-capturing-core-dumps-on

FYI the stderr console will contain messages which describe the location of the diagnostic dump files, similar to the following.
```
JVMDUMP039I Processing dump event "abort", detail "" at 2019/03/04 01:58:02 - please wait.
JVMDUMP032I JVM requested System dump using '/home/jenkins/cmdLineTest_gpTest_0/core.20190304.015802.21708.0001.dmp' in response to an event
JVMDUMP010I System dump written to /home/jenkins/cmdLineTest_gpTest_0/core.20190304.015802.21708.0001.dmp
JVMDUMP032I JVM requested Java dump using '/home/jenkins/cmdLineTest_gpTest_0/javacore.20190304.015802.21708.0002.txt' in response to an event
JVMDUMP010I Java dump written to /home/jenkins/cmdLineTest_gpTest_0/javacore.20190304.015802.21708.0002.txt
JVMDUMP032I JVM requested Snap dump using '/home/jenkins/cmdLineTest_gpTest_0/Snap.20190304.015802.21708.0003.trc' in response to an event
JVMDUMP010I Snap dump written to /home/jenkins/cmdLineTest_gpTest_0/Snap.20190304.015802.21708.0003.trc
JVMDUMP007I JVM Requesting JIT dump using '/home/jenkins/cmdLineTest_gpTest_0/jitdump.20190304.015802.21708.0004.dmp'
JVMDUMP010I JIT dump written to /home/jenkins/cmdLineTest_gpTest_0/jitdump.20190304.015802.21708.0004.dmp
JVMDUMP013I Processed dump event "abort", detail "".
```

OutOfMemoryError: Java Heap Space
---------------------------------

For a repeatable OutOfMemory condition showing `Java heap space`, enable verbose GC collection and provide the log.
-`-verbose:gc` writes the logging to stderr
- [-Xverbosegclog](https://www.eclipse.org/openj9/docs/xverbosegclog/) writes the logging to a single or multiple files
