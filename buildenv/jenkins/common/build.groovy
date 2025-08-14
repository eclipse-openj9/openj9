/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
import groovy.json.JsonSlurper;
import groovy.json.JsonOutput;
import java.util.regex.Matcher
import java.util.regex.Pattern

pipelineFunctions = load 'buildenv/jenkins/common/pipeline-functions.groovy'

def get_source() {
    stage('Get Source') {
        // Setup REPO variables
        OPENJ9_REPO_OPTION = (OPENJ9_REPO != "") ? "-openj9-repo=${OPENJ9_REPO}" : ""
        OPENJ9_BRANCH_OPTION = (OPENJ9_BRANCH != "") ? "-openj9-branch=${OPENJ9_BRANCH}" : ""
        OPENJ9_SHA_OPTION = (OPENJ9_SHA != "") ? "-openj9-sha=${OPENJ9_SHA}" : ""

        OMR_REPO_OPTION = (OMR_REPO != "") ? "-omr-repo=${OMR_REPO}" : ""
        OMR_BRANCH_OPTION = (OMR_BRANCH != "")? "-omr-branch=${OMR_BRANCH}" : ""
        OMR_SHA_OPTION = (OMR_SHA != "") ? "-omr-sha=${OMR_SHA}" : ""

        VENDOR_CODE_REPO_OPTION = (VENDOR_CODE_REPO != "") ? "-vendor-repo=${VENDOR_CODE_REPO}" : ""
        VENDOR_CODE_BRANCH_OPTION = (VENDOR_CODE_BRANCH != "") ? "-vendor-branch=${VENDOR_CODE_BRANCH}" : ""
        VENDOR_CODE_SHA_OPTION = (VENDOR_CODE_SHA != "") ? "-vendor-sha=${VENDOR_CODE_SHA}" : ""

        if (OPENJDK_REFERENCE_REPO) {
            OPENJ9_REFERENCE = "-openj9-reference=${OPENJDK_REFERENCE_REPO}"
            OMR_REFERENCE = "-omr-reference=${OPENJDK_REFERENCE_REPO}"
            if (SDK_VERSION == "8" || SPEC.contains('zos')) {
                VENDOR_CODE_REFERENCE = "-vendor-reference=${OPENJDK_REFERENCE_REPO}"
            } else {
                VENDOR_CODE_REFERENCE = ""
            }
        }

        // use sshagent with Jenkins credentials ID for all platforms except zOS
        // on zOS use the user's ssh key
        if (!SPEC.contains('zos') && (USER_CREDENTIALS_ID != '')) {
            get_sources_with_authentication()
        } else {
            get_sources()
        }
    }
}

def get_sources_with_authentication() {
    sshagent(credentials:["${USER_CREDENTIALS_ID}"]) {
        get_sources()
    }
}

def get_source_call(gskit_cred="") {
    sh "bash get_source.sh ${EXTRA_GETSOURCE_OPTIONS} ${gskit_cred} ${OPENJ9_REPO_OPTION} ${OPENJ9_BRANCH_OPTION} ${OPENJ9_SHA_OPTION} ${OPENJ9_REFERENCE} ${OMR_REPO_OPTION} ${OMR_BRANCH_OPTION} ${OMR_SHA_OPTION} ${OMR_REFERENCE} ${VENDOR_CODE_REPO_OPTION} ${VENDOR_CODE_BRANCH_OPTION} ${VENDOR_CODE_SHA_OPTION} ${VENDOR_CODE_REFERENCE}"
}

def get_source_call_optional_gskit() {
    if (EXTRA_GETSOURCE_OPTIONS.contains("-gskit-bin") || (EXTRA_CONFIGURE_OPTIONS.contains("-gskit-sdk-bin"))) {
        gskitCredentialID = variableFile.get_user_credentials_id('gskit')
        withCredentials([usernamePassword(credentialsId: "${gskitCredentialID}", passwordVariable: 'GSKIT_PASSWORD', usernameVariable: 'GSKIT_USERNAME')]) {
            get_source_call("-gskit-credential=\$GSKIT_USERNAME:\$GSKIT_PASSWORD")
        }
    } else {
        get_source_call()
    }
}

def get_sources() {
    // Temp workaround for Windows clones
    // See #3633 and JENKINS-54612
    if (SPEC.contains("win") || SPEC.contains("zos")) {

        CLONE_CMD = "git clone -b ${OPENJDK_BRANCH} ${OPENJDK_REPO} ."
        if (OPENJDK_REFERENCE_REPO) {
            CLONE_CMD += " --reference ${OPENJDK_REFERENCE_REPO}"
        }

        if (USER_CREDENTIALS_ID && SPEC.contains("win")) {
            sshagent(credentials:["${USER_CREDENTIALS_ID}"]) {
                sh "${CLONE_CMD}"
            }
        } else {
            sh "${CLONE_CMD}"
        }
    } else {
        def remote_config_parameters = [url: "${OPENJDK_REPO}"]
        if (USER_CREDENTIALS_ID) {
            remote_config_parameters.put('credentialsId', "${USER_CREDENTIALS_ID}")
        }

        def openjdkCheckoutRef = OPENJDK_BRANCH
        if (OPENJDK_SHA) {
            openjdkCheckoutRef = OPENJDK_SHA
        }
        checkout changelog: false,
                poll: false,
                scm: [$class: 'GitSCM',
                    branches: [[name: "${openjdkCheckoutRef}"]],
                    doGenerateSubmoduleConfigurations: false,
                    extensions: [[$class: 'CheckoutOption', timeout: 30],
                                [$class: 'CloneOption',
                                    depth: 0,
                                    noTags: false,
                                    reference: "${OPENJDK_REFERENCE_REPO}",
                                    shallow: false,
                                    timeout: 30]],
                    submoduleCfg: [],
                    userRemoteConfigs: [remote_config_parameters]]
    }

    // Check if this build is a PR
    // Only PR builds (old and new) pass ghprbPullId
    if (params.ghprbPullId) {
        // Look for dependent changes and checkout PR(s)
        checkout_pullrequest()
    } else {
        get_source_call_optional_gskit()
    }
}

def checkout_pullrequest() {
    echo 'Look for Dependent Changes'

    def KEYWORD = "depends"
    def COMMENT = params.ghprbCommentBody.toLowerCase(Locale.US);
    def OPENJDK_PR
    def OMR_PR
    def OPENJ9_PR
    def omr_bool = false
    def omr_upstream = false
    def openjdk_bool = false
    def openj9_bool = false
    def JDK_REPO_SUFFIX = ""

    // Determine source repo of PR
    // Cannot depend on a change from the same repo as the source repo
    def SOURCE_REPO_MESSAGE
    switch (params.ghprbGhRepository) {
        case ~/.*\/openj9-omr/:
            omr_bool = true
            OMR_PR = params.ghprbPullId
            SOURCE_REPO_MESSAGE = "PullRequest source repo appears to be OpenJ9-OMR"
            break
        case ~/.*\/openj9/:
            openj9_bool = true
            OPENJ9_PR = params.ghprbPullId
            SOURCE_REPO_MESSAGE = "PullRequest source repo appears to be OpenJ9"
            break
        case ~/.*openj9-openjdk-jdk.*/:
            openjdk_bool = true
            OPENJDK_PR = params.ghprbPullId
            SOURCE_REPO_MESSAGE = "PullRequest source repo appears to be OpenJDK"
            break
        default:
            error "Unable to determine source repo for PullRequest"
    }
    echo SOURCE_REPO_MESSAGE

    if (!COMMENT.contains(KEYWORD)) {
        echo "No dependent changes found"
    } else {
        // Gather everything after KEYWORD. Assumes a whitespace after KEYWORD, hence +1
        def DEPENDS_BLOB = COMMENT.substring(COMMENT.indexOf(KEYWORD) + KEYWORD.length() + 1 , COMMENT.length())
        def DEPENDS_ARRAY = DEPENDS_BLOB.tokenize(' ')

        // Setup PR_IDs for dependent changes
        for (DEPEND in DEPENDS_ARRAY) {
            String REPO = ''
            String PR_ID = ''
            switch (DEPEND) {
                case ~/https:\/\/github\.(ibm\.)?com\/[^\s\/]+\/[^\s\/]+\/pull\/[0-9]+/:
                    def urlList = DEPEND.tokenize('/')
                    REPO = urlList[3]
                    PR_ID = urlList[5]
                    break
                case ~/[^\s\/#]+\/[^\s\/#]+#[^\s\/#]+/:
                    REPO = DEPEND.substring(DEPEND.indexOf("/") + 1, DEPEND.indexOf("#"));
                    PR_ID = DEPEND.substring(DEPEND.indexOf("#") + 1, DEPEND.length());
                    break
                default:
                    error "Cannot parse dependent change: '${DEPEND}'"
            }
            switch (REPO) {
                case "omr":
                case "openj9-omr":
                    if (omr_bool) {
                        error "Cannot specify 2 OMR Repos"
                    }
                    omr_bool = true
                    if (REPO == 'omr') {
                        omr_upstream = true
                    }
                    OMR_PR = "${PR_ID}"
                    echo "Dependent OMR change found: #${PR_ID}"
                    break
                case "openj9":
                    if (openj9_bool) {
                        error "Cannot specify 2 OpenJ9 Repos"
                    }
                    openj9_bool = true
                    OPENJ9_PR = "${PR_ID}"
                    echo "Dependent OpenJ9 change found: #${PR_ID}"
                    break
                case ~/openj9-openjdk(.*)/:
                    if (openjdk_bool) {
                        error "Cannot specify 2 OpenJDK Repos"
                    }
                    openjdk_bool = true
                    OPENJDK_PR = "${PR_ID}"
                    echo "Dependent OpenJDK change found: #${PR_ID}"

                    // Verify OpenJDK Repo matches SDK Version of build.
                    // eg. Cannot checkout a JDK8 PR for a JDK9 build
                    echo "SDK: ${SDK_VERSION}"
                    echo "REPO: ${REPO}"
                    if (SDK_VERSION != "next") {
                        JDK_REPO_SUFFIX = SDK_VERSION
                    }
                    // For SDK 8/9/10, the Repo has the SDK version in the name
                    if (REPO != "openj9-openjdk-jdk${JDK_REPO_SUFFIX}") {
                        error "Trying to build SDK${JDK_REPO_SUFFIX} with a dependent change from repo '${REPO}' is not possible"
                    }
                    break
                default:
                    error "Bad Depends Repo: '${REPO}' in '${DEPEND}'"
            }
        }
    }

    if (openjdk_bool) {
        if (SDK_VERSION != "next") {
            JDK_REPO_SUFFIX = SDK_VERSION
        }
        checkout_pullrequest(OPENJDK_PR, "ibmruntimes/openj9-openjdk-jdk${JDK_REPO_SUFFIX}")
    }

    get_source_call_optional_gskit()

    // Checkout dependent PRs, if any were specified
    if (openj9_bool) {
        dir ('openj9') {
            checkout_pullrequest(OPENJ9_PR, 'eclipse-openj9/openj9')
        }
    }

    if (omr_bool) {
        dir ('omr') {
            if (omr_upstream) {
                sh "git config remote.origin.url https://github.com/eclipse-omr/omr.git"
            }
            checkout_pullrequest(OMR_PR, 'eclipse-omr/omr')
        }
    }
}

def checkout_pullrequest(ID, REPO) {
    // Determine if we are checking out a PR or a branch
    try {
        if (ID.isNumber()) {
            try {
                fetchRef("pull/${ID}/merge", "pr/${ID}/merge")
                checkoutRef ("pr/${ID}/merge")
            } catch (e) {
                // If merge ref doesn't exist, checkout PR base branch.
                // Not going to fix this to work for internal PRs.
                def JSONFILE = ""
                def BASEREF = ""
                def URL = "https://api.github.com/repos/${REPO}/pulls/${ID}"
                withCredentials([usernamePassword(credentialsId: "${GITHUB_API_CREDENTIALS_ID}", passwordVariable: 'PASS', usernameVariable: 'USER')]) {
                    JSONFILE = sh returnStdout: true, script: "curl -u ${USER} --pass ${PASS} ${URL}"
                }
                def slurper = new groovy.json.JsonSlurper()
                def jsonResultFile = slurper.parseText(JSONFILE)
                BASEREF = jsonResultFile.base.ref
                fetchRef ("heads/${BASEREF}", "${BASEREF}")
                checkoutRef (BASEREF)
            }
        } else {
            fetchRef ("heads/${ID}", ID)
            checkoutRef (ID)
        }
    } catch (e) {
        error("Fatal: Unable to checkout '${ID}'. If checking out a branch, please ensure the branch exists and the spelling is correct. If checking out a PullRequest, please ensure the PR is open, unmerged and has no merge conflicts.")
    }
}

def fetchRef (REF1, REF2) {
    sh "git fetch --progress origin +refs/${REF1}:refs/remotes/origin/${REF2}"
}

def checkoutRef (REF) {
    sh "git checkout refs/remotes/origin/${REF}"
}

def build() {
    stage('Compile') {
        def freemarker_option = FREEMARKER ? "--with-freemarker-jar=${FREEMARKER}" : ""
        OPENJDK_CLONE_DIR = "${env.WORKSPACE}/${OPENJDK_CLONE_DIR}"

        withEnv(BUILD_ENV_VARS_LIST) {
            dir(OPENJDK_CLONE_DIR) {
                try {
                    sh "${BUILD_ENV_CMD} bash configure ${freemarker_option} --with-boot-jdk=${BOOT_JDK} ${EXTRA_CONFIGURE_OPTIONS} && ${get_compile_command()}"
                } catch (Exception e) {
                    // last 5000 console output lines
                    LOG_MAX_LINES = params.LOG_MAX_LINES ? params.LOG_MAX_LINES.toInteger() : 5000
                    LOG_LINES = currentBuild.getRawBuild().getLog(LOG_MAX_LINES)
                    if (match_fail_pattern(LOG_LINES)) {
                        recompile()
                    } else {
                        archive_diagnostics()
                        throw e
                    }
                }
            }
        }
    }
    stage('Java Version') {
        dir(OPENJDK_CLONE_DIR) {
            def jdkImageDir = "build/${RELEASE}/images/${JDK_FOLDER}"
            try {
                sh "${jdkImageDir}/bin/java -Xjit -version"
            } catch (e) {
                archive_diagnostics(jdkImageDir)
                throw e
            }
        }
    }
}

def get_compile_command() {
    return "make ${EXTRA_MAKE_OPTIONS} all"
}

def archive_sdk() {
    stage('Archive') {
        def buildDir = "build/${RELEASE}/images/"
        def debugImageDir = "${buildDir}debug-image/"
        def testDir = "test"
        def makeDir = "make"

        dir(OPENJDK_CLONE_DIR) {
            // Code coverage files archive for report
            if (params.CODE_COVERAGE) {
                // Use WORKSPACE since code coverage gcno files are in vm folder, java is in jdk and images folder, and omr and openj9 folders are in workspace.
                def codeCoverageDir = "${env.WORKSPACE}"
                sh "touch ${CODE_COVERAGE_FILENAME}"
                if (SPEC.contains('zos')) {
                    sh "pax --exclude=${CODE_COVERAGE_FILENAME} -wvzf ${CODE_COVERAGE_FILENAME} ${codeCoverageDir}"
                } else {
                    sh "tar --exclude=${CODE_COVERAGE_FILENAME} -zcvf ${CODE_COVERAGE_FILENAME} ${codeCoverageDir}"
                }
            }

            // The archiver receives pathnames on stdin and writes to stdout.
            def archiveCmd = SPEC.contains('zos') ? 'pax -wvz -p x' : 'tar -cvz -T -'
            // Filter out unwanted files (most of which are available in the debug-image).
            def filterCmd = "sed -e '/\\.dbg\$/d' -e '/\\.debuginfo\$/d' -e '/\\.diz\$/d' -e '/\\.dSYM\\//d' -e '/\\.map\$/d' -e '/\\.pdb\$/d'"
            sh "( cd ${buildDir} && find ${JDK_FOLDER} '(' -type f -o -type l ')' | ${filterCmd} | ${archiveCmd} ) > ${SDK_FILENAME}"
            // test if the test natives directory is present, only in JDK11+
            if (fileExists("${buildDir}${testDir}")) {
                if (SPEC.contains('zos')) {
                    // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
                    sh "pax -wvz '-s#${buildDir}${testDir}#test-images#' -f ${TEST_FILENAME} ${buildDir}${testDir}"
                } else {
                    sh "tar -C ${buildDir} -zcvf ${TEST_FILENAME} ${testDir} --transform 's,${testDir},test-images,'"
                }
            }
            // test if the debug-image directory is present
            if (fileExists("${debugImageDir}")) {
                if (SPEC.contains('aix')) {
                    // On AIX, .debuginfo files are simple copies of the shared libraries.
                    // Change all suffixes from .debuginfo to .so; then remove the .so suffix
                    // from names in the bin directory. This will result in an archive that
                    // can be extracted overtop of a jdk yielding one that is debuggable.
                    sh "tar -C ${debugImageDir} -zcvf ${DEBUG_IMAGE_FILENAME} . --transform 's#\\.debuginfo\$#.so#' --transform 's#\\(bin/.*\\)\\.so\$#\\1#' --show-stored-names"
                } else if (SPEC.contains('zos')) {
                    // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
                    sh "pax -wvz '-s#${debugImageDir}##' -f ${DEBUG_IMAGE_FILENAME} ${debugImageDir}"
                } else {
                    sh "tar -C ${debugImageDir} -zcvf ${DEBUG_IMAGE_FILENAME} ."
                }
            }
            if (params.ARCHIVE_JAVADOC) {
                def javadocDir = "docs"
                def javadocOpenJ9OnlyDir = "openj9-docs"
                def extractDir = "build/${RELEASE}/images/"
                if (SDK_VERSION == "8") {
                    extractDir = "build/${RELEASE}/"
                }
                if (fileExists("${extractDir}${javadocDir}")) {
                    if (SPEC.contains('zos')) {
                        // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
                        sh "pax -wvzf ${JAVADOC_FILENAME} ${extractDir}${javadocDir}"
                    } else {
                        sh "tar -C ${extractDir} -zcvf ${JAVADOC_FILENAME} ${javadocDir}"
                    }
                }
                if (fileExists("${extractDir}${javadocOpenJ9OnlyDir}")) {
                    if (SPEC.contains('zos')) {
                        // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
                        sh "pax -wvzf ${JAVADOC_OPENJ9_ONLY_FILENAME} ${extractDir}${javadocOpenJ9OnlyDir}"
                    } else {
                        sh "tar -C ${extractDir} -zcvf ${JAVADOC_OPENJ9_ONLY_FILENAME} ${javadocOpenJ9OnlyDir}"
                    }
                }
            }
            if (ARTIFACTORY_CONFIG) {
                def specs = []
                def sdkSpec = ["pattern": "${OPENJDK_CLONE_DIR}/${SDK_FILENAME}",
                               "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                               "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                specs.add(sdkSpec)
                def testSpec = ["pattern": "${OPENJDK_CLONE_DIR}/${TEST_FILENAME}",
                                "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                                "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                specs.add(testSpec)
                def debugImageSpec = ["pattern": "${OPENJDK_CLONE_DIR}/${DEBUG_IMAGE_FILENAME}",
                                "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                                "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                specs.add(debugImageSpec)
                // Extract obfuscation change log files to Artifactory
                def findZelixOut = sh(script: "find ${makeDir} -name 'ObfuscateLog*.txt' -type f", returnStdout: true).trim()
                def zelixFiles = findZelixOut.tokenize('\n');
                for (String zelixFile in zelixFiles) {
                    def zelixSpec = ["pattern": "${zelixFile}",
                                     "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                                     "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                    specs.add(zelixSpec)
                }
                if (params.ARCHIVE_JAVADOC) {
                    def javadocSpec = ["pattern": "${OPENJDK_CLONE_DIR}/${JAVADOC_FILENAME}",
                                       "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                                       "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                    specs.add(javadocSpec)
                    def javadocOpenJ9OnlySpec = ["pattern": "${OPENJDK_CLONE_DIR}/${JAVADOC_OPENJ9_ONLY_FILENAME}",
                                                 "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                                                 "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                    specs.add(javadocOpenJ9OnlySpec)
                }
                if (params.CODE_COVERAGE) {
                    def codeCoverageSpec = ["pattern": "${OPENJDK_CLONE_DIR}/${CODE_COVERAGE_FILENAME}",
                                       "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                                       "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                    specs.add(codeCoverageSpec)
                }

                def uploadFiles = [files : specs]
                def uploadSpec = JsonOutput.toJson(uploadFiles)
                upload_artifactory(uploadSpec)

                // Always use the default server link for the description and for test.
                env.CUSTOMIZED_SDK_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${SDK_FILENAME}"
                currentBuild.description += "<br><a href=${CUSTOMIZED_SDK_URL}>${SDK_FILENAME}</a>"

                for (String zelixFile in zelixFiles) {
                    def zelixFileStripped = zelixFile.substring(zelixFile.lastIndexOf("/") + 1)
                    ZELIX_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${zelixFileStripped}"
                    currentBuild.description += "<br><a href=${ZELIX_URL}>${zelixFileStripped}</a>"
                }
                if (fileExists("${TEST_FILENAME}")) {
                    TEST_LIB_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${TEST_FILENAME}"
                    currentBuild.description += "<br><a href=${TEST_LIB_URL}>${TEST_FILENAME}</a>"
                    env.CUSTOMIZED_SDK_URL += " " + TEST_LIB_URL
                }
                if (fileExists("${DEBUG_IMAGE_FILENAME}")) {
                    DEBUG_IMAGE_LIB_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${DEBUG_IMAGE_FILENAME}"
                    currentBuild.description += "<br><a href=${DEBUG_IMAGE_LIB_URL}>${DEBUG_IMAGE_FILENAME}</a>"
                }
                if (params.ARCHIVE_JAVADOC) {
                    if (fileExists("${JAVADOC_FILENAME}")) {
                        JAVADOC_LIB_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${JAVADOC_FILENAME}"
                        currentBuild.description += "<br><a href=${JAVADOC_LIB_URL}>${JAVADOC_FILENAME}</a>"
                        echo "Javadoc:'${JAVADOC_LIB_URL}'"
                    }
                    if (fileExists("${JAVADOC_OPENJ9_ONLY_FILENAME}")) {
                        JAVADOC_OPENJ9_ONLY_LIB_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${JAVADOC_OPENJ9_ONLY_FILENAME}"
                        currentBuild.description += "<br><a href=${JAVADOC_OPENJ9_ONLY_LIB_URL}>${JAVADOC_OPENJ9_ONLY_FILENAME}</a>"
                        echo "Javadoc (OpenJ9 extensions only):'${JAVADOC_OPENJ9_ONLY_LIB_URL}'"
                    }
                }
                if (params.CODE_COVERAGE) {
                    if (fileExists("${CODE_COVERAGE_FILENAME}")) {
                        CODE_COVERAGE_LIB_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${CODE_COVERAGE_FILENAME}"
                        currentBuild.description += "<br><a href='${CODE_COVERAGE_LIB_URL}'>${CODE_COVERAGE_FILENAME}</a>"
                        env.CUSTOMIZED_SDK_URL += " " + CODE_COVERAGE_LIB_URL
                        echo "Code Coverage:'${CODE_COVERAGE_LIB_URL}'"
                    }
                }
                echo "CUSTOMIZED_SDK_URL:'${CUSTOMIZED_SDK_URL}'"
            } else {
                echo "ARTIFACTORY server is not set saving artifacts on jenkins."
                def ARTIFACTS_FILES = "**/${SDK_FILENAME},**/${TEST_FILENAME},**/${DEBUG_IMAGE_FILENAME}"
                if (params.ARCHIVE_JAVADOC) {
                    ARTIFACTS_FILES += ",**/${JAVADOC_FILENAME}"
                    ARTIFACTS_FILES += ",**/${JAVADOC_OPENJ9_ONLY_FILENAME}"
                }
                if (params.CODE_COVERAGE) {
                    ARTIFACTS_FILES += ",**/${CODE_COVERAGE_FILENAME}"
                }
                archiveArtifacts artifacts: ARTIFACTS_FILES, fingerprint: false, onlyIfSuccessful: true
            }
        }
    }
}

/*
 * When crash occurs in java, there will be an output in the log: IEATDUMP success for DSN='JENKINS.JVM.JENKIN*.*.*.X&DS'.
 * The X&DS is a macro that is passed to the IEATDUMP service on z/OS.
 * The dump is split into multiple files by the macro if the address space is large.
 * Each dataset is named in a similar way but there is a distinct suffix to specify the order.
 * For example, JVM.*.*.*.001, JVM.*.*.*.002, and JVM.*.*.*.003.
 * The function fetchFile moves each dataset into the current directory of the machine, and appends the data to the core.xxx.dmp at the same time.
 * The file core.xxx.dmp is uploaded to artifactory for triage.
 */
def fetchFile(src, dest) {
    // remove the user name from the dataset name
    def offset = USER.length() + 1
    def fileToMove = src.substring(src.indexOf("${USER}.") + offset)
    // append the dataset to core.xxx.dmp file
    sh "cat '//${fileToMove}' >> ${dest}"
    // remove the dataset
    sh "tso DELETE ${fileToMove}"
}

def archive_diagnostics(javaVersionJdkImageDir = null) {
    def datestamp = variableFile.get_date()
    def diagnosticsFilename = "${JOB_NAME}-${BUILD_NUMBER}-${datestamp}-diagnostics.tar.gz"

    def archiveCmd = "tar -zcvf ${diagnosticsFilename} -T -"

    def findCrashDataCmd = "find ."
    findCrashDataCmd += " -name 'core.*.dmp'"
    findCrashDataCmd += " -o -name 'javacore.*.txt'"
    findCrashDataCmd += " -o -name 'jitdump.*.dmp'"
    findCrashDataCmd += " -o -name 'Snap.*.trc'"

    if (SPEC.contains('linux')) {
        // collect *.debuginfo files to help diagnose omr_ddrgen failures
        findCrashDataCmd += " -o -name '*.debuginfo'"
    } else if (SPEC.contains('win')) {
        // collect *.pdb files to help diagnose omr_ddrgen failures
        findCrashDataCmd += " -o -name '*.pdb'"
    } else if (SPEC.contains('zos')) {
        def logContent = currentBuild.rawBuild.getLog()
        // search for each occurrence of IEATDUMP success for DSN=
        def matches = logContent =~ /IEATDUMP success for DSN=.*/
        for (match in matches) {
            // extract the naming convention of datasets
            def beginIndex = match.indexOf('\'') + 1
            def endIndex = match.lastIndexOf('.')
            def filename = match.substring(beginIndex, endIndex)
            // count how many datasets created by the macro
            def num = sh returnStdout: true, script: "tso listcat | grep ${filename} | wc -l"
            def numFiles = num.trim().toInteger()
            for (i = 1; i <= numFiles; i++) {
                switch (i) {
                    case 1..9:
                        src = filename + '.' + 'X00' + i;
                        break;
                    case 10..99:
                        src = filename + '.' + 'X0' + i;
                        break;
                    case 100..999:
                        src = filename + '.' + 'X' + i;
                        break;
                }
                dest = 'core' + '.' + filename + '.' + 'dmp'
                fetchFile(src,dest)
            }
        }
        // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
        archiveCmd = "pax -wzf ${diagnosticsFilename}"
    }

    def tgt = "./build/${RELEASE}"

    def findDiagnosticsCmd = findCrashDataCmd
    findDiagnosticsCmd += " -o -type d -name ddr_info -prune"
    findDiagnosticsCmd += " -o -type d -path '${tgt}/make-support/failure-logs' -prune"

    // If the JVM crashed while building, then include the binaries we may have
    // been running and any separate debug info in the diagnostics archive to
    // help debug.
    if (sh(returnStatus: true, script: "${findCrashDataCmd} | grep -q .") == 0) {
        def j2reImage = "${tgt}/images/j2re-image"
        if (sh(returnStatus: true, script: "test -d '${j2reImage}'") == 0) {
            // Java 8. The build does not run anything from jdk/ (and it seems
            // that the binaries there can't be run from that location). But it
            // does run images/j2re-image/bin/java -version, so instead of jdk/
            // get images/j2re-image/.
            findDiagnosticsCmd += " -o -type d -path '${j2reImage}' -prune"
        } else {
            // Java 11+. The build runs binaries from jdk/.
            findDiagnosticsCmd += " -o -type d -path '${tgt}/jdk' -prune"

            // The jdk/ directory may contain relative symlinks pointing into
            // support/modules_libs/ for files required to run the JVM, and
            // also for separate debug info.
            findDiagnosticsCmd += " -o -type d -path '${tgt}/support/modules_libs' -prune"
        }

        if (javaVersionJdkImageDir != null) {
            // Crashed while running $javaVersionJdkImageDir/bin/java -version.
            // Make sure to pick up the binaries from there as well.
            //
            // In Java 8, this should be images/j2sdk-image/. Usually we only
            // get images/j2re-image/.
            //
            // In Java 11+, it should be images/jdk/. Usually we only get jdk/
            // (i.e. outside of images/).
            //
            // Make sure this path starts with ./ so that it's possible for
            // -path to match when running find . [...].
            //
            if (!javaVersionJdkImageDir.startsWith("./")) {
                javaVersionJdkImageDir = "./${javaVersionJdkImageDir}";
            }

            findDiagnosticsCmd += " -o -type d -path '${javaVersionJdkImageDir}' -prune"
        }
    }

    sh "${findDiagnosticsCmd} | sed 's#^./##' | ${archiveCmd}"

    if (ARTIFACTORY_CONFIG) {
        def uploadSpec = """{
            "files":[
                {
                    "pattern": "${diagnosticsFilename}",
                    "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                    "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"
                }
            ]
        }"""
        upload_artifactory_core(ARTIFACTORY_CONFIG['defaultGeo'], uploadSpec)
        DIAGNOSTICS_FILE_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${diagnosticsFilename}"
        currentBuild.description += "<br><a href=${DIAGNOSTICS_FILE_URL}>${diagnosticsFilename}</a>"
    } else {
        archiveArtifacts artifacts: "${diagnosticsFilename}", fingerprint: false
    }
}

def upload_artifactory(uploadSpec) {
    // Loop all the servers and upload if we've determined we should do so
    for (geo in ARTIFACTORY_CONFIG['geos']) {
        if (ARTIFACTORY_CONFIG[geo]['uploadBool']) {
            /*
            * If the server is behind a vpn and we aren't on a node behind the vpn,
            * save the sdk and upload it later on a machine behind the vpn
            */
            if (ARTIFACTORY_CONFIG[geo]['vpn'] == 'true' && !NODE_LABELS.contains("ci.geo.${geo}")) {
                if (!ARTIFACTORY_CONFIG['stashed']) {
                    // Stash only what test needs (CUSTOMIZED_SDK_URL)
                    if (params.CODE_COVERAGE) {
                        stash includes: "**/${SDK_FILENAME},**/${TEST_FILENAME},**/${CODE_COVERAGE_FILENAME}", name: 'sdk'
                    } else {
                        stash includes: "**/${SDK_FILENAME},**/${TEST_FILENAME}", name: 'sdk'
                    }
                    ARTIFACTORY_CONFIG['stashed'] = true
                    ARTIFACTORY_CONFIG['uploadSpec'] = uploadSpec
                }
                ARTIFACTORY_CONFIG[geo]['uploaded'] = false
            } else {
                upload_artifactory_core(geo, uploadSpec)
                ARTIFACTORY_CONFIG[geo]['uploaded'] = true
            }
        }
    }
}

def upload_artifactory_core(geo, uploadSpec) {
    echo "Uploading to '${geo}'..."
    def server = Artifactory.server ARTIFACTORY_CONFIG[geo]['server']
    // set connection timeout to 10 mins to avoid timeout on slow platforms
    server.connection.timeout = 600

    def buildInfo = Artifactory.newBuildInfo()
    buildInfo.retention maxBuilds: ARTIFACTORY_CONFIG[geo]['numArtifacts'], maxDays: ARTIFACTORY_CONFIG[geo]['daysToKeepArtifacts'], deleteBuildArtifacts: true
    // Add BUILD_IDENTIFIER to the buildInfo. The UploadSpec adds it to the Artifact info
    buildInfo.env.filter.addInclude("BUILD_IDENTIFIER")
    buildInfo.env.capture = true
    if (ARTIFACTORY_CONFIG[geo]['buildNamePrefix'] != '') {
        buildInfo.name = ARTIFACTORY_CONFIG[geo]['buildNamePrefix'] + '/' + JOB_NAME
    }
    println "buildInfo.name:$buildInfo.name"

    //Retry uploading to Artifactory if errors occur
    // Do not upload buildInfo if Server is behind a VPN as the Controller will not be able to talk to it.
    pipelineFunctions.retry_and_delay({
        server.upload spec: uploadSpec, buildInfo: buildInfo;
        server.publishBuildInfo buildInfo},
        3, 300)

    ARTIFACTORY_CONFIG[geo]['url'] = server.getUrl()
    ARTIFACTORY_CONFIG[geo]['creds'] = server.getCredentialsId()
    // If this is the default server, save the creds to pass to test
    if (geo == ARTIFACTORY_CONFIG['defaultGeo']) {
        env.ARTIFACTORY_CREDS = ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']].creds
    }
}

def upload_artifactory_post() {
    // Determine if we didn't do any Artifactory uploads because of vpn.
    // At the time of writing this code, we would only hit this case if we compiled at OSU on plinux and needed to upload to UNB.
    if (ARTIFACTORY_CONFIG && ARTIFACTORY_CONFIG['stashed']) {
        for (geo in ARTIFACTORY_CONFIG['geos']) {
            if (ARTIFACTORY_CONFIG[geo]['uploadBool'] && !ARTIFACTORY_CONFIG[geo]['uploaded']) {
                // Grab a node with the same geo as the server so we can upload
                node("ci.geo.${geo}") {
                    try {
                        unstash 'sdk'
                        upload_artifactory_core(geo, ARTIFACTORY_CONFIG['uploadSpec'])
                    } finally {
                        cleanWs()
                    }
                }
            }
        }
    }
}

def add_node_to_description() {
    def TMP_DESC = (currentBuild.description) ? currentBuild.description + "<br>" : ""
    currentBuild.description = TMP_DESC + "<a href=${JENKINS_URL}computer/${NODE_NAME}>${NODE_NAME}</a>"
}

def cleanWorkspace (KEEP_WORKSPACE) {
    if (!KEEP_WORKSPACE) {
        try {
            cleanWs disableDeferredWipeout: true, deleteDirs: true
        } catch (Exception e) {
            try {
                if (SPEC.contains('win')) {
                    WORKSPACE = sh(script: "cygpath -u '${env.WORKSPACE}'", returnStdout: true).trim()
                }
                if (WORKSPACE) {
                    sh "rm -fr ${WORKSPACE}/*"
                }
            } catch(Exception f) {
                throw f
            }
        }
    }
}

def getDockerRegistry(String name) {
    def i = name.indexOf('/')
    def defaultDomain = ''

    if (i == -1 || (!name[0..<i].contains('.') && !name[0..<i].contains(':') && name[0..<i] != 'localhost')){
        return  defaultDomain
    }
    return 'https://' + name[0..<i]
}

def clean_docker_containers() {
    println("Listing docker containers to attempt removal")
    sh "docker ps -a"
    def containers = sh (script: "docker ps -a --format \"{{.ID}}\"", returnStdout: true).trim()
    if (containers) {
        println("Stop all docker containers")
        sh "docker ps -a --format \"{{.ID}}\" | xargs docker stop"
        println("Remove all docker containers")
        sh "docker ps -a --format \"{{.ID}}\" | xargs docker rm --force"
    }
}

def prepare_docker_environment() {
    clean_docker_containers()
    DOCKER_IMAGE_ID = get_docker_image_id(DOCKER_IMAGE)

    // if there is no id found then attempt to pull it
    if (!DOCKER_IMAGE_ID) {
        echo "${DOCKER_IMAGE} not found locally, attempting to pull from dockerhub"
        dockerRegistry = getDockerRegistry(DOCKER_IMAGE)
        dockerCredentialID = variableFile.get_user_credentials_id(dockerRegistry.replaceAll('https://','') ?: 'dockerhub')
        docker.withRegistry(dockerRegistry, "${dockerCredentialID}") {
            sh "docker pull ${DOCKER_IMAGE}"
        }
        DOCKER_IMAGE_ID = get_docker_image_id(DOCKER_IMAGE)
    }
    echo "Using ID ${DOCKER_IMAGE_ID} of ${DOCKER_IMAGE}"
}

def get_docker_image_id(image) {
    return sh (script: "docker images ${image} --format \"{{.ID}}\"", returnStdout: true).trim()
}

def _build_all() {
    try {
        cleanWorkspace(false)
        add_node_to_description()
        // initialize BOOT_JDK, FREEMARKER, OPENJDK_REFERENCE_REPO here
        // to correctly expand $HOME variable
        variableFile.set_build_variables_per_node()
        get_source()
        variableFile.set_sdk_variables()
        variableFile.set_artifactory_config(BUILD_IDENTIFIER)
        variableFile.set_build_custom_options()
        build()
        archive_sdk()
    } finally {
        KEEP_WORKSPACE = (params.KEEP_WORKSPACE) ?: false
        cleanWorkspace(KEEP_WORKSPACE)
    }
}

// TODO: remove this workaround when https://github.com/adoptium/infrastructure/issues/3597 resolved. related: infra 9292
def create_docker_image_locally()
{
    new_image_name = DOCKER_IMAGE.replace(':','-') + '_cuda'
    // Always build a new cuda image. Remove the old one if it exists.
    CUDA_DOCKER_IMAGE_ID = get_docker_image_id(new_image_name)
    if (CUDA_DOCKER_IMAGE_ID) {
        sh "docker rmi $CUDA_DOCKER_IMAGE_ID"
    }
    sh """
        echo "FROM nvcr.io/nvidia/cuda:12.2.0-devel-ubi8 AS cuda
            FROM ${DOCKER_IMAGE}
            RUN mkdir -p /usr/local/cuda/nvvm
            COPY --from=cuda /usr/local/cuda/include         /usr/local/cuda/include
            COPY --from=cuda /usr/local/cuda/nvvm/include    /usr/local/cuda/nvvm/include
            ENV CUDA_HOME='/usr/local/cuda'" > dockerFile
    """
    println "Preparing Docker image ${new_image_name} locally ..."
    dockerRegistry = getDockerRegistry(DOCKER_IMAGE)
    dockerCredentialID = variableFile.get_user_credentials_id(dockerRegistry.replaceAll('https://','') ?: 'dockerhub')
    docker.withRegistry(dockerRegistry, "${dockerCredentialID}") {
            docker.build(new_image_name, "-f dockerFile .")
    }
    DOCKER_IMAGE = new_image_name
}

def build_all() {
    stage ('Queue') {
        timeout(time: 10, unit: 'HOURS') {
            node("${NODE}") {
                timeout(time: 5, unit: 'HOURS') {
                    stage('Setup') {
                        if ("${DOCKER_IMAGE}") {
                            // TODO: Remove this workaround when https://github.com/adoptium/infrastructure/issues/3597 is resolved. Related: infra 9292.
                            if ((PLATFORM ==~ /ppc64le_linux.*/) || (PLATFORM ==~ /x86-64_linux.*/)) {
                                create_docker_image_locally()
                            }
                            prepare_docker_environment()
                            docker.image(DOCKER_IMAGE_ID).inside("-v /home/jenkins/openjdk_cache:/home/jenkins/openjdk_cache:rw,z -v /home/jenkins/.ssh:/home/jenkins/.ssh:rw,z") {
                                _build_all()
                            }
                        } else {
                            _build_all()
                        }
                    }
                }
            }
            upload_artifactory_post()
        }
    }
}

def match_fail_pattern(outputLines) {
    if (!FAIL_PATTERN) {
        return false
    }

    println("Build failure, searching fail pattern in the last ${outputLines.size()} output lines")

    for (failPattern in FAIL_PATTERN.tokenize('|')) {
        Pattern pattern = Pattern.compile(failPattern)
        for (line in outputLines) {
            Matcher matcher = pattern.matcher(line)
            if (matcher.find()) {
                println("Found ${failPattern} fail pattern!")
                return true
            }
        }
    }

    println("Fail pattern not found!")
    return false
}

def recompile() {
    def maxRetry = 3
    def retryCounter = 0
    def doRetry = true

    while ((maxRetry > retryCounter) && doRetry) {
        retryCounter++
        println("Attempt to recompile, retry: ${retryCounter}")

        try {
            sh "make clean && ${get_compile_command()}"
            doRetry = false
        } catch (Exception f) {
            // crop console output, search only new output
            def newLines = currentBuild.getRawBuild().getLog(LOG_MAX_LINES).minus(LOG_LINES)
            // cache log
            LOG_LINES.addAll(newLines)

            if (!match_fail_pattern(newLines)) {
                // different error
                archive_diagnostics()
                throw f
            }
            if (retryCounter >= maxRetry) {
                throw f
            }
        }
    }
}

return this
