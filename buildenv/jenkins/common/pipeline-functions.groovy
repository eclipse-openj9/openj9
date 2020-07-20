/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

def get_shas(OPENJDK_REPO, OPENJDK_BRANCH, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, VENDOR_TEST_REPOS_MAP=null, VENDOR_TEST_BRANCHES_MAP=null, VENDOR_TEST_SHAS_MAP=null) {
    // Get a set of SHAs for a standard OpenJ9 build
    def SHAS = [:]

    // check if the SHAs are set as build parameters
    // if not set, sniff the remote repositories references

    stage ('Sniff Repos') {
        def description = ''

        if (OPENJDK_SHA instanceof Map && OPENJDK_REPO instanceof Map && OPENJDK_BRANCH instanceof Map) {
            // fetch SHAs for all OpenJDK repositories
            def OPENJDK_SHAS_BY_RELEASES = [:]

            // e.g. OPENJDK_SHA = [ 8: [linux_x86-64: sha1, linux_x86-64_cmprssptrs: sha2, ...],
            //                     10: [linux_x86-64: sha1, linux_x86-64_cmprssptrs: sha2, ...],...]
            OPENJDK_SHA.each { release, shas_by_specs ->
                def unique_shas = [:]

                shas_by_specs.each { spec, sha ->
                    def repoUrl = OPENJDK_REPO.get(release).get(spec)

                    if (!unique_shas.containsKey(repoUrl)) {
                        if (!sha) {
                            sha = get_repository_sha(repoUrl, OPENJDK_BRANCH.get(release).get(spec))
                        }
                        unique_shas.put(repoUrl, sha)

                        def repo_short_name = repoUrl.substring(repoUrl.lastIndexOf('/') + 1, repoUrl.indexOf('.git'))
                        description += "<br/>OpenJDK${release}: <a href=${get_http_repo_url(repoUrl)}/commit/${sha}>${get_short_sha(sha)}</a> - ${repo_short_name}"
                        echo "OPENJDK${release}_SHA:${sha} - ${repo_short_name}"
                    }
                }
                OPENJDK_SHAS_BY_RELEASES.put(release, unique_shas)
            }

            SHAS['OPENJDK'] = OPENJDK_SHAS_BY_RELEASES
        } else {
            // fetch SHA for an OpenJDK repository
            SHAS['OPENJDK'] = OPENJDK_SHA
            if (!SHAS['OPENJDK']) {
                SHAS['OPENJDK'] = get_repository_sha(OPENJDK_REPO, OPENJDK_BRANCH)
            }
            description += "<br/>OpenJDK: <a href=${get_http_repo_url(OPENJDK_REPO)}/commit/${SHAS['OPENJDK']}>${get_short_sha(SHAS['OPENJDK'])}</a>"
            echo "OPENJDK_SHA:${SHAS['OPENJDK']}"
        }

        // fetch extensions SHAs
        SHAS['OPENJ9'] = OPENJ9_SHA
        if (!SHAS['OPENJ9'] && (OPENJ9_REPO && OPENJ9_BRANCH)) {
            SHAS['OPENJ9'] = get_repository_sha(OPENJ9_REPO, OPENJ9_BRANCH)
        }

        SHAS['OMR'] = OMR_SHA
        if (!SHAS['OMR'] && (OMR_REPO && OMR_BRANCH)) {
            SHAS['OMR'] = get_repository_sha(OMR_REPO, OMR_BRANCH)
        }

        // Write the SHAs to the Build Description
        echo "OPENJ9_SHA:${SHAS['OPENJ9']}"
        echo "OMR_SHA:${SHAS['OMR']}"
        def TMP_DESC = (currentBuild.description) ? currentBuild.description + "<br>" : ""
        currentBuild.description = TMP_DESC + "OpenJ9: <a href=${get_http_repo_url(OPENJ9_REPO)}/commit/${SHAS['OPENJ9']}>${get_short_sha(SHAS['OPENJ9'])}</a><br/>OMR: <a href=${get_http_repo_url(OMR_REPO)}/commit/${SHAS['OMR']}>${get_short_sha(SHAS['OMR'])}</a>${description}"

        if (VENDOR_TEST_REPOS_MAP && VENDOR_TEST_BRANCHES_MAP) {
            // fetch SHAs for vendor test repositories
            VENDOR_TEST_REPOS_MAP.each { repoName, vendorRepoURL ->
                if (!VENDOR_TEST_SHAS_MAP[repoName]) {
                    VENDOR_TEST_SHAS_MAP[repoName] = get_repository_sha(vendorRepoURL, VENDOR_TEST_BRANCHES_MAP[repoName])
                }

                // update build description
                currentBuild.description += "<br/>${repoName}: <a href=${get_http_repo_url(OPENJ9_REPO)}/commit/${VENDOR_TEST_SHAS_MAP[repoName]}>${get_short_sha(VENDOR_TEST_SHAS_MAP[repoName])}</a>"
                echo "${repoName}_SHA: ${VENDOR_TEST_SHAS_MAP[repoName]}"
            }

            // add vendor test repositories SHAs to the list
            SHAS['VENDOR_TEST'] = VENDOR_TEST_SHAS_MAP
        }

        return SHAS
    }
}

def get_repository_sha(REPO, BRANCH) {
   // use ssh-agent to avoid permission denied on private repositories
    if (USER_CREDENTIALS_ID != '') {
        return sshagent(credentials:["${USER_CREDENTIALS_ID}"]) {
            get_sha(REPO, BRANCH)
        }
    }

    return get_sha(REPO, BRANCH)
}

def get_sha(REPO, BRANCH) {
    // Get the SHA at the tip of the BRANCH in REPO.
    // Allows Pipelines to kick off multiple builds and have the same SHA built everywhere.
    return sh (
            // "git ls-remote $REPO" will return all refs, adding "$BRANCH" will only return the specific branch we are interested in
            // return the full 40 characters sha instead of the short version
            // to avoid errors due to short sha ambiguousness due to multiple matches for a short sha
            script: "git ls-remote $REPO refs/heads/$BRANCH | cut -c1-40",
            returnStdout: true
        ).trim()
}

def get_short_sha(SHA) {
    if (SHA) {
        // return the first 7 characters of a given SHA.
        return SHA.take(7)
    }

    return SHA
}

def get_http_repo_url(repo) {
    return repo.replace("git@", "https://").replace("com:", "com/").replace(".git", "")
}

def git_push_auth(REPO, OPTION, CRED_ID) {
    REPO -= 'https://'
    withCredentials([usernamePassword(credentialsId: "${CRED_ID}", usernameVariable: 'USERNAME', passwordVariable: 'PASSWORD')]) {
        sh "git push https://${USERNAME}:${PASSWORD}@${REPO} ${OPTION}"
    }
}

/*
 * Set Github Commit Status
 *
 * Args:
 * - REPO: Repository to update status
 * - CONTEXT: Usually Build Name
 * - SHA: Git commit to update
 * - URL: Link to build details
 * - STATE: Build Result, one of PENDING, SUCCESS, FAILURE, ERROR
 * - MESSAGE: Build result message
 */
def set_build_status(REPO, CONTEXT, SHA, URL, STATE, MESSAGE) {
    step([
        $class: "GitHubCommitStatusSetter",
        reposSource: [$class: "ManuallyEnteredRepositorySource", url: REPO],
        contextSource: [$class: "ManuallyEnteredCommitContextSource", context: CONTEXT],
        errorHandlers: [[$class: "ChangingBuildStatusErrorHandler", result: "UNSTABLE"]],
        commitShaSource: [$class: "ManuallyEnteredShaSource", sha: SHA ],
        statusBackrefSource: [$class: "ManuallyEnteredBackrefSource", backref: URL],
        statusResultSource: [$class: "ConditionalStatusResultSource", results: [[$class: "AnyBuildResult", message: MESSAGE, state: STATE]] ]
    ]);
}

def cancel_running_builds(JOB_NAME, BUILD_IDENTIFIER) {
    echo "Looking to stop running jobs:'${JOB_NAME}' from build:'${BUILD_IDENTIFIER}'"
    Jenkins.instance.getItemByFullName(JOB_NAME, Job.class)._getRuns().each { number, run ->
        // get all runs/jobs from the build
        // Class org.jenkinsci.plugins.workflow.job.WorkflowRun
        // Only look for running jobs with matching BUILD_IDENTIFIER
        if ((run) && (run.isBuilding()) && (run.getEnvironment().get('BUILD_IDENTIFIER') == "${BUILD_IDENTIFIER}")) {
            echo "Cancelling Job:'${run}'"
            run.doStop()
        }
    }
    echo "Done stopping jobs"
}

def build(BUILD_JOB_NAME, OPENJDK_REPO, OPENJDK_BRANCH, OPENJDK_SHA, OPENJ9_REPO, OPENJ9_BRANCH, OPENJ9_SHA, OMR_REPO, OMR_BRANCH, OMR_SHA, VARIABLE_FILE, VENDOR_REPO, VENDOR_BRANCH, VENDOR_CREDENTIALS_ID, NODE, SETUP_LABEL, BUILD_IDENTIFIER, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, EXTRA_GETSOURCE_OPTIONS, EXTRA_CONFIGURE_OPTIONS, EXTRA_MAKE_OPTIONS, OPENJDK_CLONE_DIR, CUSTOM_DESCRIPTION, ghprbPullId, ghprbCommentBody, ghprbTargetBranch, ARCHIVE_JAVADOC) {
    stage ("${BUILD_JOB_NAME}") {
        return build_with_slack(BUILD_JOB_NAME, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER,
            [string(name: 'OPENJDK_REPO', value: OPENJDK_REPO),
            string(name: 'OPENJDK_BRANCH', value: OPENJDK_BRANCH),
            string(name: 'OPENJDK_SHA', value: OPENJDK_SHA),
            string(name: 'OPENJ9_REPO', value: OPENJ9_REPO),
            string(name: 'OPENJ9_BRANCH', value: OPENJ9_BRANCH),
            string(name: 'OPENJ9_SHA', value: OPENJ9_SHA),
            string(name: 'OMR_REPO', value: OMR_REPO),
            string(name: 'OMR_BRANCH', value: OMR_BRANCH),
            string(name: 'OMR_SHA', value: OMR_SHA),
            string(name: 'VARIABLE_FILE', value: VARIABLE_FILE),
            string(name: 'VENDOR_REPO', value: VENDOR_REPO),
            string(name: 'VENDOR_BRANCH', value: VENDOR_BRANCH),
            string(name: 'VENDOR_CREDENTIALS_ID', value: VENDOR_CREDENTIALS_ID),
            string(name: 'NODE', value: NODE),
            string(name: 'SETUP_LABEL', value: SETUP_LABEL),
            string(name: 'BUILD_IDENTIFIER', value: BUILD_IDENTIFIER),
            string(name: 'EXTRA_GETSOURCE_OPTIONS', value: EXTRA_GETSOURCE_OPTIONS),
            string(name: 'EXTRA_CONFIGURE_OPTIONS', value: EXTRA_CONFIGURE_OPTIONS),
            string(name: 'EXTRA_MAKE_OPTIONS', value: EXTRA_MAKE_OPTIONS),
            string(name: 'OPENJDK_CLONE_DIR', value: OPENJDK_CLONE_DIR),
            string(name: 'CUSTOM_DESCRIPTION', value: CUSTOM_DESCRIPTION),
            string(name: 'ghprbPullId', value: ghprbPullId),
            string(name: 'ghprbGhRepository', value: ghprbGhRepository),
            string(name: 'ghprbCommentBody', value: ghprbCommentBody),
            string(name: 'ghprbTargetBranch', value: ghprbTargetBranch),
            string(name: 'SCM_BRANCH', value: SCM_BRANCH),
            string(name: 'SCM_REFSPEC', value: SCM_REFSPEC),
            string(name: 'SCM_REPO', value: SCM_REPO),
            booleanParam(name: 'ARCHIVE_JAVADOC', value: ARCHIVE_JAVADOC)])
    }
}

def test(JOB_NAME, UPSTREAM_JOB_NAME, UPSTREAM_JOB_NUMBER, NODE, OPENJ9_REPO, OPENJ9_BRANCH, OPENJ9_SHA, VENDOR_TEST_REPOS, VENDOR_TEST_BRANCHES, VENDOR_TEST_SHAS, VENDOR_TEST_DIRS, USER_CREDENTIALS_ID, CUSTOMIZED_SDK_URL, ARTIFACTORY_CREDS, TEST_FLAG, BUILD_IDENTIFIER, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH, PARALLEL, extraTestLabels, keepReportDir, buildList, NUM_MACHINES, OPENJDK_REPO, OPENJDK_BRANCH) {
    stage ("${JOB_NAME}") {
        def testParams = []
        testParams.addAll([string(name: 'LABEL', value: NODE),
            string(name: 'LABEL_ADDITION', value: extraTestLabels),
            string(name: 'ADOPTOPENJDK_REPO', value: ADOPTOPENJDK_REPO),
            string(name: 'ADOPTOPENJDK_BRANCH', value: ADOPTOPENJDK_BRANCH),
            string(name: 'OPENJ9_REPO', value: OPENJ9_REPO),
            string(name: 'OPENJ9_BRANCH', value: OPENJ9_BRANCH),
            string(name: 'OPENJ9_SHA', value: OPENJ9_SHA),
            string(name: 'JDK_REPO', value: OPENJDK_REPO),
            string(name: 'JDK_BRANCH', value: OPENJDK_BRANCH),
            string(name: 'VENDOR_TEST_REPOS', value: VENDOR_TEST_REPOS),
            string(name: 'VENDOR_TEST_BRANCHES', value: VENDOR_TEST_BRANCHES),
            string(name: 'VENDOR_TEST_SHAS', value: VENDOR_TEST_SHAS),
            string(name: 'VENDOR_TEST_DIRS', value: VENDOR_TEST_DIRS),
            string(name: 'USER_CREDENTIALS_ID', value: USER_CREDENTIALS_ID),
            string(name: 'TEST_FLAG', value: TEST_FLAG),
            string(name: 'KEEP_REPORTDIR', value: keepReportDir),
            string(name: 'BUILD_IDENTIFIER', value: BUILD_IDENTIFIER),
            string(name: 'PARALLEL', value: PARALLEL),
            string(name: 'NUM_MACHINES', value: NUM_MACHINES)])
        if (ARTIFACTORY_CREDS) {
            testParams.addAll([string(name: 'CUSTOMIZED_SDK_URL', value: CUSTOMIZED_SDK_URL),
                string(name: 'CUSTOMIZED_SDK_URL_CREDENTIAL_ID', value: ARTIFACTORY_CREDS)])
        } else {
            testParams.addAll([string(name: 'UPSTREAM_JOB_NAME', value: UPSTREAM_JOB_NAME),
            string(name: 'UPSTREAM_JOB_NUMBER', value: "${UPSTREAM_JOB_NUMBER}")])
        }
        // If BUILD_LIST is set, pass it, otherwise don't pass it in order to pickup the default in the test job config.
        if (buildList) {
            testParams.add(string(name: 'BUILD_LIST', value: buildList))
        }
        return build_with_slack(JOB_NAME, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, testParams)
    }
}

def get_causes(job) {
    def cause_string = ''
    for (cause in job.rawBuild.getCauses()) {
        cause_string += get_causes_recursively(cause)
    }
    return cause_string
}

def get_causes_recursively(cause) {
    def cause_string = cause.getShortDescription()
    if (cause instanceof hudson.model.Cause.UpstreamCause) {
        for (upCause in cause.getUpstreamCauses()) {
            cause_string += '\n' + get_causes_recursively(upCause)
        }
    }
    return cause_string
}

def build_with_slack(DOWNSTREAM_JOB_NAME, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, PARAMETERS) {
    GITHUB_REPO = "https://${GITHUB_SERVER}/${ghprbGhRepository}"
    // Set Github Commit Status
    if (ghprbActualCommit) {
        node(SETUP_LABEL) {
            try {
                retry_and_delay({set_build_status(GITHUB_REPO, DOWNSTREAM_JOB_NAME, ghprbActualCommit, BUILD_URL, 'PENDING', "Build Started")})
            } catch (e) {
                println "Failed to set the GitHub commit status to PENDING when the job STARTED"
            }
        }
    }

    def JOB = build job: DOWNSTREAM_JOB_NAME, parameters: PARAMETERS, propagate: false
    def DOWNSTREAM_JOB_NUMBER = JOB.getNumber()
    def DOWNSTREAM_JOB_URL = JOB.getAbsoluteUrl()
    def DOWNSTREAM_JOB_TIME = JOB.getDurationString()

    if (JOB.resultIsWorseOrEqualTo('UNSTABLE')) {
        // Get build causes
        build_causes_string = get_causes(currentBuild)

        if ((JOB.result == "UNSTABLE") || (JOB.result == "ABORTED")) { // Job unstable (YELLOW) or aborted (GREY)
            // If the job was ABORTED it either was killed by someone or
            // killed by cancel_running_builds(). In this case we don't
            // want to offer the restart option as it is not desired for PR builds.
            // If the job was UNSTABLE this indicates test(s) failed and we don't want to offer the restart option.
            echo "WARNING: Downstream job ${DOWNSTREAM_JOB_NAME} is ${JOB.result} after ${DOWNSTREAM_JOB_TIME}. Job Number: ${DOWNSTREAM_JOB_NUMBER} Job URL: ${DOWNSTREAM_JOB_URL}"
            def slack_colour = ''
            if (JOB.result == "UNSTABLE") {
                slack_colour = 'warning'
                unstable "Setting overall pipeline status to UNSTABLE"
            } else {
                // Job aborted (GREY)
                slack_colour = '#808080'
                currentBuild.result = JOB.result
            }
            if (SLACK_CHANNEL) {
                slackSend channel: SLACK_CHANNEL, color: slack_colour, message: "${JOB.result}: ${DOWNSTREAM_JOB_NAME} #${DOWNSTREAM_JOB_NUMBER} (<${DOWNSTREAM_JOB_URL}|Open>)\nStarted by ${JOB_NAME} #${BUILD_NUMBER} (<${BUILD_URL}|Open>)\n${build_causes_string}"
            }
            // Set Github Commit Status
            if (ghprbActualCommit) {
                node(SETUP_LABEL) {
                    try {
                        retry_and_delay({set_build_status(GITHUB_REPO, DOWNSTREAM_JOB_NAME, ghprbActualCommit, DOWNSTREAM_JOB_URL, 'FAILURE', "Build ${JOB.result}")})
                    } catch (e) {
                        println "Failed to set the GitHub commit status to FAILURE when the job ${JOB.result}"
                    }
                }
            }
        } else { // Job failed (RED)
            if (SLACK_CHANNEL) {
                slackSend channel: SLACK_CHANNEL, color: 'danger', message: "Failure: ${DOWNSTREAM_JOB_NAME} #${DOWNSTREAM_JOB_NUMBER} (<${DOWNSTREAM_JOB_URL}|Open>)\nStarted by ${JOB_NAME} #${BUILD_NUMBER} (<${BUILD_URL}|Open>)\n${build_causes_string}\nWould you like to restart the job? (<${RUN_DISPLAY_URL}|Open>)"
            }
            // Set Github Commit Status
            if (ghprbActualCommit) {
                node(SETUP_LABEL) {
                    try {
                        retry_and_delay({set_build_status(GITHUB_REPO, DOWNSTREAM_JOB_NAME, ghprbActualCommit, DOWNSTREAM_JOB_URL, 'FAILURE', "Build FAILED")})
                    } catch (e) {
                        println "Failed to set the GitHub commit status to FAILURE when the job Failed"
                    }
                }
            }
            timeout(time: RESTART_TIMEOUT.toInteger(), unit: RESTART_TIMEOUT_UNITS) {
                input message: "Downstream job ${DOWNSTREAM_JOB_NAME} failed after ${DOWNSTREAM_JOB_TIME}. Job Number: ${DOWNSTREAM_JOB_NUMBER} Job URL: ${DOWNSTREAM_JOB_URL}\nRestart failed job '${DOWNSTREAM_JOB_NAME}'?", ok: 'Restart'
                // If restart is aborted or is timed-out, an error is thrown
            }
            // If restart is approved, recursively call this function until we get a pass or a restart-rejection
            return build_with_slack(DOWNSTREAM_JOB_NAME, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, PARAMETERS)
        }
    } else {
        echo "Downstream job ${DOWNSTREAM_JOB_NAME} PASSED after ${DOWNSTREAM_JOB_TIME}"
        // Set Github Commit Status
        if (ghprbActualCommit) {
            node(SETUP_LABEL) {
                try {
                    retry_and_delay({set_build_status(GITHUB_REPO, DOWNSTREAM_JOB_NAME, ghprbActualCommit, DOWNSTREAM_JOB_URL, 'SUCCESS', "Build PASSED")})
                } catch (e) {
                    println "Failed to set the Github commit status to SUCCESS when the job PASSED"
                }
            }
        }
    }
    return JOB
}

def workflow(SDK_VERSION, SPEC, SHAS, OPENJDK_REPO, OPENJDK_BRANCH, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, TESTS_TARGETS, VENDOR_TEST_REPOS_MAP, VENDOR_TEST_BRANCHES_MAP, VENDOR_TEST_DIRS_MAP, USER_CREDENTIALS_ID, SETUP_LABEL, ghprbGhRepository, ghprbActualCommit, EXTRA_GETSOURCE_OPTIONS, EXTRA_CONFIGURE_OPTIONS, EXTRA_MAKE_OPTION, OPENJDK_CLONE_DIR, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH, BUILD_JOB_NAME, CUSTOM_DESCRIPTION, ARCHIVE_JAVADOC) {
    def jobs = [:]

    // Set ghprbGhRepository and ghprbActualCommit for the purposes of Github commit status updates
    // Only for Nightly build
    // PR build will already have them set
    GITHUB_SERVER = OPENJ9_REPO.substring(OPENJ9_REPO.indexOf('github.'), OPENJ9_REPO.indexOf('.com') +4)
    if (BUILD_IDENTIFIER.toLowerCase() == "nightly") {
        ghprbGhRepository = OPENJ9_REPO.substring(OPENJ9_REPO.indexOf('.com') +5, OPENJ9_REPO.indexOf('.git'))
        ghprbActualCommit = SHAS['OPENJ9']
    }
    echo "Repo:${ghprbGhRepository}, Commit:${ghprbActualCommit}, GITHUB_SERVER:${GITHUB_SERVER}"

    // compile the source and build the SDK
    jobs["build"] = build(BUILD_JOB_NAME, OPENJDK_REPO, OPENJDK_BRANCH, SHAS['OPENJDK'], OPENJ9_REPO, OPENJ9_BRANCH, SHAS['OPENJ9'], OMR_REPO, OMR_BRANCH, SHAS['OMR'], params.VARIABLE_FILE, params.VENDOR_REPO, params.VENDOR_BRANCH, params.VENDOR_CREDENTIALS_ID, params.BUILD_NODE, SETUP_LABEL, BUILD_IDENTIFIER, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, EXTRA_GETSOURCE_OPTIONS, EXTRA_CONFIGURE_OPTIONS, EXTRA_MAKE_OPTIONS, OPENJDK_CLONE_DIR, CUSTOM_DESCRIPTION, ghprbPullId, ghprbCommentBody, ghprbTargetBranch, ARCHIVE_JAVADOC)

    // Determine if Build job archived to Artifactory
    def BUILD_JOB_ENV = jobs["build"].getBuildVariables()
    ARTIFACTORY_CREDS = ''
    CUSTOMIZED_SDK_URL = ''
    echo "BUILD_JOB_ENV:'${BUILD_JOB_ENV}'"
    if (BUILD_JOB_ENV['CUSTOMIZED_SDK_URL']) {
        CUSTOMIZED_SDK_URL = BUILD_JOB_ENV['CUSTOMIZED_SDK_URL']
        ARTIFACTORY_CREDS = BUILD_JOB_ENV['ARTIFACTORY_CREDS']
        ARTIFACTORY_SERVER = BUILD_JOB_ENV['ARTIFACTORY_SERVER']
        ARTIFACTORY_REPO = BUILD_JOB_ENV['ARTIFACTORY_REPO']
        ARTIFACTORY_NUM_ARTIFACTS = BUILD_JOB_ENV['ARTIFACTORY_NUM_ARTIFACTS']
        ARTIFACTORY_MANUAL_CLEANUP = BUILD_JOB_ENV['ARTIFACTORY_MANUAL_CLEANUP']
        echo "Passing CUSTOMIZED_SDK_URL:'${CUSTOMIZED_SDK_URL}'"
        echo "Using ARTIFACTORY_CREDS:'${ARTIFACTORY_CREDS}'"
        echo "Using ARTIFACTORY_SERVER:'${ARTIFACTORY_SERVER}'"
        echo "Passing ARTIFACTORY_MANUAL_CLEANUP:'${ARTIFACTORY_MANUAL_CLEANUP}'"

        cleanup_artifactory(ARTIFACTORY_MANUAL_CLEANUP, BUILD_JOB_NAME, ARTIFACTORY_SERVER, ARTIFACTORY_REPO, ARTIFACTORY_NUM_ARTIFACTS)
    }

    if (TESTS) {
        def testJobs = [:]

        if (SHAS['VENDOR_TEST']) {
            // the downstream job is expecting comma separated SHAs
            // convert vendor shas map to a string
            VENDOR_TEST_SHAS = SHAS['VENDOR_TEST'].values().join(',')
        }

        echo "Using VENDOR_TEST_REPOS = ${VENDOR_TEST_REPOS}, VENDOR_TEST_BRANCHES = ${VENDOR_TEST_BRANCHES}, VENDOR_TEST_SHAS = ${VENDOR_TEST_SHAS}, VENDOR_TEST_DIRS = ${VENDOR_TEST_DIRS}"

        // For PullRequest Builds, overwrite the OpenJ9 sha for test jobs so they checkout the PR (OpenJ9 PRs only)
        if (params.ghprbPullId && params.ghprbGhRepository == 'eclipse/openj9') {
            SHAS['OPENJ9'] = "origin/pr/${params.ghprbPullId}/merge"
        }

        TESTS.each { id, target ->
            def testFlag = target['testFlag']
            def extraTestLabels = target['extraTestLabels']
            def keepReportDir = target['keepReportDir']
            def buildList = target['buildList']
            echo "Test:'${id}' testFlag:'${testFlag}' extraTestLabels:'${extraTestLabels}', keepReportDir:'${keepReportDir}'"

            def testJobName = get_test_job_name(id, SPEC, SDK_VERSION, BUILD_IDENTIFIER)

            def PARALLEL = "None"
            if (testJobName.contains("special.system")) {
                PARALLEL = "Subdir"
            }

            def NUM_MACHINES = ""
            if (testJobName.contains("functional")) {
                PARALLEL = "Dynamic"
                NUM_MACHINES = "2"
            }
            testJobs[id] = {
                if (params.ghprbPullId) {
                    cancel_running_builds(testJobName, BUILD_IDENTIFIER)
                }
                if (ARTIFACTORY_CREDS) {
                    cleanup_artifactory(ARTIFACTORY_MANUAL_CLEANUP, testJobName, ARTIFACTORY_SERVER, ARTIFACTORY_REPO, ARTIFACTORY_NUM_ARTIFACTS)
                }
                jobs[id] = test(testJobName, BUILD_JOB_NAME, jobs["build"].getNumber(), TEST_NODE, OPENJ9_REPO, OPENJ9_BRANCH, SHAS['OPENJ9'], VENDOR_TEST_REPOS, VENDOR_TEST_BRANCHES, VENDOR_TEST_SHAS, VENDOR_TEST_DIRS, USER_CREDENTIALS_ID, CUSTOMIZED_SDK_URL, ARTIFACTORY_CREDS, testFlag, BUILD_IDENTIFIER, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH, PARALLEL, extraTestLabels, keepReportDir, buildList, NUM_MACHINES, OPENJDK_REPO, OPENJDK_BRANCH)
            }
        }
        if (params.AUTOMATIC_GENERATION != 'false') {
            generate_test_jobs(TESTS, SPEC, ARTIFACTORY_SERVER, ARTIFACTORY_REPO)
        }
        parallel testJobs
    }

    // return jobs for further reference
    return jobs
}

def get_build_job_name(spec, version, identifier) {
    id = convert_build_identifier(identifier)
    return "Build_JDK${version}_${spec}_${id}"
}

def get_test_job_name(targetName, spec, version, identifier) {
    // No need to use function 'move_spec_suffix_to_id' since the test job name returned below will match.
    // Although we may want to in case behaviour changes.
    def id = convert_build_identifier(identifier)
    if (spec ==~ /.*_valhalla/) {
        version = 'valhalla'
    }
    return "Test_openjdk${version}_${SDK_IMPL_SHORT}_${targetName}_${spec}_${id}"
}

def convert_build_identifier(val) {
    switch (val) {
        case "Nightly":
        case "Release":
            return val
            break
        case ~/^.*-Acceptance$/:
            return val.substring(0,val.indexOf('-'))
            break
        default:
            return "Personal"
    }
}

/*
 * Finds the downstream builds of a build.
 * Limits the search only to the downstream builds with given names.
 * Returns a map of builds by job name, where builds is a list of builds in
 * descending order.
 */
def get_downstream_builds(upstreamBuild, upstreamJobName, downstreamJobNames) {
    def downstreamBuilds = [:]
    def pattern = /.*${upstreamJobName}.*${upstreamBuild.number}.*/
    def startTime = System.currentTimeMillis()

    for (name in downstreamJobNames.sort()) {
        def job = Jenkins.getInstance().getItemByFullName(name)
        if (job) {
            def builds = []
            //find downstream jobs
            for (build in job.getBuilds().byTimestamp(upstreamBuild.getStartTimeInMillis(), System.currentTimeMillis())) {
                if ((build && build.getCause(hudson.model.Cause.UpstreamCause) && build.getCause(hudson.model.Cause.UpstreamCause).upstreamRun)
                    && (build.getCause(hudson.model.Cause.UpstreamCause).upstreamRun==~pattern)) {
                        // cache all builds (in case of multiple runs)
                        builds.add(build)
                }
            }

            if (!builds.isEmpty()) {
                downstreamBuilds.put(name, builds)
            }
        }
    }

    elapsedTime = (System.currentTimeMillis() - startTime)/1000
    echo "Time spent fetching downstream jobs for ${upstreamJobName}: ${elapsedTime} seconds"

    return downstreamBuilds
}

/*
 * Returns the build status icon.
 * Requires Embeddable Build Status plugin.
 */
def get_build_embedded_status_link(build) {
    if (build) {
        def buildLink = "${JENKINS_URL}${build.getUrl()}"
        return "<a href=\"${buildLink}\"><img src=\"${buildLink}badge/icon\"></a>"
    }
    return ''
}

/*
 * Returns a map storing the downstream job names for given version and spec.
 */
def get_downstream_job_names(spec, version, identifier) {
    /* e.g. [build:               Build-JDK11-linux_390-64_cmprssptrs,
     *       extended.functional: Test-extended.functional-JDK11-linux_390-64_cmprssptrs,
     *       extended.system:     Test-extended.system-JDK11-linux_390-64_cmprssptrs,
     *       sanity.functional:   Test-sanity.functional-JDK11-linux_390-64_cmprssptrs,
     *       sanity.system:       Test-sanity.system-JDK11-linux_390-64_cmprssptrs]
     */

    downstreamJobNames = [:]
    downstreamJobNames['build'] = get_build_job_name(spec, version, identifier)

    TESTS.each { id, target ->
        downstreamJobNames[id] = get_test_job_name(id, spec, version, identifier)
    }

    return downstreamJobNames
}

def cleanup_artifactory(artifactory_manual_cleanup, job_name, artifactory_server, artifactory_repo, artifactory_num_artifacts) {
    if (artifactory_manual_cleanup == 'true') {
        try {
            def cleanup_job_params = [
                string(name: 'JOB_TYPE', value: 'COUNT'),
                string(name: 'JOB_TO_CHECK', value: job_name),
                string(name: 'ARTIFACTORY_SERVER', value: artifactory_repo),
                string(name: 'ARTIFACTORY_REPO', value: artifactory_repo),
                string(name: 'ARTIFACTORY_NUM_ARTIFACTS', value: artifactory_num_artifacts)]

            build job: 'Cleanup_Artifactory', parameters: cleanup_job_params, wait: false
        } catch (any) {
            echo 'The Cleanup_Artifactory job is not available'
        }
    }
}

def generate_test_jobs(TESTS, SPEC, ARTIFACTORY_SERVER, ARTIFACTORY_REPO) {
    def levels = []
    def groups = []

    TESTS.each { id, target ->
        def splitTarget = id.tokenize('.')
        levels.add(splitTarget[0])
        groups.add(splitTarget[1])
    }
    levels.unique(true)
    groups.unique(true)

    def spec_id = move_spec_suffix_to_id(SPEC, convert_build_identifier(BUILD_IDENTIFIER))
    def sdk_version = SDK_VERSION
    def auto_detect = true
    if (SPEC ==~ /.*valhalla/) {
        // Test expects Valhalla with a capital V
        sdk_version = 'Valhalla'
        auto_detect = false
    }

    if (levels && groups) {
        def parameters = [
            string(name: 'LEVELS', value: levels.join(',')),
            string(name: 'GROUPS', value: groups.join(',')),
            string(name: 'JDK_VERSIONS', value: sdk_version),
            string(name: 'SUFFIX', value: "_${spec_id['id']}"),
            string(name: 'ARCH_OS_LIST', value: spec_id['spec']),
            string(name: 'JDK_IMPL', value: SDK_IMPL),
            string(name: 'ARTIFACTORY_SERVER', value: ARTIFACTORY_SERVER),
            string(name: 'ARTIFACTORY_REPO', value: ARTIFACTORY_REPO),
            string(name: 'BUILDS_TO_KEEP', value: DISCARDER_NUM_BUILDS),
            booleanParam(name: 'AUTO_DETECT', value: auto_detect)
        ]
        build job: 'Test_Job_Auto_Gen', parameters: parameters, propagate: false
    }
}

/*
 * Use curly brackets to wrap the parameter, command.
 * Use a semicolon to separate each command if there are mutiple commands.
 * The parameters, numRetries and waitTime, are integers.
 * The parameter, units, is a string.
 * The parameter, units, can be 'NANOSECONDS', 'MICROSECONDS', 'MILLISECONDS',
 * 'SECONDS', 'MINUTES', 'HOURS', 'DAYS'
 * Example:
 * retry_and_delay({println "This is an example"})
 * retry_and_delay({println "This is an example"}, 4)
 * retry_and_delay({println "This is an example"}, 4, 120)
 * retry_and_delay({println "This is an example"}, 4, 3, 'MINUTES')
 * retry_and_delay({println "This is an example"; println "Another example"})
 */
def retry_and_delay(command, numRetries = 3, waitTime = 60, units = 'SECONDS') {
    def ret = false
    retry(numRetries) {
        if (ret) {
            sleep time: waitTime, unit: units
        } else {
            ret = true
        }
        command()
    }
}

def setup_pull_request() {
    // Parse Github trigger comments
    // For example:
    /* This is great
       Jenkins test extended <platform>
       It was tested*/

    def parsedByNewlineComment = ghprbCommentBody.split(/\\r?\\n/)
    for (comment in parsedByNewlineComment) {
        def lowerCaseComment = comment.toLowerCase().tokenize(' ')
        if (("${lowerCaseComment[0]}" == "jenkins") && (["compile", "test"].contains(lowerCaseComment[1]))) {
            setup_pull_request_single_comment(lowerCaseComment)
            return
        }
    }
    error("Invalid trigger comment")
}

def setup_pull_request_single_comment(parsedComment) {
    // Parse Github trigger comment
    // Jenkins test sanity <platform>*
    // Jenkins test extended <platform>*
    // Jenkins compile <platform>*
    // Jenkins test <comma_separated_test_target> <platform> *
    /*
     * Comment indexes - '+' is an offset of 0/1 if running tests or not
     * 0 - Jenkins
     * 1 - test / compile
     * 1+ - test target
     * 2+ - platforms
     * 3+ - jdk versions
     *
     * Note: Depends logic is already part of the build/compile job and is located in the checkout_pullrequest() function.
     */
    // Don't both checking parsedComment[0] since it is hardcoded in the trigger regex of the Jenkins job.

    // Setup TESTS_TARGETS
    def offset
    switch ("${parsedComment[1]}") {
        case "compile":
            if (parsedComment.size() < 4) {
                error("Pull Request trigger comment needs to specify PLATFORM and SDK_VERSION")
            }
            TESTS_TARGETS = 'none'
            offset = 0
            break
        case "test":
            if (parsedComment.size() < 5) {
                error("Pull Request trigger comment needs to specify TESTS_TARGET, PLATFORM and SDK_VERSION")
            }
            TESTS_TARGETS = "${parsedComment[2]}"
            if ("${TESTS_TARGETS}" == "all") {
                error("Test Target 'all' not allowed for Pull Request builds")
            }
            if ("${TESTS_TARGETS}" == "compile") {
                TESTS_TARGETS = 'none'
            }
            offset = 1
            break
        default:
            error("Unable to parse trigger comment. Unknown build option:'${parsedComment[1]}'")
    }
    echo "TESTS_TARGETS:'${TESTS_TARGETS}'"

    def minCommentSize = 1
    // Setup JDK VERSIONS
    switch (ghprbGhRepository) {
        case ~/.*openj9-openjdk-jdk.*/:
            // <org>/openj9-openjdk-jdk<version>(-zos)?
            def tmp_version = ghprbGhRepository.substring(ghprbGhRepository.indexOf('-jdk')+4)
            if ("${tmp_version}" == "") {
                tmp_version = 'next'
            }
            if (tmp_version.contains('-')) {
                // Strip off '-zos'
                tmp_version = tmp_version.substring(0, tmp_version.indexOf('-'))
            }
            RELEASES.add(tmp_version)
            minCommentSize = 3
            break
        case ~/.*\/openj9(-omr)?/:
            def tmpVersions = parsedComment[3+offset].tokenize(',')
            tmpVersions.each { version ->
                echo "VERSION:'${version}'"
                if ("${version}" == "all") {
                    error("JDK version 'all' not allowed for Pull Request builds\nExpected 'jdkX' where X is one of ${CURRENT_RELEASES}")
                }
                if (!("${version}" ==~ /jdk.*/)) {
                    error("Incorrect syntax for requested JDK version:'${version}'\nExpected 'jdkX' where X is one of ${CURRENT_RELEASES}")
                }
                if (CURRENT_RELEASES.contains(version.substring(3))) {
                    RELEASES.add(version.substring(3))
                } else {
                    error("Unsupported JDK version or incorrect syntax:'${version}'\nExpected 'jdkX' where X is one of ${CURRENT_RELEASES}")
                }
            }
            minCommentSize = 4
            break
        default:
            error("Unknown source repo:'${ghprbGhRepository}'")
    }

    echo "RELEASES:'${RELEASES}'"

    if (parsedComment.size() < minCommentSize) { error("Pull Request trigger comment does not contain enough elements.")}

    // Setup PLATFORMS
    def parsedPlatforms = parsedComment[2+offset].tokenize(',')
    parsedPlatforms.each { platformName ->
        if (platformName in SPECS.keySet()) {
            PLATFORMS.add(platformName)
        } else {
            def longPlatformName = SHORT_NAMES["${platformName}"]
            if (longPlatformName) {
                PLATFORMS.addAll(longPlatformName)
            } else {
                def namesList = (SPECS.keySet() + SHORT_NAMES.keySet()).toSorted()
                error("Unknown PLATFORM:'${platformName}'\nExpected one of: ${namesList}")
            }
        }
    }
    echo "PLATFORMS:'${PLATFORMS}'"

    PERSONAL_BUILD = 'true'

    /*
     * Override ghprcomment with single line comment. Since checkout_pullrequest
     * in build.groovy is expecting a single line comment.
     */
     ghprbCommentBody = parsedComment.join(' ')
}

def move_spec_suffix_to_id(spec, id) {
    /*
     * Move spec suffixes (eg. cmake/jit) from SPEC name to BUILD_IDENTIFIER
     * Since cmake and JITServer is a method for how the SDK is built,
     * from a test perspective it is the same sdk. Therefore the test jobs use
     * the same Jenkinsfiles. In order to keep the test jobs separated, we will
     * move the suffix from the spec to the identifier since test auto-gen allows
     * a build suffix for identification purposes.
     */
    def spec_id = [:]
    spec_id['spec'] = spec
    spec_id['id'] = id
    for (suffix in ['cm', 'jit', 'valhalla', 'uma']) {
        if (spec.contains("_${suffix}")) {
            spec_id['spec'] = spec - "_${suffix}"
            spec_id['id'] = "${suffix}_" + id
        }
    }
    return spec_id
}

def build_all() {
    try {
        if (params.AUTOMATIC_GENERATION != 'false') {
            node(SETUP_LABEL) {
                unstash 'DSL'
                variableFile.create_job(BUILD_NAME, SDK_VERSION, SPEC, 'build', buildFile.convert_build_identifier(BUILD_IDENTIFIER))
            }
        }
        jobs = buildFile.workflow(SDK_VERSION, SPEC, SHAS, OPENJDK_REPO, OPENJDK_BRANCH, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, TESTS_TARGETS, VENDOR_TEST_REPOS_MAP, VENDOR_TEST_BRANCHES_MAP, VENDOR_TEST_DIRS_MAP, USER_CREDENTIALS_ID, SETUP_LABEL, ghprbGhRepository, ghprbActualCommit, EXTRA_GETSOURCE_OPTIONS, EXTRA_CONFIGURE_OPTIONS, EXTRA_MAKE_OPTIONS, OPENJDK_CLONE_DIR, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH, BUILD_NAME, CUSTOM_DESCRIPTION, ARCHIVE_JAVADOC)
    } finally {
        //display the build status of the downstream jobs
        def downstreamBuilds = get_downstream_builds(currentBuild, currentBuild.projectName, get_downstream_job_names(SPEC, SDK_VERSION, BUILD_IDENTIFIER).values())
        add_badges(downstreamBuilds)
        add_summary_badge(downstreamBuilds)
    }
}

/*
 * Adds status badges for the given downstream build.
 */
def add_badges(downstreamBuilds) {
    downstreamBuilds.entrySet().each { entry ->
        // entry.value is a list of builds in descending order
        // fetch the latest build
        def build = entry.value.get(0)

        switch (build.getResult()) {
            case "SUCCESS":
                icon = "/images/16x16/accept.png"
                break
            case "UNSTABLE":
                icon = "/images/16x16/yellow.png"
                break
            case "FAILURE":
                icon = "/images/16x16/error.png"
                break
            case "ABORTED":
                icon = "/images/16x16/aborted.png"
                break
            default:
                icon = "/images/16x16/grey.png"
        }

        addBadge icon: "${icon}",
                 id: "",
                 link: "${JENKINS_URL}${build.getUrl()}",
                 text: "${entry.key} - #${build.getNumber()}"
    }
}

/*
 * Adds a summary badge.
 * Requires Groovy Postbuild plugin.
 */
def add_summary_badge(downstreamBuilds) {
    def summaryText = "Downstream Jobs Status:<br/>"
    summaryText += "<table>"

    downstreamBuilds.entrySet().each { entry ->
        // entry.value is a list of builds in descending order
        // fetch latest build
        def build = entry.value.get(0)
        def buildLink = buildFile.get_build_embedded_status_link(build)
        Job job = build.getParent()
        def blueLink = "<a href=\"${JENKINS_URL}blue/organizations/jenkins/${job.getFullName()}/detail/${entry.key}/${build.getNumber()}/pipeline\">${entry.key}</a>"
        summaryText += "<tr><td>${blueLink}</td><td>${buildLink}</td></tr>"
    }

    summaryText += "</table>"

    // add summary badge
    manager.createSummary("plugin.png").appendText(summaryText)
}

return this
