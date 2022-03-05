<!--
Copyright (c) 2017, 2021 IBM Corp. and others

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

<p align="center">
<img src="https://github.com/eclipse-openj9/openj9/blob/master/artwork/OpenJ9.svg" alt="OpenJ9 logo" align="middle" width="50%" height="50%" />
<p>

Welcome to the Eclipse OpenJ9 repository
========================================
[![License](https://img.shields.io/badge/License-EPL%202.0-green.svg)](https://opensource.org/licenses/EPL-2.0)
[![License](https://img.shields.io/badge/License-APL%202.0-green.svg)](https://opensource.org/licenses/Apache-2.0)


We're not sure which route you might have taken on your way here, but we're really pleased to see you! If you came directly from our website, you've probably already learned a lot about Eclipse OpenJ9 and how it fits in to the OpenJDK ecosystem. If you came via some other route, here are a few key links to get you started:

- [Eclipse OpenJ9 website](http://www.eclipse.org/openj9) - Learn about this high performance, enterprise-grade Java Virtual Machine (JVM) and why we think you want to get involved in its development.
- Build instructions for [JDK8](https://github.com/eclipse-openj9/openj9/blob/master/doc/build-instructions/Build_Instructions_V8.md), [JDK11](https://github.com/eclipse-openj9/openj9/blob/master/doc/build-instructions/Build_Instructions_V11.md), and [More](https://github.com/eclipse-openj9/openj9/blob/master/doc/build-instructions) - Here's how you can build an OpenJDK with OpenJ9 yourself.

If you're looking for ways to help out at the project (thanks!), we have:
- [Beginner issues](https://github.com/eclipse-openj9/openj9/issues?q=is%3Aopen+is%3Aissue+label%3Abeginner)
- [Help Wanted issues](https://github.com/eclipse-openj9/openj9/issues?utf8=%E2%9C%93&q=is%3Aopen+is%3Aissue+label%3A%22help+wanted%22)

If you're here to learn more about the project, read on ...

What is Eclipse OpenJ9?
=======================

Eclipse OpenJ9 is an independent implementation of a Java Virtual Machine. "Independent implementation"
means it was built using the Java Virtual Machine specification without using any code from any other Java
Virtual Machine. 

The OpenJ9 JVM combines with the Java Class libraries from OpenJDK to create a complete JDK tuned for
footprint, performance, and reliability that is well suited for cloud deployments.

The original source contribution to OpenJ9 came from the IBM "J9" JVM which has been used in production
by thousands of Java applications for the last two decades.  In September 2017, IBM completed open sourcing
the J9 JVM as "Eclipse OpenJ9" at the Eclipse Foundation. Significant parts of J9 are also open source
at the [Eclipse OMR project](https://github.com/eclipse/omr). OpenJ9 has a permissive license (Apache
License 2.0 or Eclipse Public License 2.0 with a secondary compatibility license for the OpenJDK project's
GPLv2 license) that is designed to allow OpenJDK to be built with the OpenJ9 JVM.  Please see our
[LICENSE file](https://github.com/eclipse-openj9/openj9/blob/master/LICENSE) for more details.

Eclipse OpenJ9 is a source code project that can be built alongside Java class libraries. See the
[build instructions](https://github.com/eclipse-openj9/openj9/blob/master/doc/build-instructions). Eclipse
Foundation projects are not permitted to distribute, market or promote JDK binaries unless they have
passed a Java SE Technology Compatibility Kit licensed from Oracle, to which the OpenJ9 project does
not currently have access. See the [Eclipse Adoptium Project Charter](https://projects.eclipse.org/projects/adoptium/charter).

What is the goal of the project?
================================

The long term goal of the Eclipse OpenJ9 project is to foster an open ecosystem of JVM developers that can collaborate and innovate with designers and developers of hardware platforms, operating systems, tools, and frameworks.

The project welcomes collaboration, embraces fresh innovation, and extends an opportunity to influence the development of OpenJ9 for the next generation of Java applications.

The Java community has benefited over its history from having multiple implementations of the JVM specification competing to provide the best runtime for your application.  Whether adding compressed references, new Cloud features, AOT (ahead of time compilation), or straight up faster performance and lower memory use, the ecosystem has improved through that competition.  Eclipse OpenJ9 aims to continue to spur innovation in the runtimes space.

How do I contribute?
====================

Since we are an Eclipse Foundation project, each contributor needs to sign an Eclipse Contributor Agreement. The Eclipse Foundation operates under the [Eclipse Code of Conduct][coc]
to promote fairness, openness, and inclusion.

To get started, read our [Contribution Guide](CONTRIBUTING.md).

[coc]: https://eclipse.org/org/documents/Community_Code_of_Conduct.php

If you think you want to contribute but you're not ready to sign the Eclipse Contributor Agreement, why not come along to our weekly *Ask the OpenJ9 community* calls to find out more about how we work. We talk about new ideas, answer any questions that get raised, and discuss project plans and status. We also do lightning talks on features and functions of the VM. Visit the *#planning* channel in our [Slack workspace](https://openj9.slack.com/) for information about upcoming community calls and minutes from previous meetings (Join [here](https://join.slack.com/t/openj9/shared_invite/enQtNDU4MDI4Mjk0MTk2LWVhNTMzMGY1N2JkODQ1OWE0NTNmZjM4ZDcxOTBiMjk3NGFjM2U0ZDNhMmY0MDZlNzU0ZjAyNzQ1ODlmYjg3MjA)). 

What repos are part of the project?
===================================
- https://github.com/eclipse-openj9/openj9 : OpenJ9 main code base
- https://github.com/eclipse-openj9/openj9-omr : Eclipse OMR clone to stage temporary OMR changes.  (None so far!)
- https://github.com/eclipse-openj9/openj9-systemtest : OpenJ9-specific system tests
- https://github.com/eclipse-openj9/openj9-website : OpenJ9 website repo
- https://github.com/eclipse-openj9/openj9-docs : OpenJ9 documentation repo
- https://github.com/eclipse-openj9/build-openj9 : OpenJ9 GitHub actions repo
- https://github.com/eclipse-openj9/openj9-utils : OpenJ9 utility programs / tools repo, a place to develop community around the tools


Where can I learn more?
=======================

- [Moving to OpenJ9](https://blog.openj9.org/2019/02/26/moving-to-openjdk-with-eclipse-openj9/)
- [User documentation](https://www.eclipse.org/openj9/docs/)
- Many more resources are available on the [OpenJ9 blog](https://blog.openj9.org/)

Videos and Presentations
------------------------

- [JavaOne 2017: John Duimovich and Mike Milinkovich having fun chatting about Eclipse OpenJ9 (and EE4J)](https://www.youtube.com/watch?v=4g9SdVCPlnk)
- [JavaOne 2017: Holly Cummins interviewing Dan Heidinga and Mark Stoodley on Eclipse OpenJ9 and OMR](https://www.youtube.com/watch?v=c1LVXqD3cII)
- [JavaOne 2017: Open sourcing the IBM J9 Java Virtual Machine](https://www.slideshare.net/MarkStoodley/javaone-2017-mark-stoodley-open-sourcing-ibm-j9-jvm)
- [JavaOne 2017: Eclipse OpenJ9 Under the hood of the next open source JVM](https://www.slideshare.net/DanHeidinga/javaone-2017-eclipse-openj9-under-the-hood-of-the-jvm)
- [JavaOne 2017: Ask the OpenJ9 Architects](https://www.youtube.com/watch?v=qb5ennM_pgc)
- [Eclipse OpenJ9 verbose JIT logs](https://mstoodle.github.io/EclipseOpenJ9JitVerboseLogs/)
- [JavaOne 2017: How to build a debuggable runtime](https://www.slideshare.net/TobiAjila1/how-to-build-a-debuggle-runtime)
- [Reducing Garbage Collection pause times with Concurrent Scavenge and the Guarded Storage Facility](https://developer.ibm.com/javasdk/2017/09/18/reducing-garbage-collection-pause-times-concurrent-scavenge-guarded-storage-facility/)
- [How Concurrent Scavenge using the Guarded Storage Facility Works](https://developer.ibm.com/javasdk/2017/09/25/concurrent-scavenge-using-guarded-storage-facility-works/)
- [Are you still paying for unused memory when your Java app is idle?](https://developer.ibm.com/javasdk/2017/09/25/still-paying-unused-memory-java-app-idle/)
- [Under the hood of the Testarossa JIT Compiler](https://www.slideshare.net/MarkStoodley/under-the-hood-of-the-testarossa-jit-compiler)
- [Class sharing In Eclipse OpenJ9](https://developer.ibm.com/components/java-platform/tutorials/j-class-sharing-openj9)

Investigating #219 
Should NOT be reviewed
Created by: Jerome Ju jeromeqju@gmail.com

Copyright (c) 2017, 2020 IBM Corp. and others
