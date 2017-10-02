<!--
Copyright (c) 2017, 2017 IBM Corp. and others

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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->

[Eclipse OpenJ9 Jenkins Builds](https://ci.eclipse.org/openj9/)

This folder contains Jenkins pipeline scripts that are used in the OpenJ9 Jenkins builds.

### Triggering Pull Request Builds from Github

- Current supported PR builds are "Compile", "Compile & Sanity", or "Compile & Extended"
- Current available platforms are Linux s390x and Linux PPCLE
- OpenJ9 committers can request builds by commenting in a pull request
- Request a build in a PR by commenting
    - `Jenkins compile`
    - `Jenkins test sanity`
    - `Jenkins test extended`
- Request a single platform instead of "all"
    - `Jenkins test sanity zlinux`
    - `Jenkins test sanity plinux`
- You can also request a "Compile & Sanity" build from an [openj9-openjdk-jdk9](https://github.com/ibmruntimes/openj9-openjdk-jdk9) PR or an [openj9-omr](https://github.com/eclipse/openj9-omr) PR.

##### Dependent Changes

- If you have dependent change(s) in either eclipse/omr, eclipse/openj9-omr, or ibmruntimes/openj9-openjdk-jdk9, you can build & test with all needed changes
- Request a build by including the PR ref in your trigger comment
- Ex. Dependent change in OMR Pull Request `#123`
    - `Jenkins test sanity depends eclipse/omr#123`
- Ex. Dependent change in OpenJ9-OMR Pull Request `#456`
    - `Jenkins test sanity depends eclipse/openj9-omr#456`
- Ex. Dependent change in OpenJDK Pull Request `#789`
    - `Jenkins test sanity depends ibmruntimes/openj9-openjdk-jdk9#789`
- Ex. Dependent changes in OMR and OpenJDK
    - `Jenkins test sanity depends eclipse/omr#123 ibmruntimes/openj9-openjdk-jdk9#789`
- Ex. If you have a dependent change and only want one platform, depends comes last
    - `Jenkins test sanity zlinux depends eclipse/omr#123`

**Note:** Dependent changes can only be requested from an OpenJ9 Pull Request

##### Other Pull Requests builds

- To trigger a Line Endings Check
   - `Jenkins line endings check`

### Overview of Builds

#### Build

- Build-linux_390-64_cmprssptrs
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Build-linux_390-64_cmprssptrs)](https://ci.eclipse.org/openj9/job/Build-linux_390-64_cmprssptrs)
    - Description:
        - Compiles on linux_390-64_cmprssptrs
        - Archives the SDK and test material for use in downstream jobs
    - Trigger:
        - This job is used in other pipelines but can be launched manually

- Build-linux_ppc-64_cmprssptrs_le
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Build-linux_ppc-64_cmprssptrs_le)](https://ci.eclipse.org/openj9/job/Build-linux_ppc-64_cmprssptrs_le)
    - Description
        - Compiles on linux_ppc-64_cmprssptrs_le
        - Archives the SDK and test material for use in downstream jobs
    - Trigger:
        - This job is used in other pipelines but can be launched manually

#### Infrastructure

- Mirror-OMR-to-OpenJ9-OMR
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Mirror-OMR-to-OpenJ9-OMR)](https://ci.eclipse.org/openj9/job/Mirror-OMR-to-OpenJ9-OMR)
    - Description:
        - Mirrors [eclipse/omr/master](https://github.com/eclipse/omr/tree/master) to [eclipse/openj9-omr/master](https://github.com/eclipse/openj9-omr/tree/master)
        - Triggers `Pipeline-OMR-Acceptance` when there is new content        
    - Trigger:
        - Build periodically, 15 minutes

- Promote-OpenJ9-OMR-master-to-openj9
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Promote-OpenJ9-OMR-master-to-openj9)](https://ci.eclipse.org/openj9/job/Promote-OpenJ9-OMR-master-to-openj9)
    - Description:
        - Promotes eclipse/openj9-omr branch master to branch openj9
        - Lays a tag down on the promoted SHA in the format `omr_merge_YYYYMMDD_HHMMSS` with annotations including the current OpenJ9 and OpenJDK SHAs
    - Trigger:
        - Last step of `Pipeline-OMR-Acceptance`

- Mirror-OpenJ9-Website-to-Eclipse
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Mirror-OpenJ9-Website-to-Eclipse)](https://ci.eclipse.org/openj9/job/Mirror-OpenJ9-Website-to-Eclipse)
    - Description:
        - Mirrors [github.com/eclipse/openj9-website](https://github.com/eclipse/openj9-website/tree/master) to the Eclipse.org repo
    - Trigger:
        - Poll Github repo for changes
 
#### Pipelines

- Pipeline-Build-Test-linux_390-64_cmprssptrs
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Pipeline-Build-Test-linux_390-64_cmprssptrs)](https://ci.eclipse.org/openj9/job/Pipeline-Build-Test-linux_390-64_cmprssptrs)
    - Description:
        - Compile and Test Sanity & Extended
        - Triggers
            - `Build-linux_390-64_cmprssptrs`
            - `Test-Sanity-linux_390-64_cmprssptrs`
            - `Test-Extended-linux_390-64_cmprssptrs`

    - Trigger:
        - build periodically, @midnight

- Pipeline-Build-Test-linux_ppc-64_cmprssptrs_le
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Pipeline-Build-Test-linux_ppc-64_cmprssptrs_le)](https://ci.eclipse.org/openj9/job/Pipeline-Build-Test-linux_ppc-64_cmprssptrs_le)
    - Description:
        - Compile and Test Sanity & Extended
        - Triggers
            - `Build-linux_ppc-64_cmprssptrs_le`
            - `Test-Sanity-linux_ppc-64_cmprssptrs_le`
            - `Test-Extended-linux_ppc-64_cmprssptrs_le`
    - Trigger:
        - build periodically, @midnight
        
- Pipeline-OMR-Acceptance
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Pipeline-OMR-Acceptance)](https://ci.eclipse.org/openj9/job/Pipeline-OMR-Acceptance)
    - Description:
        - Compile and Test Sanity against new OMR content
        - Triggers
            - `Build-linux_390-64_cmprssptrs`
            - `Build-linux_ppc-64_cmprssptrs_le`
            - `Test-Sanity-linux_390-64_cmprssptrs`
            - `Test-Sanity-linux_ppc-64_cmprssptrs_le`
            - `Promote-OpenJ9-OMR-master-to-openj9`
    - Trigger: Triggered by `Mirror-OMR-to-OpenJ9-OMR`

#### Pull Requests

- PullRequest-Compile-linux_390-64_cmprssptrs
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=PullRequest-Compile-linux_390-64_cmprssptrs)](https://ci.eclipse.org/openj9/job/PullRequest-Compile-linux_390-64_cmprssptrs)
    - Description:
        - Compile on linux_390-64_cmprssptrs
    - Trigger:
        - Github PR comment `Jenkins compile`    

- PullRequest-Compile-linux_ppc-64_cmprssptrs_le
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=PullRequest-Compile-linux_ppc-64_cmprssptrs_le)](https://ci.eclipse.org/openj9/job/PullRequest-Compile-linux_ppc-64_cmprssptrs_le)
    - Description:
        - Compile on linux_ppc-64_cmprssptrs_le
    - Trigger:
        - Github PR comment `Jenkins compile`

- PullRequest-Extended-linux_390-64_cmprssptrs
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=PullRequest-Extended-linux_390-64_cmprssptrs)](https://ci.eclipse.org/openj9/job/PullRequest-Extended-linux_390-64_cmprssptrs)
    - Description:
        - Compile and Extended tests on linux_390-64_cmprssptrs
    - Trigger:
        - Github PR comment `Jenkins test extended`    

- PullRequest-Extended-linux_ppc-64_cmprssptrs_le
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=PullRequest-Extended-linux_ppc-64_cmprssptrs_le)](https://ci.eclipse.org/openj9/job/PullRequest-Extended-linux_ppc-64_cmprssptrs_le)
    - Description:
        - Compile and Extended tests on linux_ppc-64_cmprssptrs_le
    - Trigger:
        - Github PR comment `Jenkins test extended`

- PullRequest-Sanity-linux_390-64_cmprssptrs
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=PullRequest-Sanity-linux_390-64_cmprssptrs)](https://ci.eclipse.org/openj9/job/PullRequest-Sanity-linux_390-64_cmprssptrs)
    - Description:
        - Compile and Sanity tests on linux_390-64_cmprssptrs
    - Trigger:
        - Github PR comment `Jenkins test sanity`    

- PullRequest-Sanity-linux_ppc-64_cmprssptrs_le
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=PullRequest-Sanity-linux_ppc-64_cmprssptrs_le)](https://ci.eclipse.org/openj9/job/PullRequest-Sanity-linux_ppc-64_cmprssptrs_le)
    - Description:
        - Compile and Sanity tests on linux_ppc-64_cmprssptrs_le
    - Trigger:
        - Github PR comment `Jenkins test sanity`

- PullRequest-LineEndingsCheck
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=PullRequest-LineEndingsCheck)](https://ci.eclipse.org/openj9/job/PullRequest-LineEndingsCheck)
    - Description:
        - Checks the files modified in a pull request have correct line endings
    - Trigger:
        - Automatically builds on every PR
        - Retrigger with `Jenkins line endings check`

#### Test

- Test-Extended-linux_390-64_cmprssptrs
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Test-Extended-linux_390-64_cmprssptrs)](https://ci.eclipse.org/openj9/job/Test-Extended-linux_390-64_cmprssptrs)
    - Description:
        - Runs extended test suite against the SDK and test material that is passed as parameters
    - Trigger:
        - This job is used in other pipelines but can be launched manually
        
- Test-Extended-linux_ppc-64_cmprssptrs_le
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Test-Extended-linux_ppc-64_cmprssptrs_le)](https://ci.eclipse.org/openj9/job/Test-Extended-linux_ppc-64_cmprssptrs_le)
    - Description:
        - Runs extended test suite against the SDK and test material that is passed as parameters
    - Trigger:
        - This job is used in other pipelines but can be launched manually
        
- Test-Sanity-linux_390-64_cmprssptrs
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Test-Sanity-linux_390-64_cmprssptrs)](https://ci.eclipse.org/openj9/job/Test-Sanity-linux_390-64_cmprssptrs)
    - Description:
        - Runs sanity test suite against the SDK and test material that is passed as parameters
    - Trigger:
        - This job is used in other pipelines but can be launched manually

- Test-Sanity-linux_ppc-64_cmprssptrs_le
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Test-Sanity-linux_ppc-64_cmprssptrs_le)](https://ci.eclipse.org/openj9/job/Test-Sanity-linux_ppc-64_cmprssptrs_le)
    - Description:
        - Runs sanity test suite against the SDK and test material that is passed as parameters
    - Trigger:
        - This job is used in other pipelines but can be launched manually

### Adding Builds
- Always add pipeline style jobs so the code can be committed to the repo once it is ready
- Update this readme when your build is in production use
- Whenever possible, reuse existing builds or existing jenkins files
