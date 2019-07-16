/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

ARGS = ['SDK_VERSION', 'PLATFORM']

SETUP_LABEL = params.SETUP_LABEL
if (!SETUP_LABEL) {
    SETUP_LABEL = 'worker'
}
def BUILD_NAME = 'Build-JDK'

timestamps {
    node(SETUP_LABEL) {
        try{
            currentBuild.description = "<a href=\"${RUN_DISPLAY_URL}\">Blue Ocean</a>"
            checkout scm
            variableFile = load 'buildenv/jenkins/common/variables-functions.groovy'
            variableFile.set_job_variables('pipeline')

            buildFile = load 'buildenv/jenkins/common/pipeline-functions.groovy'
            SHAS = buildFile.get_shas(OPENJDK_REPO, OPENJDK_BRANCH, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, VENDOR_TEST_REPOS_MAP, VENDOR_TEST_BRANCHES_MAP, VENDOR_TEST_SHAS_MAP)
            BUILD_NAME = buildFile.get_build_job_name(SPEC, SDK_VERSION, BUILD_IDENTIFIER)
            // Stash DSL file so we can quickly load it on master
            if (params.AUTOMATIC_GENERATION != 'false'){
                stash includes: 'buildenv/jenkins/jobs/pipelines/Pipeline_Template.groovy', name: 'DSL'
            }
        } finally {
            // disableDeferredWipeout also requires deleteDirs. See https://issues.jenkins-ci.org/browse/JENKINS-54225
            cleanWs notFailBuild: true, disableDeferredWipeout: true, deleteDirs: true
        }
    }

    try {
        if (params.AUTOMATIC_GENERATION != 'false'){
            node('master') {
                unstash 'DSL'
                variableFile.create_job(BUILD_NAME, SDK_VERSION, SPEC, 'build', buildFile.convert_build_identifier(BUILD_IDENTIFIER))
            }
        }
        jobs = buildFile.workflow(SDK_VERSION, SPEC, SHAS, OPENJDK_REPO, OPENJDK_BRANCH, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, TESTS_TARGETS, VENDOR_TEST_REPOS_MAP, VENDOR_TEST_BRANCHES_MAP, VENDOR_TEST_DIRS_MAP, USER_CREDENTIALS_ID, SETUP_LABEL, ghprbGhRepository, ghprbActualCommit, EXTRA_GETSOURCE_OPTIONS, EXTRA_CONFIGURE_OPTIONS, EXTRA_MAKE_OPTIONS, OPENJDK_CLONE_DIR, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH, BUILD_NAME, CUSTOM_DESCRIPTION)
    } finally {
        //display the build status of the downstream jobs
        def downstreamBuilds = buildFile.get_downstream_builds(currentBuild, currentBuild.projectName, buildFile.get_downstream_job_names(SPEC, SDK_VERSION, BUILD_IDENTIFIER).values())
        add_badges(downstreamBuilds)
        add_summary_badge(downstreamBuilds)
    }
}

/*
* Adds status badges for the given downstream build.
*/
def add_badges(downstreamBuilds) {
    downstreamBuilds.entrySet().each { entry ->
        def build = entry.value

        switch(build.getResult()){
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
        def buildLink = buildFile.get_build_embedded_status_link(entry.value)
        Job job = entry.value.getParent()
        def blueLink = "<a href=\"${JENKINS_URL}blue/organizations/jenkins/${job.getFullName()}/detail/${entry.key}/${entry.value.getNumber()}/pipeline\">${entry.key}</a>"
        summaryText += "<tr><td>${blueLink}</td><td>${buildLink}</td></tr>"
    }

    summaryText += "</table>"

    // add summary badge
    manager.createSummary("plugin.png").appendText(summaryText)
}
