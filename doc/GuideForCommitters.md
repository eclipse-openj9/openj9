<!--
Copyright IBM Corp. and others 2019

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


# Eclipse OpenJ9 Committer Guide

This document outlines some general rules for Eclipse OpenJ9 committers to
follow when reviewing and merging pull requests and issues. It also provides
a checklist of items that committers must ensure have been completed prior to
merging a pull request.

Committers should be aware of the
[Eclipse Foundation Generative Artificial Intelligence Usage Guidelines](https://www.eclipse.org/projects/guidelines/genai/)
as well as the
[Eclipse OpenJ9 Generative Artificial Intelligence Usage Guidelines](/CONTRIBUTING.md#Generative-Artificial-Intelligence-Usage-Guidelines).

## General Guidelines

* Committers should not merge their own pull requests.

* If a pull request modifies the [Contribution Guidelines](https://github.com/eclipse-openj9/openj9/blob/master/CONTRIBUTING.md),
request the author to post a detailed summary of the changes on the
`openj9-dev@eclipse.org` mailing list after the pull request is merged.

* Do not merge a pull request if you are not one of the Assignees.

* If you will be one of the primary committers, add yourself to the set of
Assignees. Being the primary committer does not necessarily mean you have to
review the pull request in detail. You may delegate a deeper technical review
to a developer with expertise in a particular area by `@mentioning` them. Even
if you delegate a technical review you should look through the requested changes
anyway as you will ultimately be responsible for merging them.

* Do not merge a pull request if its title is prefixed with `WIP:` (indicating
it's a work-in-progress).

* If a review or feedback was requested by the author by `@mentioning` particular
developers in a pull request, do not merge until all have provided the input
requested.

* Do not merge a pull request until all validation checks and continuous
integration builds have passed. If there are circumstances where you believe
this rule should be violated (for example, a known test failure unrelated to
this change) then the justification must be documented in the pull request
prior to merging.

* If a pull request is to revert a previous commit, ensure there is a reasonable
explanation in the pull request from the author of the rationale along with a
description of the problem if the commit is not reverted. Additionally, committers
should open an issue that links the revert commit to the original PR and tags the
original author to ensure that the cause of the revert is addressed and not lost.

* Regardless of the simplicity of the pull request, explicitly Approve the
changes in the pull request.


## Pre-Merge Checklist

* Ensure the pull request adheres to all the Eclipse OpenJ9 [Contribution Guidelines](https://github.com/eclipse-openj9/openj9/blob/master/CONTRIBUTING.md).

* Ensure pull requests and issues are annotated with descriptive metadata by
attaching GitHub labels. The current set of labels can be found [here](https://github.com/eclipse-openj9/openj9/labels).

* Be sure to validate the commit title and message on **each** commit in the PR (not
just the pull request message) and ensure they describe the contents of the commit.

* Ensure the commits in the pull request are squashed into only as few commits as
is logically required to represent the changes contributed (in other words, superfluous
commits are discouraged without justification). This is difficult to quantify
objectively, but the committer should verify that each commit contains distinct
changes that should not otherwise be logically squashed with other commits in the
same pull request.

* You must initiate pull request builds sufficient to cover
all affected architectures and language levels prior to merging. To launch a pull
request build, see [Triggering PR Builds](https://github.com/eclipse-openj9/openj9/tree/master/buildenv/jenkins).

   If testing is only warranted on a subset of platforms (for example, only files
built on x86 are modified) then pull request testing can be limited to only those
platforms. However, a comment from the committer providing justification is
mandatory.

   If newer commits have been added (either as truly new commits or a rebase of existing
commits), PR builds must be triggered again.

   If any builds fail because of infrastructure issues, they must be restarted.

* When a PR is opened, synchronized, reopened, or set ready for review
(see [pull_request event](https://docs.github.com/en/actions/reference/events-that-trigger-workflows#pull_request)),
a GitHub action to run the JIT linter launches automatically. The linter can be re-run
from the `Checks` tab of the PR if necessary.

* If the code change(s) necessitate change(s) to the [OpenJ9 Documentation](https://www.eclipse.org/openj9/docs/),
first add the `depends:doc` label to the OpenJ9 PR, and then ensure the contributer
has opened an associated PR in the [openj9-docs](https://github.com/eclipse-openj9/openj9-docs)
repository. An OpenJ9 PR that requires documentation changes should not be merged
until the associated `openj9-docs` PR is also approved and ready to be merged.


## Coordinated Merges With Eclipse OMR

While changes requiring commits to occur in both the Eclipse OMR and
OpenJ9 projects should ideally be staged to manage any dependencies,
there are circumstances when this is not possible.  In such cases, a
"coordinated merge" in both projects is required to prevent build
breaks in the Eclipse OpenJ9 project.

**The following can only be done by those who are committers in both
the Eclipse OMR and Eclipse OpenJ9 projects.**

Note: it is important to perform these steps with as little delay as
possible between them.

1.  Launch a pull request build from the OpenJ9 pull request that you
    wish to merge that performs the same tests that the OMR Acceptance
    build.  This ensures that the two changes put together would pass
    an OMR Acceptance build if one were launched.
    ```
    Jenkins test sanity.functional,sanity.openjdk all jdk8,jdk11,jdk17,jdk21 depends eclipse/omr#{OMR PR number}
    ```
    Before proceeding with the coordinated merge, you must ensure that
    the build is successful and that it was performed relatively
    recently (generally within a day of a successful build).

    It is strongly recommended that you check that the tips of the
    `eclipse-openj9/openj9-omr` [`master`](https://github.com/eclipse-openj9/openj9-omr/tree/master),
    `eclipse-openj9/openj9-omr` [`openj9`](https://github.com/eclipse-openj9/openj9-omr/tree/openj9)
    and `eclipse-omr/omr` [`master`](https://github.com/eclipse-omr/omr/tree/master)
    branches are all the same before proceeding.  If they are different,
    be aware that you will be introducing other OMR changes that have
    not yet passed an OMR Acceptance build.  While not strictly
    required for the following steps to succeed, you should use your
    discretion and proceed with caution.

2.  Announce your intention to perform a coordinated merge on the OpenJ9
    [`#committers-public`](https://openj9.slack.com/archives/C8PQL5N65)
    Slack channel.

3.  In the Eclipse OMR project, merge the OMR pull request.

4.  Ensure you are logged in to the [Eclipse OpenJ9 Jenkins instance](https://openj9-jenkins.osuosl.org/).

5.  Launch a ["Mirror-OMR-To-OpenJ9-OMR Build"](https://openj9-jenkins.osuosl.org/job/Mirror-OMR-to-OpenJ9-OMR/)
    job in the Eclipse OpenJ9 project by clicking "Build Now".  This will
    pull the latest Eclipse OMR project `master` branch into the
    [`eclipse-openj9/openj9-omr`](https://github.com/eclipse-openj9/openj9-omr) repo's
    `master` branch and automatically launch an OMR Acceptance Build at
    OpenJ9 Jenkins.

6.  Cancel the [OMR Acceptance build](https://openj9-jenkins.osuosl.org/job/Pipeline-OMR-Acceptance/)
    that is automatically triggered by the previous step.  It is not
    necessary to wait for this build to finish because an equivalent
    of the OMR Acceptance Build was already tested in Step 1.

7.  Verify that the [master branch](https://github.com/eclipse-openj9/openj9-omr/commits/master)
    of the [eclipse-openj9/openj9-omr](https://github.com/eclipse-openj9/openj9-omr)
    repo contains the Eclipse OMR commit you merged in Step 3.
    Make note of the commit SHA for Step 8.

8.  Launch a ["Promote_OMR"](https://openj9-jenkins.osuosl.org/job/Promote_OMR/)
    job on the SHA of the commit that was just merged in the Eclipse
    OMR repo.

    | Field          | Value                                       |
    | :------------- | :------------------------------------------ |
    | REPO           | `https://github.com/eclipse-openj9/openj9-omr.git` |
    | COMMIT         | *SHA of the OMR commit from Step 7*         |
    | TARGET_BRANCH  | `openj9`                                    |

    This will cause the OMR commit to be promoted from the
    `master` branch into the `openj9` branch of the `eclipse-openj9/openj9-omr`
    repo.

9.  When the Promotion Build in Step 8 completes, merge the OpenJ9
    commit.

    At this point, the OMR commit should now be in the `openj9` branch of
    the `eclipse-openj9/openj9-omr` repo, and the OpenJ9 commit should now be
    in the `master` branch of the `eclipse-openj9/openj9` repo.

10. Announce in the `#committers-public` Slack channel that the coordinated
    merge has completed.
