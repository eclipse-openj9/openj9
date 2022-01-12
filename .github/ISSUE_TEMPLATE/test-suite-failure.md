---
name: OpenJ9 test suite failure
about: A failing functionality, system, or JTReg test
labels: "test failure"
---

Failure link
------------

Public link to the failing test
and ideally extracting relevant info from the failed run (just enough info to [run a Grinder](https://github.com/adoptium/aqa-tests/wiki/How-to-Run-a-Grinder-Build-on-Jenkins)), include:
- test category, in Grinder equates to BUILD_LIST (functional, systemtest, openjdk, etc): 
- test target name, in Grinder equates to TARGET (testCaseName from playlist.xml, jdk_lang, jdk_net, etc):
- OS/architecture, in Grinder equates to Jenkinsfile (openjdk_x86-64_linux, openjdk_x86-64_windows, etc): 
- public build SHAs (i.e. java -version output), in Grinder provides JDK_VERSION/JDK_IMPL info and helps identify point at which regression introduced: 

Optional info
-------------

- intermittent failure (yes|no): 
- regression or new test:  
- if regression, what are the last passing / first failing public SHAs (OpenJ9, OMR, JCL) :

Failure output (captured from console output)
---------------------------------------------
Include test case name, stack trace output.  If test fails with a crash, please include a link to the artifacts (xxxx_test_output.tar.gz) as they include the core/dmp files.  Artifacts are either stored as an attachment to the CI server job itself or as a link from the job to an Artifactory location.
