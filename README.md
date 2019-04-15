<!--
Copyright (c) 2017, 2019 IBM Corp. and others

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
<img src="https://github.com/eclipse/openj9/blob/master/artwork/OpenJ9.svg" alt="OpenJ9 logo" align="middle" width="50%" height="50%" />
<p>

Welcome to the Eclipse OpenJ9 repository
========================================
[![License](https://img.shields.io/badge/License-EPL%202.0-green.svg)](https://opensource.org/licenses/EPL-2.0)
[![License](https://img.shields.io/badge/License-APL%202.0-green.svg)](https://opensource.org/licenses/Apache-2.0)


We're not sure which route you might have taken on your way here, but we're really pleased to see you! If you came directly from our website, you've probably already learned a lot about Eclipse OpenJ9 and how it fits in to the OpenJDK ecosystem. If you came via some other route, here are a few key links to get you started:

- [Eclipse OpenJ9 website](http://www.eclipse.org/openj9) - Learn about this high performance, enterprise-grade Java Virtual Machine (JVM) and why we think you want to get involved in its development.
- [AdoptOpenJDK website](https://adoptopenjdk.net/releases.html?variant=openjdk9-openj9) - Grab pre-built OpenJDK binaries that embed OpenJ9 and try it out.
- [Build instructions](https://www.eclipse.org/openj9/oj9_build.html) - Here's how you can build an OpenJDK with OpenJ9 yourself.
- [FAQ](https://www.eclipse.org/openj9/oj9_faq.html)

If you're looking for ways to help out at the project (thanks!), we have:
- [Beginner issues](https://github.com/eclipse/openj9/issues?q=is%3Aopen+is%3Aissue+label%3Abeginner)
- [Help Wanted issues](https://github.com/eclipse/openj9/issues?utf8=%E2%9C%93&q=is%3Aopen+is%3Aissue+label%3A%22help+wanted%22)

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
[LICENSE file](https://github.com/eclipse/openj9/blob/master/LICENSE) for more details.

Eclipse OpenJ9 is a source code project that can be built alongside Java class libraries.  Cross platform
nightly and release binaries and docker containers for OpenJDK with OpenJ9, targeting several JDK levels
(like JDK8, JDK10, etc.) are built by the [AdoptOpenJDK organization](https://github.com/adoptopenjdk)
and can be downloaded from the [AdoptOpenJDK download site](https://adoptopenjdk.net/?variant=openjdk8-openj9)
or on [DockerHub](https://hub.docker.com/search/?isAutomated=0&isOfficial=0&page=1&pullCount=0&q=openj9&starCount=0).

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

If you think you want to contribute but you're not ready to sign the Eclipse Contributor Agreement, why not come along to our weekly *Ask the OpenJ9 community* calls to find out more about how we work. We talk about new ideas, answer any questions that get raised, and discuss project plans and status. We also do lightning talks on features and functions of the VM. Visit the *#planning* channel in our [Slack workspace](https://openj9.slack.com/) for information about upcoming community calls and minutes from previous meetings (Join [here](https://join.slack.com/t/openj9/shared_invite/enQtNDU4MDI4Mjk0MTk2LWM2MjliMTQ4NWM5YjMwNTViOTgzMzM2ZDhlOWJmZTc1MjhmYmRjMDg2NDljNGM0YTAwOWRiZDE0YzI0NjgyOWI)). 

What repos are part of the project?
===================================
- https://github.com/eclipse/openj9 : OpenJ9 main code base
- https://github.com/eclipse/openj9-omr : Eclipse OMR clone to stage temporary OMR changes.  (None so far!)
- https://github.com/eclipse/openj9-systemtest : OpenJ9-specific system tests
- https://github.com/eclipse/openj9-website : OpenJ9 website repo
- https://github.com/eclipse/openj9-docs : OpenJ9 documentation repo


Where can I learn more?
=======================

Videos and Presentations
------------------------

- [JavaOne 2017: John Duimovich and Mike Milinkovich having fun chatting about Eclipse OpenJ9 (and EE4J)](https://www.youtube.com/watch?v=4g9SdVCPlnk)
- [JavaOne 2017: Holly Cummins interviewing Dan Heidinga and Mark Stoodley on Eclipse OpenJ9 and OMR](https://www.youtube.com/watch?v=c1LVXqD3cII)
- [JavaOne 2017: Open sourcing the IBM J9 Java Virtual Machine](https://www.slideshare.net/MarkStoodley/javaone-2017-mark-stoodley-open-sourcing-ibm-j9-jvm)
- [JavaOne 2017: Eclipse OpenJ9 Under the hood of the next open source JVM](https://www.slideshare.net/DanHeidinga/javaone-2017-eclipse-openj9-under-the-hood-of-the-jvm)
- [JavaOne 2017: Ask the OpenJ9 Architects](https://www.youtube.com/watch?v=qb5ennM_pgc)

Also check out the [Resources](https://www.eclipse.org/openj9/oj9_resources.html) page on our website.

Copyright (c) 2017, 2018 IBM Corp. and others
