# Notices for Eclipse OpenJ9

* Project home: https://github.com/eclipse-openj9/openj9


## Trademarks

Java and all Java-based trademarks are trademarks of Oracle Corporation in the United States, other countries, or both.


## Eclipse Foundation Software User Agreement
August 31, 2017


### Usage Of Content

THE ECLIPSE FOUNDATION MAKES AVAILABLE SOFTWARE, DOCUMENTATION, INFORMATION AND/OR OTHER MATERIALS FOR OPEN SOURCE PROJECTS
(COLLECTIVELY "CONTENT").  USE OF THE CONTENT IS GOVERNED BY THE TERMS AND CONDITIONS OF THIS AGREEMENT AND/OR THE TERMS AND
CONDITIONS OF LICENSE AGREEMENTS OR NOTICES INDICATED OR REFERENCED BELOW.  BY USING THE CONTENT, YOU AGREE THAT YOUR USE
OF THE CONTENT IS GOVERNED BY THIS AGREEMENT AND/OR THE TERMS AND CONDITIONS OF ANY APPLICABLE LICENSE AGREEMENTS OR
NOTICES INDICATED OR REFERENCED BELOW.  IF YOU DO NOT AGREE TO THE TERMS AND CONDITIONS OF THIS AGREEMENT AND THE TERMS AND
CONDITIONS OF ANY APPLICABLE LICENSE AGREEMENTS OR NOTICES INDICATED OR REFERENCED BELOW, THEN YOU MAY NOT USE THE CONTENT.


### Applicable Licenses

Unless otherwise indicated, all Content made available by the Eclipse Foundation is provided to you under the terms and conditions of the Eclipse Public License Version 2.0
("EPL").  A copy of the EPL is provided with this Content and is also available at [https://www.eclipse.org/legal/epl-2.0](https://www.eclipse.org/legal/epl-2.0).
For purposes of the EPL, "Program" will mean the Content.

Content includes, but is not limited to, source code, object code, documentation and other files maintained in the Eclipse Foundation source code
repository ("Repository") in software modules ("Modules") and made available as downloadable archives ("Downloads").

* Content may be structured and packaged into modules to facilitate delivering, extending, and upgrading the Content.  Typical modules may include plug-ins ("Plug-ins"), plug-in fragments ("Fragments"), and features ("Features").
* Each Plug-in or Fragment may be packaged as a sub-directory or JAR (Java&trade; ARchive) in a directory named "plugins".
* A Feature is a bundle of one or more Plug-ins and/or Fragments and associated material.  Each Feature may be packaged as a sub-directory in a directory named "features".  Within a Feature, files named "feature.xml" may contain a list of the names and version numbers of the Plug-ins
      and/or Fragments associated with that Feature.
* Features may also include other Features ("Included Features"). Within a Feature, files named "feature.xml" may contain a list of the names and version numbers of Included Features.

The terms and conditions governing Plug-ins and Fragments should be contained in files named "about.html" ("Abouts"). The terms and conditions governing Features and
Included Features should be contained in files named "license.html" ("Feature Licenses").  Abouts and Feature Licenses may be located in any directory of a Download or Module
including, but not limited to the following locations:

* The top-level (root) directory
* Plug-in and Fragment directories
* Inside Plug-ins and Fragments packaged as JARs
* Sub-directories of the directory named "src" of certain Plug-ins
* Feature directories

Note: if a Feature made available by the Eclipse Foundation is installed using the Provisioning Technology (as defined below), you must agree to a license ("Feature Update License") during the
installation process.  If the Feature contains Included Features, the Feature Update License should either provide you with the terms and conditions governing the Included Features or
inform you where you can locate them.  Feature Update Licenses may be found in the "license" property of files named "feature.properties" found within a Feature.
Such Abouts, Feature Licenses, and Feature Update Licenses contain the terms and conditions (or references to such terms and conditions) that govern your use of the associated Content in
that directory.

THE ABOUTS, FEATURE LICENSES, AND FEATURE UPDATE LICENSES MAY REFER TO THE EPL OR OTHER LICENSE AGREEMENTS, NOTICES OR TERMS AND CONDITIONS.  SOME OF THESE
OTHER LICENSE AGREEMENTS MAY INCLUDE (BUT ARE NOT LIMITED TO):

* Eclipse Distribution License Version 1.0 (available at [https://www.eclipse.org/licenses/edl-v1.0.html](https://www.eclipse.org/licenses/edl-v10.html))
* Eclipse Distribution License Version 2.0 (available at [https://www.eclipse.org/legal/epl-2.0](https://www.eclipse.org/legal/epl-2.0))
* Common Public License Version 1.0 (available at [https://www.eclipse.org/legal/cpl-v10.html](https://www.eclipse.org/legal/cpl-v10.html))
* Apache Software License 1.1 (available at [https://www.apache.org/licenses/LICENSE](https://www.apache.org/licenses/LICENSE))
* Apache Software License 2.0 (available at [https://www.apache.org/licenses/LICENSE-2.0](https://www.apache.org/licenses/LICENSE-2.0))
* Mozilla Public License Version 1.1 (available at [https://www.mozilla.org/MPL/MPL-1.1.html](https://www.mozilla.org/MPL/MPL-1.1.html))

IT IS YOUR OBLIGATION TO READ AND ACCEPT ALL SUCH TERMS AND CONDITIONS PRIOR TO USE OF THE CONTENT.  If no About, Feature License, or Feature Update License is provided, please
contact the Eclipse Foundation to determine what terms and conditions govern that particular Content.


### Use of Provisioning Technology

The Eclipse Foundation makes available provisioning software, examples of which include, but are not limited to, p2 and the Eclipse
Update Manager ("Provisioning Technology") for the purpose of allowing users to install software, documentation, information and/or
other materials (collectively "Installable Software"). This capability is provided with the intent of allowing such users to
install, extend and update Eclipse-based products. Information about packaging Installable Software is available at
[https://www.eclipse.org/equinox/p2/repository_packaging.html](https://www.eclipse.org/equinox/p2/repository_packaging.html)
("Specification").

You may use Provisioning Technology to allow other parties to install Installable Software. You shall be responsible for enabling the
applicable license agreements relating to the Installable Software to be presented to, and accepted by, the users of the Provisioning Technology
in accordance with the Specification. By using Provisioning Technology in such a manner and making it available in accordance with the
Specification, you further acknowledge your agreement to, and the acquisition of all necessary rights to permit the following:

1. A series of actions may occur ("Provisioning Process") in which a user may execute the Provisioning Technology
on a machine ("Target Machine") with the intent of installing, extending or updating the functionality of an Eclipse-based
product.
2. During the Provisioning Process, the Provisioning Technology may cause third party Installable Software or a portion thereof to be
accessed and copied to the Target Machine.
3. Pursuant to the Specification, you will provide to the user the terms and conditions that govern the use of the Installable
Software ("Installable Software Agreement") and such Installable Software Agreement shall be accessed from the Target
Machine in accordance with the Specification. Such Installable Software Agreement must inform the user of the terms and conditions that govern
the Installable Software and must solicit acceptance by the end user in the manner prescribed in such Installable Software Agreement. Upon such
indication of agreement by the user, the provisioning Technology will complete installation of the Installable Software.


### Cryptography

Content may contain encryption software. The country in which you are currently may have restrictions on the import, possession, and use, and/or re-export to
another country, of encryption software. BEFORE using any encryption software, please check the country's laws, regulations and policies concerning the import,
possession, or use, and re-export of encryption software, to see if this is permitted.


## Third-party Content

This project may leverage the following third party content when building and at runtime.

Checkpoint/Restore In Userspace (CRIU) (3.18)

* License: GPL v2, LGPL v2.1 [https://github.com/checkpoint-restore/criu/blob/criu-dev/COPYING](https://github.com/checkpoint-restore/criu/blob/criu-dev/COPYING)
* Project: https://criu.org/Main_Page
* Source: https://github.com/ibmruntimes/criu

OpenSSL (1.1.1)

* License: https://github.com/openssl/openssl/blob/OpenSSL_1_1_1-stable/LICENSE
* Project: https://www.openssl.org/
* Source: https://github.com/openssl/openssl or https://github.com/ibmruntimes/openssl

OpenSSL (3.x)

* License: https://www.openssl.org/source/apache-license-2.0.txt
* Project: https://www.openssl.org/
* Source: https://github.com/openssl/openssl or https://github.com/ibmruntimes/openssl

protobuf-c (prereq of CRIU)

* License: BSD-3-Clause
* Project: https://developers.google.com/protocol-buffers/
* Source: https://github.com/google/protobuf

This project leverages the following third party content for testing.

Apache Ant

* License: Apache-2.0, W3C License, Public Domain

Apache Commons Exec (1.1)

* License: Apache-2.0

Apache commons-cli (1.2)

* License: Apache-2.0

Apache Log4J (2.16)

* License: Apache-2.0

apache-ant-contrib (1.0)

* License: Apache-1.1 AND Apache-2.0
* Project: http://ant-contrib.sourceforge.net/

ASM (9.0)

* License: New BSD License

asmtools (7.0)

* License: GPL-2.0-only WITH Classpath-exception-2.0
* Project: https://openjdk.org/projects/code-tools/
* Source: https://github.com/openjdk/asmtools/

java assist (3.20)

* License: MPL-1.1 OR LGPL-2.1+
* Project: https://github.com/jboss-javassist/javassist/
* Source: https://github.com/jboss-javassist/javassist/archive/rel_3_12_1_ga.zip

Java Hamcrest

* License: BSD-3-Clause
* Project: https://hamcrest.org/JavaHamcrest/
* Source: https://github.com/hamcrest/JavaHamcrest

JAXB API (2.3)

* License: CDDL-1.1

jcommander (1.48)

* License: Apache-2.0

junit (4.10)

* License: Common Public License 1.0

JSON.simple (1.1.1)

* License: Apache-2.0

Jython (2.7.2)

* License: CNRI-Jython

Mauve

* Project: https://www.sourceware.org/mauve/

OSGi (3.16.100)

* License: EPL-1.0

testng (6.14)

* License: Apache-2.0 AND MIT
