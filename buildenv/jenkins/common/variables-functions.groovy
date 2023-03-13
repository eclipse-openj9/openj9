/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/*
 * Represents a single build spec ie. platform it
 */
class Buildspec {
    private List parents;
    private my_def;

    /*
     * Helper function to check if a variable is a map.
     * This is needed because .isMap() is not on the default jenkins allow list
     */
    private static boolean isMap(x) {
        switch (x) {
        case Map:
            return true
        default:
            return false
        }
    }

    /*
     * If x can be converted to integer, return the result.
     * else return x
     */
    private static toKey(x) {
        def key = x
        try {
            key = x as int
        } catch (def e) { }
        return key
    }

    /* perform a repeated map lookup of a given '.' separated name in my def
     * eg. getNestedField('foo.bar.baz') is equivilent to
     * my_def['foo']['bar']['baz'], with the added benefit that if any lookup
     * along the path fails, null is returned rather than throwing an exception
     */
    private getNestedField(String full_name) {
        def names = full_name.split("\\.")
        def value = my_def
        // note we don't really want to find, but we use this as a verson of .each which we can abort
        if (names.size() == 0) {
            return null
        }
        names.find { element_name ->
            if (value.containsKey(element_name)) {
                value = value[element_name]
                return false // continue iterating the list
            } else {
                value = null
                return true // abort processing
            }
        }
        return value
    }

    /*
     * look up a scalar field for the first field in this list to have a value:
     * if <name> is a map:
     *    - <name>.<sdk_ver>
     *    - <name>.all
     * else:
     *    - <name>
     *  - <parent>.getScalarField()
     * with the parents being evaluated in the order they are listed in the yaml file
     */
    public getScalarField(name, sdk_ver) {
        def sdk_key = toKey(sdk_ver)
        def field = getNestedField(name)
        if (field == null) {
            field = parents.findResult {it.getScalarField(name, sdk_ver)}
        }

        // Does this entry specify different values for different sdk versions?
        if (null != field && isMap(field)) {
            // If we have an sdk specific value use that
            if (field.containsKey(sdk_key)) {
                field = field[sdk_key]
            } else {
                // else fall back to the "all" key
                field = field['all']
            }
        }
        return field
    }

    /* Get a list of values composed by concatinating the values of the
     * following fields in order:
     * - <parent>.getVectorField()
     * if <name> is a map:
     *   - <name>.all
     *   - <name>.sdk
     * else:
     *   - <name>
     * with parents being evaluated in the order they are listed in the yaml file.
     * - Note: any list elements which are themselves list ar flattened.
     */
    public List getVectorField(name, sdk_ver) {
        // Yaml will use an integer key if possible, otherwise it falls back to string
        def sdk_key = toKey(sdk_ver)

        def field_value = []
        parents.each { parent ->
            field_value += parent.getVectorField(name, sdk_ver);
        }
        def my_value = getNestedField(name)
        if (my_value != null) {
            // Do we specify different values for different SDK versions?
            if (isMap(my_value)) {
                // If there is an "all" definition put that first
                if (my_value.containsKey('all')) {
                    field_value += my_value['all']
                }
                // If we have an SDK specific value, add that as well
                if (my_value.containsKey(sdk_key)) {
                    def sdk_val = my_value[sdk_key]
                    // Special case handling for old style variables where
                    // excluded tests are stored as a map rather than a list
                    if (isMap(sdk_val)) {
                        field_value += sdk_val.keySet()
                    } else {
                        field_value += sdk_val
                    }
                }
            } else {
                field_value += my_value
            }
        }
        return field_value.flatten()
    }

    /* Construct a build spec
     * mgr - reference to the BuildspecManager used to cache Buildspecs
     * cfg - Collection returned from yaml corresponding to this buildspec.
     *       Typically this would be VARIABLES[<build_spec_name>]
     */
    public Buildspec(mgr, cfg) {
        my_def = cfg
        parents = []
        if (my_def.containsKey('extends')) {
            my_def['extends'].each { parent_name ->
                parents << mgr.getSpec(parent_name)
            }
        }
    }
}

/*
 * Used to cache a map of names -> Builspecs
 */
class BuildspecManager {

    private buildspecs
    private vars

    /*
     * vars_ - Collection that the buildspec info should be pulled from
     *         Typically this would be VARIABLES.
     */
    public BuildspecManager(vars_) {
        vars = vars_
        buildspecs = [:]
    }

    /*
     * Get a Buildspec for a given name.
     */
    @NonCPS
    public getSpec(name) {
        if (!buildspecs.containsKey(name)) {
            buildspecs[name] = new Buildspec(this, vars[name])
        }
        return buildspecs[name]
    }
}

def VARIABLES
def buildspec_manager
pipelineFunctions = load 'buildenv/jenkins/common/pipeline-functions.groovy'

/*
 * Parses the Jenkins job variables file and populates the VARIABLES collection.
 * If a variables file is not set, parse the default variables file.
 * The variables file is in YAML format and contains configuration settings
 * required to compile and test the OpenJ9 extensions for OpenJDK on multiple
 * platforms.
 */
def parse_variables_file() {
    DEFAULT_VARIABLE_FILE = "buildenv/jenkins/variables/defaults.yml"

    // check if a variable file is passed as a Jenkins build parameter
    // if not use default configuration settings
    VARIABLE_FILE = get_variables_file()
    if (!VARIABLE_FILE) {
        VARIABLE_FILE = "${DEFAULT_VARIABLE_FILE}"
    }

    if (!fileExists("${VARIABLE_FILE}")) {
        error("Missing variable file: ${VARIABLE_FILE}")
    }

    echo "Using variables file: ${VARIABLE_FILE}"
    VARIABLES = readYaml file: "${VARIABLE_FILE}"
    buildspec_manager = new BuildspecManager(VARIABLES)
}

/*
 * Fetch the user provided variable file.
 */
def get_variables_file() {
    VARIABLE_FILE = (params.VARIABLE_FILE) ? params.VARIABLE_FILE : ""
    echo "VARIABLE_FILE:'${VARIABLE_FILE}'"
    VENDOR_REPO = (params.VENDOR_REPO) ? params.VENDOR_REPO : ""
    echo "VENDOR_REPO:'${VENDOR_REPO}'"
    VENDOR_BRANCH = (params.VENDOR_BRANCH) ? params.VENDOR_BRANCH : ""
    echo "VENDOR_BRANCH:'${VENDOR_BRANCH}'"
    VENDOR_CREDENTIALS_ID = (params.VENDOR_CREDENTIALS_ID) ? params.VENDOR_CREDENTIALS_ID : ""
    echo "VENDOR_CREDENTIALS_ID:'${VENDOR_CREDENTIALS_ID}'"
    if (VARIABLE_FILE && VENDOR_REPO && VENDOR_BRANCH) {
        if (VENDOR_CREDENTIALS_ID) {
            sshagent(credentials:[VENDOR_CREDENTIALS_ID]) {
                checkout_file()
            }
        } else {
            checkout_file()
        }
    }
    return VARIABLE_FILE
}

/*
 * Check out the user variable file.
 */
def checkout_file() {
    sh "git config remote.vendor.url ${VENDOR_REPO}"
    sh "git config remote.vendor.fetch +refs/heads/${VENDOR_BRANCH}:refs/remotes/vendor/${VENDOR_BRANCH}"
    sh "git fetch vendor"
    sh "git checkout vendor/${VENDOR_BRANCH} -- ${VARIABLE_FILE}"
}

/*
 * Sets the Git repository URLs and branches for all sources required to build
 * the OpenJ9 extensions for OpenJDK.
 * Initializes variables to the value passed as build parameters.
 * When no values are available as build parameters set them to the values
 * available with the variable file otherwise set them to empty strings.
 */
def set_repos_variables(BUILD_SPECS=null) {
    // check if passed as Jenkins build parameters
    // if not set as params, check the variables file

    if (!BUILD_SPECS) {
        // set variables (as strings) for a single OpenJDK release build

        // fetch OpenJDK repo and branch from variables file as a map
        def openjdk_info = get_openjdk_info(SDK_VERSION, [SPEC], false)

        OPENJDK_REPO = openjdk_info.get(SPEC).get('REPO')
        OPENJDK_BRANCH = openjdk_info.get(SPEC).get('BRANCH')
        OPENJDK_SHA = openjdk_info.get(SPEC).get('SHA')

        echo "Using OPENJDK_REPO = ${OPENJDK_REPO} OPENJDK_BRANCH = ${OPENJDK_BRANCH} OPENJDK_SHA = ${OPENJDK_SHA}"

        // set OpenJ9 and OMR repos, branches and SHAs
        set_extensions_variables()

     } else {
        // set OpenJDK variables (as maps) for a multiple releases build

        def build_releases = get_build_releases(BUILD_SPECS)
        build_releases.each { release ->
            // fetch OpenJDK repo and branch from variables file as a map
            def openjdk_info = get_openjdk_info(release, get_build_specs_by_release(release, BUILD_SPECS), true)

            def repos = [:]
            def branches = [:]
            def shas = [:]

            openjdk_info.each { build_spec, info_map ->
                repos.put(build_spec, info_map['REPO'])
                branches.put(build_spec, info_map['BRANCH'])
                shas.put(build_spec, info_map['SHA'])
            }

            OPENJDK_REPO[release] = repos
            OPENJDK_BRANCH[release] = branches
            OPENJDK_SHA[release] = shas
        }

        echo "Using OPENJDK_REPO = ${OPENJDK_REPO.toString()} OPENJDK_BRANCH = ${OPENJDK_BRANCH.toString()} OPENJDK_SHA = ${OPENJDK_SHA.toString()}"

        // default URL and branch for the OpenJ9 and OMR repositories (no entries in defaults.yml)
        EXTENSIONS = ['OpenJ9': ['repo': 'https://github.com/eclipse-openj9/openj9.git',     'branch': 'master'],
                      'OMR'   : ['repo': 'https://github.com/eclipse-openj9/openj9-omr.git', 'branch': 'openj9']]

        // set OpenJ9 and OMR repos, branches and SHAs
        set_extensions_variables(EXTENSIONS)
    }
}

/*
 * Initializes OpenJDK repository URL, branch, SHA for given release and returns
 * them as a map.
 * Build parameters take precedence over custom variables (see variables file).
 * Throws an error if repository URL or branch are not provided.
 */
def get_openjdk_info(SDK_VERSION, SPECS, MULTI_RELEASE) {
    // map to store git repository information by spec
    def openjdk_info = [:]

    // fetch values from variables file
    def default_openjdk_info = get_value(VARIABLES.openjdk, SDK_VERSION)

    // the build expects the following parameters:
    // OPENJDK*_REPO, OPENJDK*_BRANCH, OPENJDK*_SHA,
    // OPENJDK*_REPO_<spec>, OPENJDK*_BRANCH_<spec>, OPENJDK*_SHA_<spec>
    def param_name = "OPENJDK"
    if (MULTI_RELEASE) {
        // if a wrapper pipeline build, expect parameters names to be prefixed with "OPENJDK<version>_"
        param_name += SDK_VERSION
    }

    for (build_spec in SPECS) {

        // default values from variables file
        def repo = default_openjdk_info.get('default').get('repoUrl')
        def branch = default_openjdk_info.get('default').get('branch')
        def sha = ''

        def shortBuildSpecName = build_spec
        for (entry in default_openjdk_info.entrySet()) {
            if ((entry.key != 'default') && entry.key.contains(build_spec)) {
                repo = entry.value.get('repoUrl')
                branch = entry.value.get('branch')
                shortBuildSpecName = entry.key.get(0)
                break
            }
        }

        // check build parameters
        if (params."${param_name}_REPO") {
            // got OPENJDK*_REPO build parameter, overwrite repo
            repo = params."${param_name}_REPO"
        }

        if (params."${param_name}_BRANCH") {
            // got OPENJDK*_BRANCH build parameter, overwrite branch
            branch = params."${param_name}_BRANCH"
        }

        if (params."${param_name}_SHA") {
            // got OPENJDK*_SHA build parameter, overwrite sha
            sha = params."${param_name}_SHA"
        }

        if (MULTI_RELEASE) {
            if (params."${param_name}_REPO_${shortBuildSpecName}") {
                // got OPENJDK*_REPO_<spec> build parameter, overwrite repo
                repo = params."${param_name}_REPO_${shortBuildSpecName}"
            }

            if (params."${param_name}_BRANCH_${shortBuildSpecName}") {
                // got OPENJDK*_BRANCH_<spec> build parameter, overwrite branch
                branch = params."${param_name}_BRANCH_${shortBuildSpecName}"
            }

            if (params."${param_name}_SHA_${shortBuildSpecName}") {
                // got OPENJDK*_SHA_<spec> build parameter, overwrite sha
                sha = params."${param_name}_SHA_${shortBuildSpecName}"
            }
        }

        // populate the map
        openjdk_info.put(build_spec, ['REPO': convert_url(repo), 'BRANCH': branch, 'SHA': sha])
    }

    return openjdk_info
}

/*
 * Initializes the OpenJ9 and OMR repositories variables with values from
 * the variables file if they are not set as build parameters.
 * If no values available in the variable file then initialize these variables
 * with default values, otherwise set them to empty strings (to avoid
 * downstream builds errors - Jenkins build parameters should not be null).
 */
def set_extensions_variables(defaults=null) {
    OPENJ9_REPO = params.OPENJ9_REPO
    if (!OPENJ9_REPO) {
        // set it to the value set into the variable file
        OPENJ9_REPO = VARIABLES.openj9_repo
    }

    if (!OPENJ9_REPO) {
        OPENJ9_REPO = ''
        if (defaults) {
            OPENJ9_REPO = defaults.get('OpenJ9').get('repo')
        }
    }
    OPENJ9_REPO = convert_url(OPENJ9_REPO)

    OPENJ9_BRANCH = params.OPENJ9_BRANCH
    if (!OPENJ9_BRANCH) {
         OPENJ9_BRANCH = VARIABLES.openj9_branch
    }

    if (!OPENJ9_BRANCH) {
        OPENJ9_BRANCH = ''
        if (defaults) {
            OPENJ9_BRANCH = defaults.get('OpenJ9').get('branch')
        }
    }

    OPENJ9_SHA = params.OPENJ9_SHA
    if (!OPENJ9_SHA) {
        OPENJ9_SHA = ''
    }

    OMR_REPO = params.OMR_REPO
    if (!OMR_REPO) {
        // set it to the value set into the variable file
        OMR_REPO = VARIABLES.omr_repo
    }

    if (!OMR_REPO) {
        OMR_REPO = ''
        if (defaults) {
            OMR_REPO = defaults.get('OMR').get('repo')
        }
    }
    OMR_REPO = convert_url(OMR_REPO)

    OMR_BRANCH = params.OMR_BRANCH
    if (!OMR_BRANCH) {
        // set it to the value set into the variable file
        OMR_BRANCH = VARIABLES.omr_branch
    }

    if (!OMR_BRANCH) {
        OMR_BRANCH = ''
        if (defaults) {
            OMR_BRANCH = defaults.get('OMR').get('branch')
        }
    }

    OMR_SHA = params.OMR_SHA
    if (!OMR_SHA) {
        OMR_SHA = ''
    }

    echo "Using OPENJ9_REPO = ${OPENJ9_REPO} OPENJ9_BRANCH = ${OPENJ9_BRANCH} OPENJ9_SHA = ${OPENJ9_SHA}"
    echo "Using OMR_REPO = ${OMR_REPO} OMR_BRANCH = ${OMR_BRANCH} OMR_SHA = ${OMR_SHA}"
}

/*
 * Returns the value of the element identified by the given key or an empty string
 * if the map contains no mapping for the key.
 */
def get_value(MAP, KEY) {
    if (MAP != null) {
        for (item in MAP) {
            if ("${item.key}" == "${KEY}") {
                if ((item.value instanceof Map) || (item.value instanceof java.util.List)) {
                    return item.value
                } else {
                    // stringify value
                    return "${item.value}"
                }
            }
        }
    }

    return ''
}

/*
 * Returns Jenkins credentials ID required to connect to GitHub using
 * the ssh protocol.
 * Returns empty string if no values is provided in the variables file (not
 * required for public repositories).
 */
def get_git_user_credentials_id() {
    return get_user_credentials_id("git")
}

/*
 * Returns Jenkins credentials ID required to connect to GitHub using
 * API (username & password)
 * Returns empty string if no values is provided in the variables file
 */
def get_github_user_credentials_id() {
    return get_user_credentials_id("github")
}

/*
 * Returns Jenkins credentials ID from the variable file for given key.
 */
def get_user_credentials_id(KEY) {
    if (VARIABLES.credentials && VARIABLES.credentials."${KEY}") {
        return VARIABLES.credentials."${KEY}"
    }

    return ''
}

/*
 * Sets the NODE variable to the labels identifying nodes by platform suitable
 * to run a Jenkins job.
 * Fetches the labels from the variables file when the node is not set as build
 * parameter.
 * The node's labels could be a single label - e.g. label1 - or a boolean
 * expression - e.g. label1 && label2 || label3.
 */
def set_node(job_type) {
    // fetch labels for given platform/spec
    NODE = ''
    DOCKER_IMAGE = buildspec_manager.getSpec(SPEC).getScalarField("node_labels.docker_image", SDK_VERSION) ?: ''
    for (key in ['NODE', 'LABEL']) {
        // check the build parameters
        if (params.containsKey(key)) {
            NODE = params.get(key)
            break
        }
    }

    if (!NODE) {
        // fetch from variables file
        NODE = buildspec_manager.getSpec(SPEC).getScalarField("node_labels.${job_type}", SDK_VERSION)
        if (!NODE) {
            error("Missing ${job_type} NODE!")
        }
    }
}

/*
 * Set the RELEASE variable with the value provided in the variables file.
 */
def set_release() {
    RELEASE = buildspec_manager.getSpec(SPEC).getScalarField("release", SDK_VERSION)
}

/*
 * Set the JDK_FOLDER variable with the value provided in the variables file.
 */
def set_jdk_folder() {
    JDK_FOLDER = buildspec_manager.getSpec('misc').getScalarField("jdk_image_dir", SDK_VERSION)
}

/*
 * Sets variables for a job that builds the OpenJ9 extensions for OpenJDK for a
 * given platform and version.
 */
def set_build_variables() {
    set_repos_variables()

    def buildspec = buildspec_manager.getSpec(SPEC)

    // fetch values per spec and Java version from the variables file
    set_release()
    set_jdk_folder()
    set_build_extra_options()
    set_sdk_impl()

    // set variables for the build environment configuration
    // check job parameters, if not provided default to variables file
    BUILD_ENV_VARS = params.BUILD_ENV_VARS
    if (!BUILD_ENV_VARS) {
        BUILD_ENV_VARS = buildspec.getVectorField("build_env.vars", SDK_VERSION).join(" ")
    }

    BUILD_ENV_VARS_LIST = []
    if (BUILD_ENV_VARS) {
        // expect BUILD_ENV_VARS to be a space separated list of environment variables
        // e.g. <variable1>=<value1> <variable2>=<value2> ... <variableN>=<valueN>
        // convert space separated list to array
        BUILD_ENV_VARS_LIST.addAll(BUILD_ENV_VARS.tokenize(' '))
    }

    EXTRA_BUILD_ENV_VARS = params.EXTRA_BUILD_ENV_VARS
    if (EXTRA_BUILD_ENV_VARS) {
        BUILD_ENV_VARS_LIST.addAll(EXTRA_BUILD_ENV_VARS.tokenize(' '))
    }

    BUILD_ENV_CMD = params.BUILD_ENV_CMD
    if (!BUILD_ENV_CMD) {
        BUILD_ENV_CMD = buildspec.getScalarField("build_env.cmd", SDK_VERSION)
    }

    if (BUILD_ENV_CMD) {
        // BUILD_ENV_CMD is used as preamble command
        BUILD_ENV_CMD += ' && '
    } else {
        BUILD_ENV_CMD = ''
    }

    echo "Using BUILD_ENV_CMD = ${BUILD_ENV_CMD}, BUILD_ENV_VARS_LIST = ${BUILD_ENV_VARS_LIST.toString()}"

    if (VARIABLES.misc.custom_filename) {
        customFile = load VARIABLES.misc.custom_filename
    } else {
        customFile = null
    }
}

def set_sdk_variables() {
    set_sdk_versions()
    DATESTAMP = get_date()
    SDK_FILE_EXT = SPEC.contains('zos') ? '.pax' : '.tar.gz'
    SDK_FILENAME = get_value(VARIABLES.misc.sdk_filename_template, BUILD_IDENTIFIER) ?: get_value(VARIABLES.misc.sdk_filename_template, 'default')
    echo "SDK_FILENAME (before processing):'${SDK_FILENAME}'"
    // If filename has unresolved variables at this point, use Groovy Template Binding engine to resolve them
    if (SDK_FILENAME.contains('$')) {
        set_sdk_impl()
        // Add all variables that could be used in a template
        def binding = ["SDK_IMPL":SDK_IMPL, "SPEC":SPEC, "SDK_VERSION":SDK_VERSION, "FULL_SDK_VERSION":FULL_SDK_VERSION, "JAVA_RELEASE":JAVA_RELEASE, "JAVA_SUB_RELEASE":JAVA_SUB_RELEASE, "DATESTAMP":DATESTAMP, "SDK_FILE_EXT":SDK_FILE_EXT]
        def engine = new groovy.text.SimpleTemplateEngine()
        SDK_FILENAME = engine.createTemplate(SDK_FILENAME).make(binding)
    }
    echo "SDK_FILENAME:'${SDK_FILENAME}'"

    TEST_FILENAME = "test-images.tar.gz"
    CODE_COVERAGE_FILENAME = "code-coverage-files.tar.gz"
    JAVADOC_FILENAME = "OpenJ9-JDK${SDK_VERSION}-Javadoc-${SPEC}-${DATESTAMP}.tar.gz"
    JAVADOC_OPENJ9_ONLY_FILENAME = "OpenJ9-JDK${SDK_VERSION}-Javadoc-openj9-${SPEC}-${DATESTAMP}.tar.gz"
    DEBUG_IMAGE_FILENAME = "debug-image.tar.gz"
    echo "Using SDK_FILENAME = ${SDK_FILENAME}"
    echo "Using TEST_FILENAME = ${TEST_FILENAME}"
    echo "Using CODE_COVERAGE_FILENAME = ${CODE_COVERAGE_FILENAME}"
    echo "Using JAVADOC_FILENAME = ${JAVADOC_FILENAME}"
    echo "Using JAVADOC_OPENJ9_ONLY_FILENAME = ${JAVADOC_OPENJ9_ONLY_FILENAME}"
    echo "Using DEBUG_IMAGE_FILENAME = ${DEBUG_IMAGE_FILENAME}"
}

def set_sdk_versions() {
    /*
     * Get full version from tag and replace + with _
     * Eg. JDK8
     * OPENJDK_TAG := jdk8u272-b08
     * FULL_SDK_VERSION = jdk8u272-b08
     * JAVA_RELEASE = 8
     * JAVS_SUB_RELEASE = 8u272
     *
     * Eg. JDK11
     * OPENJDK_TAG := jdk-11.0.8+10
     * FULL_SDK_VERSION = jdk-11.0.8_10
     * JAVA_RELEASE = 11.0
     * JAVS_SUB_RELEASE = 11.0.8
     *
     * Eg. Initial feature release
     * OPENJDK_TAG := jdk-15+36
     * FULL_SDK_VERSION = jdk-15_36
     * JAVA_RELEASE = 15.0
     * JAVA_SUB_RELEASE = 15.0.0
     */

    FULL_SDK_VERSION = sh (
        script: "sed -n -e 's/+/_/g; s/^OPENJDK_TAG := //p' < closed/openjdk-tag.gmk",
        returnStdout: true
    ).trim()
    def subReleaseStartIndexChar = 'jdk-'
    def subReleaseEndIndexChar = '_'
    def releaseEndIndexChar = '.'
    if (SDK_VERSION.equals('8')) {
        subReleaseStartIndexChar = 'jdk'
        subReleaseEndIndexChar = '-'
        releaseEndIndexChar = 'u'
    }
    JAVA_SUB_RELEASE = FULL_SDK_VERSION.substring(FULL_SDK_VERSION.indexOf(subReleaseStartIndexChar) + subReleaseStartIndexChar.length(), FULL_SDK_VERSION.lastIndexOf(subReleaseEndIndexChar))

    if (!JAVA_SUB_RELEASE.contains('.') && !SDK_VERSION.equals('8')) {
        // Must be the initial
        JAVA_SUB_RELEASE += ".0.0"
    }
    JAVA_RELEASE = JAVA_SUB_RELEASE.substring(0,JAVA_SUB_RELEASE.lastIndexOf(releaseEndIndexChar))

    echo "FULL_SDK_VERSION:'${FULL_SDK_VERSION}'"
    echo "JAVA_RELEASE:'${JAVA_RELEASE}'"
    echo "JAVA_SUB_RELEASE:'${JAVA_SUB_RELEASE}'"
}

def get_date() {
    //Returns a string for the current date YYYYMMDD-HHMMSS
    return sh (
        script: 'date +%Y%m%d-%H%M%S',
        returnStdout: true
    ).trim()
}

/*
 * Set TESTS_TARGETS, indicating the level of testing.
 */
def set_test_targets() {
    // Map of Maps
    TESTS = [:]
    if (TESTS_TARGETS != 'none') {
        for (target in TESTS_TARGETS.replaceAll("\\s","").toLowerCase().tokenize(',')) {
            switch (target) {
                case ["sanity", "extended"]:
                    TESTS["${target}.functional"] = [:]
                    break
                default:
                    if (target.contains('+')) {
                        def id = target.substring(0, target.indexOf('+'))
                        TESTS[id] = [:]
                        TESTS[id]['testFlag'] = target.substring(target.indexOf('+') +1).toUpperCase()
                    } else {
                        TESTS[target] = [:]
                    }
                    break
            }
        }
    }
    echo "TESTS:'${TESTS}'"

    set_sdk_impl()
}

def set_sdk_impl() {
    // Set SDK Implementation, default to OpenJ9
    SDK_IMPL = buildspec.getScalarField("sdk_impl", 'all') ?: 'openj9'
    SDK_IMPL_SHORT = buildspec.getScalarField("sdk_impl_short", 'all') ?: 'j9'
    echo "SDK_IMPL:'${SDK_IMPL}'"
    echo "SDK_IMPL_SHORT:'${SDK_IMPL_SHORT}'"
}

/*
 * Set TEST_FLAG for all targets if defined in variable file.
 * Set EXCLUDED_TESTS if defined in variable file.
 * Set EXTRA_TEST_LABELS map if defined in variable file.
 */
def set_test_misc() {
    if (!params.TEST_NODE) {
        // Only add extra test labels if the user has not specified a specific TEST_NODE
        TESTS.each { id, target ->
            target['extraTestLabels'] = buildspec.getVectorField("extra_test_labels", SDK_VERSION).join('&&') ?: ''
        }
    } else {
        TESTS.each { id, target ->
            target['extraTestLabels'] = ''
        }
    }

    def buildspec = buildspec_manager.getSpec(SPEC)
    def excludedTests = buildspec.getVectorField("excluded_tests", SDK_VERSION)
    if (excludedTests) {
        TESTS.each { id, target ->
            if (excludedTests.contains(id)) {
                echo "Excluding test target:'${id}'"
                TESTS.remove(id)
            }
        }
    }

    TESTS.each { id, target ->
        flag = buildspec.getScalarField("test_flags", id) ?: ''
        if (!target['testFlag']) {
            target['testFlag'] = flag
        } else if (flag) {
            error("Cannot set more than 1 TEST_FLAG:'${target['testFlag']}' & '${flag}'")
        }

        // Set test param KEEP_REPORTDIR to false unless set true in variable file.
        target['keepReportDir'] = buildspec_manager.getSpec('misc').getScalarField("test_keep_reportdir", id) ?: 'false'

        target['buildList'] = buildspec.getScalarField("test_build_list", id) ?: ''
    }

    echo "TESTS:'${TESTS}'"
}

def set_slack_channel() {
    SLACK_CHANNEL = params.SLACK_CHANNEL
    if (!SLACK_CHANNEL && !params.PERSONAL_BUILD.equalsIgnoreCase('true')) {
        SLACK_CHANNEL = VARIABLES.slack_channel
    }
}

def set_artifactory_config(id="Nightly") {
    set_basic_artifactory_config(id)

    if (VARIABLES.artifactory.defaultGeo) {
        def repo = ARTIFACTORY_CONFIG['repo']
        ARTIFACTORY_CONFIG['uploadDir'] = get_value(VARIABLES.artifactory.uploadDir, id) ?: get_value(VARIABLES.artifactory.uploadDir, 'default')
        if (!ARTIFACTORY_CONFIG['uploadDir'].endsWith('/')) {
            ARTIFACTORY_CONFIG['uploadDir'] += '/'
        }
        // If uploadDir has unresolved variables at this point, use Groovy Template Binding engine to resolve them
        if (ARTIFACTORY_CONFIG['uploadDir'].contains('$')) {
            if (ARTIFACTORY_CONFIG['uploadDir'].contains('SDK_IMPL') && !binding.hasVariable('SDK_IMPL')) {
                set_sdk_impl()
            }
            // Add all variables that could be used in a template
            def binding = ["JOB_NAME":JOB_NAME, "BUILD_ID":BUILD_ID, "repo":repo, "SDK_IMPL":SDK_IMPL, "JAVA_RELEASE":JAVA_RELEASE, "JAVA_SUB_RELEASE":JAVA_SUB_RELEASE, "SPEC":SPEC, "DATESTAMP":DATESTAMP]
            def engine = new groovy.text.SimpleTemplateEngine()
            ARTIFACTORY_CONFIG['uploadDir'] = engine.createTemplate(ARTIFACTORY_CONFIG['uploadDir']).make(binding)
        }

        ARTIFACTORY_CONFIG[VARIABLES.artifactory.defaultGeo]['uploadBool'] = true

        // Determine if we need to upload more than the default server
        if (ARTIFACTORY_CONFIG['geos'].size() > 1) {
            // What platform did we build on
            compilePlatform = get_node_platform(NODE_LABELS)

            // See if there are servers with colocated nodes of matching platform
            def testNodes = jenkins.model.Jenkins.instance.getLabel('ci.role.test').getNodes()
            for (node in testNodes) {
                def nodeGeo = get_node_geo(node.getLabelString())
                // If we haven't determined yet if we will need to upload to 'nodeGeo'...
                if (ARTIFACTORY_CONFIG[nodeGeo] && !ARTIFACTORY_CONFIG[nodeGeo]['uploadBool']) {
                    def nodePlatform = get_node_platform(node.getLabelString())
                    // Upload if there is a server at geo where there are machines matching our platform.
                    if (nodePlatform == compilePlatform && ARTIFACTORY_CONFIG['geos'].contains(nodeGeo)) {
                        ARTIFACTORY_CONFIG[nodeGeo]['uploadBool'] = true
                    }
                }
            }
        }
        echo "ARTIFACTORY_CONFIG:'${ARTIFACTORY_CONFIG}'"
    }
}

def set_basic_artifactory_config(id="Nightly") {
    ARTIFACTORY_CONFIG = [:]
    echo "Configure Artifactory..."

    if (VARIABLES.artifactory.defaultGeo) {
        // Allow default geo to be overridden with a param. Used by the Clenaup script to target a specific server.
        ARTIFACTORY_CONFIG['defaultGeo'] = params.ARTIFACTORY_GEO ?: VARIABLES.artifactory.defaultGeo
        ARTIFACTORY_CONFIG['geos'] = VARIABLES.artifactory.server.keySet()
        ARTIFACTORY_CONFIG['repo'] = get_value(VARIABLES.artifactory.repo, id) ?: get_value(VARIABLES.artifactory.repo, 'default')

        for (geo in ARTIFACTORY_CONFIG['geos']) {
            ARTIFACTORY_CONFIG[geo] = [:]
            ARTIFACTORY_CONFIG[geo]['server'] = get_value(VARIABLES.artifactory.server, geo)
            def numArtifacts = get_value(VARIABLES.artifactory.numArtifacts, geo)
            if (!numArtifacts) {
                numArtifacts = get_value(VARIABLES.build_discarder.logs, pipelineFunctions.convert_build_identifier(id))
            }
            ARTIFACTORY_CONFIG[geo]['numArtifacts'] = numArtifacts.toInteger()
            ARTIFACTORY_CONFIG[geo]['daysToKeepArtifacts'] = get_value(VARIABLES.artifactory.daysToKeepArtifacts, geo).toInteger()
            ARTIFACTORY_CONFIG[geo]['manualCleanup'] = get_value(VARIABLES.artifactory.manualCleanup, geo)
            ARTIFACTORY_CONFIG[geo]['vpn'] = get_value(VARIABLES.artifactory.vpn, geo)
        }

        /*
        * Write out default server values to string variables.
        * The upstream job calls job.getBuildVariables() which only returns strings.
        * Rather than parsing out the ARTIFACTORY_CONFIG map that is stored as a string,
        * we'll write out the values to env here as strings to save work later.
        */
        env.ARTIFACTORY_SERVER = ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['server']
        env.ARTIFACTORY_REPO = ARTIFACTORY_CONFIG['repo']
        env.ARTIFACTORY_NUM_ARTIFACTS = ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['numArtifacts']
        env.ARTIFACTORY_DAYS_TO_KEEP_ARTIFACTS = ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['daysToKeepArtifacts']
        env.ARTIFACTORY_MANUAL_CLEANUP = ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['manualCleanup']
    }
}

def get_node_geo(nodeLabels) {
    if (nodeLabels.contains('ci.geo.')) {
        labelArray = nodeLabels.tokenize()
        for (label in labelArray) {
            if (label ==~ /ci\.geo\..*/) {
                return label.substring(7)
            }
        }
    }
    return ''
}

def get_node_platform(nodeLabels) {
    /*
    * xlinux -> arch=x86 && baseOS=linux
    * plinux -> arch=ppc64le && baseOS=linux
    * zlinux -> arch=s390x && baseOS=linux
    * aix -> baseOS=aix
    * windows -> baseOS=windows
    * osx -> baseOS=osx
    * zos -> baseOS=zos
    * aarch64 -> arch=aarch64
    */
    def arch = ''
    def baseOS = ''
    if (nodeLabels.contains('hw.arch.') && nodeLabels.contains('sw.os.')) {
        labelArray = nodeLabels.tokenize()
        for (label in labelArray) {
            switch (label) {
                case ~/hw\.arch\.[a-z0-9]+/:
                    arch = label.substring(8)
                    //println arch
                    break
                case ~/sw\.os\.[a-z]+/:
                    baseOS = label.substring(6)
                    //println baseOS
                    break
            }
        }
    }
    if (!arch || !baseOS) {
        echo "WARNING: Unable to determine node arch/os:'${nodeLabels}'"
        return ''
    }
    switch (baseOS) {
        case ['aix', 'windows', 'osx', 'zos']:
            return baseOS
            break
        case ['linux', 'ubuntu', 'rhel', 'cent', 'sles'] :
            switch (arch) {
                case 'x86':
                    return 'xlinux'
                    break
                case 'ppc64le':
                    return 'plinux'
                    break
                case 'ppc64':
                    return 'plinuxBE'
                case 's390x':
                    return 'zlinux'
                    break
                case ['aarch64', 'aarch32']:
                    return 'alinux'
                    break
                default:
                    echo "WARNING: Unknown OS:'${baseOS}' for arch:'${arch}'"
            }
            break
        default:
            echo "WARNING: Unknown baseOS:'${baseOS}'"
    }
    return ''
}

def set_restart_timeout() {
    RESTART_TIMEOUT = params.RESTART_TIMEOUT
    if (!RESTART_TIMEOUT) {
        RESTART_TIMEOUT = VARIABLES.restart_timeout.time
    }
    RESTART_TIMEOUT_UNITS = params.RESTART_TIMEOUT_UNITS
    if (!RESTART_TIMEOUT_UNITS) {
        RESTART_TIMEOUT_UNITS = VARIABLES.restart_timeout.units
    }
}

def set_build_identifier() {
    BUILD_IDENTIFIER = params.BUILD_IDENTIFIER
    if (!BUILD_IDENTIFIER) {
        BUILD_IDENTIFIER = ''
        if (params.ghprbPullId && params.ghprbGhRepository) {
            // If Pull Request build
            BUILD_IDENTIFIER = params.ghprbGhRepository + '#' + params.ghprbPullId
        } else if (PERSONAL_BUILD.equalsIgnoreCase('true')) {
            // If Personal build
            wrap([$class: 'BuildUser']) {
                BUILD_IDENTIFIER = "${BUILD_USER_EMAIL}"
            }
        } else {
            echo "WARNING: Can't determine appropriate BUILD_IDENTIFIER automatically. Please add it as a build parameter if you would like it set."
        }
    }
    if (BUILD_IDENTIFIER) {
        currentBuild.displayName = "#${BUILD_NUMBER} - ${BUILD_IDENTIFIER}"
    }
}

def set_custom_description() {
    if (params.CUSTOM_DESCRIPTION) {
        def TMP_DESC = (currentBuild.description) ? "<br>" + currentBuild.description : ""
        currentBuild.description = CUSTOM_DESCRIPTION + TMP_DESC
    }
}

/*
 * Adds a String type Parameter
 * Takes a Key-Value map which are used as the param name and default value
 */
def add_string_params(PARAMETERS_TO_ADD) {
    PARAMETERS_TO_ADD.each { key, value ->
        PARAMETERS.add(string(name: key, defaultValue: value, description: '', trim: true))
    }
}

def add_pr_to_description() {
    if (params.ghprbPullId) {
        def TMP_DESC = (currentBuild.description) ? currentBuild.description + "<br>" : ""
        currentBuild.description = TMP_DESC + "<a href=https://github.com/${params.ghprbGhRepository}/pull/${params.ghprbPullId}>PR #${params.ghprbPullId}</a>"
    }
}

def setup() {
    set_job_variables(params.JOB_TYPE)

    switch (params.JOB_TYPE) {
        case "pipeline":
            // pipelineFunctions already loads pipeline-functions, so in this case let's just dup the variable buildFile
            // TODO revisit all the loads and try to optimize.
            buildFile = pipelineFunctions
            SHAS = buildFile.get_shas(OPENJDK_REPO, OPENJDK_BRANCH, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, VENDOR_TEST_REPOS_MAP, VENDOR_TEST_BRANCHES_MAP, VENDOR_TEST_SHAS_MAP)
            BUILD_NAME = buildFile.get_build_job_name(SPEC, SDK_VERSION, BUILD_IDENTIFIER)
            // Stash DSL file so we can quickly load it on the Jenkins Manager node
            if (params.AUTOMATIC_GENERATION != 'false') {
                stash includes: 'buildenv/jenkins/jobs/pipelines/Pipeline_Template.groovy', name: 'DSL'
            }
            break
        case "build":
            buildFile = load 'buildenv/jenkins/common/build.groovy'
            break
        default:
            error("Unknown Jenkins job type:'${params.JOB_TYPE}'")
    }
}

/*
 * Initializes all of the required variables for a Jenkins job by given job type.
 */
def set_job_variables(job_type) {

    // initialize VARIABLES
    parse_variables_file()

    // fetch credentials required to connect to GitHub using ssh protocol
    set_user_credentials()

    // Setup Build Identifier so we can determine where a build belongs
    set_build_identifier()

    // check mandatory parameters if any
    try {
        validate_arguments(ARGS)
    } catch (MissingPropertyException e) {
        // ignore when ARGS is not set
    }

    // Add custom description
    set_custom_description()

    // Set ARCHIVE_JAVADOC flag
    ARCHIVE_JAVADOC = (params.ARCHIVE_JAVADOC) ? params.ARCHIVE_JAVADOC : false
    echo "Using ARCHIVE_JAVADOC = ${ARCHIVE_JAVADOC}"
    // Set CODE_COVERAGE flag
    CODE_COVERAGE = (params.CODE_COVERAGE) ? params.CODE_COVERAGE : false
    echo "Using CODE_COVERAGE = ${CODE_COVERAGE}"

    USE_TESTENV_PROPERTIES = params.USE_TESTENV_PROPERTIES ?: false
    echo "Using USE_TESTENV_PROPERTIES = ${USE_TESTENV_PROPERTIES}"

    switch (job_type) {
        case "build":
            // set the node the Jenkins build would run on
            set_node(job_type)
            // set variables for a build job
            set_build_variables()
            add_pr_to_description()
            break
        case "pipeline":
            currentBuild.description = "<a href=\"${RUN_DISPLAY_URL}\">Blue Ocean</a>"
            // set variables for a pipeline job
            set_repos_variables()
            set_adoptopenjdk_tests_repository()
            set_vendor_variables()
            set_build_extra_options()
            set_test_targets()
            set_test_misc()
            set_slack_channel()
            set_restart_timeout()
            add_pr_to_description()
            break
        case "wrapper":
            //set variable for pipeline all/personal
            set_repos_variables(BUILD_SPECS)
            set_build_extra_options(BUILD_SPECS)
            set_adoptopenjdk_tests_repository(get_build_releases(BUILD_SPECS))
            set_test_targets()
            set_restart_timeout()
            break
        default:
            error("Unknown Jenkins job type:'${job_type}'")
    }
}

/*
 * Checks if mandatory arguments are set.
 */
def validate_arguments(ARGS) {
    for (arg in ARGS) {
        if (!params.get(arg)) {
            error("Must specify ${arg}")
        }

        if (arg == 'PLATFORM') {
            // set SPEC - a groovy variable - here
            // SPEC cannot be a build parameter because build parameters are
            // exported as environment variables and it will overwrite the
            // make SPEC variable set in the configure phase in build.build() method
            SPEC = params.get(arg)
        }
    }
}

/*
 * Prints the error stack trace.
 */
def printStackTrace(e) {
    def writer = new StringWriter()
    e.printStackTrace(new PrintWriter(writer))
    echo writer.toString()
}

/*
 * Sets USER_CREDENTIALS_ID to the value provided in the variables file or an
 * empty string.
 */
def set_user_credentials() {
    USER_CREDENTIALS_ID = get_git_user_credentials_id()
    if (!USER_CREDENTIALS_ID) {
         USER_CREDENTIALS_ID = ''
    }
     GITHUB_API_CREDENTIALS_ID = get_github_user_credentials_id()
     if (!GITHUB_API_CREDENTIALS_ID) {
         GITHUB_API_CREDENTIALS_ID = ''
    }
}

/*
 * Converts from SSH to HTTPS protocol the public Git repository URL and from
 * HTTPS to SSH the private repositories.
 */
def convert_url(value) {
    if ((value && value.contains('git@')) && !USER_CREDENTIALS_ID) {
        // external repos, no credentials, use HTTPS protocol
        return value.replace(':', '/').replace('git@', 'https://')
    }

    if ((value && value.startsWith('https://')) && USER_CREDENTIALS_ID) {
        // private repos, have credentials, use SSH protocol
        return value.replace('https://', 'git@').replaceFirst('/', ':')
    }

    return value
}

/*
 * Checks for vendor test sources and initializes variables required to clone the
 * vendor test source Git repositories. Uses variable files when build parameters
 * are not provided.
 */
def set_vendor_variables() {
    VENDOR_TEST_REPOS_MAP = [:]
    VENDOR_TEST_BRANCHES_MAP = [:]
    VENDOR_TEST_DIRS_MAP = [:]
    VENDOR_TEST_SHAS_MAP = [:]

    // initialize variables from build parameters
    // VENDOR_TEST* build parameters are comma separated string parameters
    // convert each comma separated string parameter to array
    def branches = []
    if (params.VENDOR_TEST_BRANCHES) {
        branches.addAll(params.VENDOR_TEST_BRANCHES.tokenize(','))
    }

    def shas = []
    if (params.VENDOR_TEST_SHAS) {
        shas.addAll(params.VENDOR_TEST_SHAS.tokenize(','))
    }

    def dirs = []
    if (params.VENDOR_TEST_DIRS) {
        dirs.addAll(params.VENDOR_TEST_DIRS.tokenize(','))
    }

    if (params.VENDOR_TEST_REPOS) {
        // populate the VENDOR_TEST_* maps

        params.VENDOR_TEST_REPOS.tokenize(',').eachWithIndex { repoURL, index ->
            repoURL = repoURL.trim()
            REPO_NAME = get_repo_name_from_url(repoURL)
            VENDOR_TEST_REPOS_MAP[REPO_NAME] = repoURL

            if (branches[index]) {
                VENDOR_TEST_BRANCHES_MAP[REPO_NAME] = branches[index].trim()
            } else {
                error("Missing branch for ${repoURL} from VENDOR_TEST_BRANCHES parameter")
            }

            if (dirs[index]) {
                VENDOR_TEST_DIRS_MAP[REPO_NAME] = dirs[index].trim()
            }

            if (shas[index]) {
                VENDOR_TEST_SHAS_MAP[REPO_NAME] = shas[index].trim()
            }
        }
    } else {
        // fetch from variables file

        if (VARIABLES.vendor_source && VARIABLES.vendor_source.test) {
            // parse the vendor source multi-map
            // and populate the VENDOR_TEST_* maps

            VARIABLES.vendor_source.test.each { entry ->
                REPO_NAME = get_repo_name_from_url(entry.get('repo'))
                VENDOR_TEST_REPOS_MAP[REPO_NAME] = entry.get('repo')
                VENDOR_TEST_BRANCHES_MAP[REPO_NAME] = entry.get('branch')
                VENDOR_TEST_DIRS_MAP[REPO_NAME] = entry.get('directory')
            }
        }
    }

    // the downstream job expects VENDOR_TEST* parameters to be comma separated strings
    // convert VENDOR_TEST* maps to strings
    if (VENDOR_TEST_REPOS_MAP) {
        VENDOR_TEST_REPOS = VENDOR_TEST_REPOS_MAP.values().join(',')
    } else {
        VENDOR_TEST_REPOS = ''
    }

    if (VENDOR_TEST_BRANCHES_MAP) {
        VENDOR_TEST_BRANCHES = VENDOR_TEST_BRANCHES_MAP.values().join(',')
    } else {
        VENDOR_TEST_BRANCHES = ''
    }

    if (!VENDOR_TEST_SHAS_MAP) {
        VENDOR_TEST_SHAS = ''
    }

    if (VENDOR_TEST_DIRS_MAP) {
        VENDOR_TEST_DIRS = VENDOR_TEST_DIRS_MAP.values().join(',')
    } else {
        VENDOR_TEST_DIRS = ''
    }
}

/*
 * Extracts the Git repository name from URL and converts it to upper case.
 */
def get_repo_name_from_url(URL) {
    return URL.substring(URL.lastIndexOf('/') + 1, URL.lastIndexOf('.git')).toUpperCase()
}

/*
 * Returns a list of releases to build.
 */
def get_build_releases(releases_by_specs) {
    def build_releases = []

    // find releases to build based on given specs
    releases_by_specs.values().each { versions ->
        build_releases.addAll(versions)
    }

    return build_releases.unique()
}

/*
 * Returns a list of specs for a given version.
 */
def get_build_specs_by_release(version, specs) {
    def specs_by_release = []

    specs.each { build_spec, build_releases ->
        if (build_releases.contains(version)) {
            specs_by_release.add(build_spec)
        }
    }

    return specs_by_release
}

/**
 * Parses the build parameters to identify the specs and the releases to build
 * and test. Returns a map of releases by specs.
 */
def get_specs(SUPPORTED_SPECS) {
    def SPECS = [:]

    if (!RELEASES) {
        // get releases
        params.each { key, value ->
            if ((key.indexOf('Java') != -1) && (value == true)) {
                RELEASES.add(key.substring(4))
            }
        }
    }
    echo "RELEASES:'${RELEASES}'"

    if (!PLATFORMS) {
        // get specs
        if (!params.PLATFORMS || (params.PLATFORMS.trim() == 'all')) {
            // build and test all supported specs
            PLATFORMS.addAll(SUPPORTED_SPECS.keySet())
        } else {
            PLATFORMS.addAll(params.PLATFORMS.split(","))
        }
    }
    echo "PLATFORMS:'${PLATFORMS}'"
    def RELEASES_STR = RELEASES.join(',')

    // Setup SPEC map
    PLATFORMS.each { SPEC ->
        SPEC = SPEC.trim()
        if (!SUPPORTED_SPECS.keySet().contains(SPEC)) {
            echo "Warning: Unknown or unsupported platform: ${SPEC}"
            return
        }

        if (RELEASES) {
            // personal build
            SPEC_RELEASES = SUPPORTED_SPECS[SPEC].intersect(RELEASES)
         } else {
            SPEC_RELEASES = SUPPORTED_SPECS[SPEC]
         }

        if (SPEC_RELEASES) {
            // populate the releases by specs map
            SPECS[SPEC] = SPEC_RELEASES
        } else {
            echo "Warning: Selected Java versions (${RELEASES_STR}) for ${SPEC} is not supported(nothing to do)!"
        }
    }

    if (!SPECS) {
        error("Invalid PLATFORMS (${params.PLATFORMS}) and/or Java versions selections(${RELEASES_STR})!")
    }
    echo "SPECS:'${SPECS}'"
    return SPECS
}

/*
 * Set build extra options
 */
def set_build_extra_options(build_specs=null) {
    if (!build_specs) {
        buildspec = buildspec_manager.getSpec(SPEC)
        // single release
        EXTRA_GETSOURCE_OPTIONS = params.EXTRA_GETSOURCE_OPTIONS
        if (!EXTRA_GETSOURCE_OPTIONS) {
            EXTRA_GETSOURCE_OPTIONS = buildspec.getVectorField("extra_getsource_options", SDK_VERSION).join(" ")
        }

        EXTRA_CONFIGURE_OPTIONS = params.EXTRA_CONFIGURE_OPTIONS
        if (!EXTRA_CONFIGURE_OPTIONS) {
            EXTRA_CONFIGURE_OPTIONS = buildspec.getVectorField("extra_configure_options", SDK_VERSION).join(" ")
        }

        EXTRA_MAKE_OPTIONS = params.EXTRA_MAKE_OPTIONS
        if (!EXTRA_MAKE_OPTIONS) {
            EXTRA_MAKE_OPTIONS = buildspec.getVectorField("extra_make_options", SDK_VERSION).join(" ")
        }
        if (params.CODE_COVERAGE) {
           if (EXTRA_MAKE_OPTIONS.contains("EXTRA_CMAKE_ARGS=")) {
                EXTRA_MAKE_OPTIONS = EXTRA_MAKE_OPTIONS.replaceAll(/(-DCODE_COVERAGE=).*?(\"|\s|$)/, '$1ON$2')
                if (EXTRA_MAKE_OPTIONS.contains("EXTRA_CMAKE_ARGS=\"")) { // multiple arguments for EXTRA_CMAKE_ARGS="XX YY"
                    extracted_extra_cmake_args = EXTRA_MAKE_OPTIONS.find(/EXTRA_CMAKE_ARGS=\".*?\"/)
                    if(!extracted_extra_cmake_args.contains("-DCODE_COVERAGE=ON")) {
                        EXTRA_MAKE_OPTIONS = EXTRA_MAKE_OPTIONS.replace("EXTRA_CMAKE_ARGS=\"", "EXTRA_CMAKE_ARGS=\"-DCODE_COVERAGE=ON ")
                    }
                } else { // only one argument for EXTRA_CMAKE_ARGS=-D
                    if(!EXTRA_MAKE_OPTIONS.contains("EXTRA_CMAKE_ARGS=-DCODE_COVERAGE=ON")) {
                        sub_options = EXTRA_MAKE_OPTIONS.split(" ")
                        sub_options.eachWithIndex { sub_option, index ->
                            if (sub_option.contains("EXTRA_CMAKE_ARGS=")) {
                                sub_option = sub_option.replace("EXTRA_CMAKE_ARGS=", "EXTRA_CMAKE_ARGS=\"-DCODE_COVERAGE=ON ")
                                sub_options[index] = sub_option + "\""
                            }
                        }
                        EXTRA_MAKE_OPTIONS = sub_options.join(" ")
                    }
                }
            } else {
                EXTRA_MAKE_OPTIONS += " EXTRA_CMAKE_ARGS=-DCODE_COVERAGE=ON"
            }
        }

        OPENJDK_CLONE_DIR = params.OPENJDK_CLONE_DIR
        if (!OPENJDK_CLONE_DIR) {
            OPENJDK_CLONE_DIR = ''
        }
    } else {
        // multiple releases
        EXTRA_GETSOURCE_OPTIONS_MAP = [:]
        EXTRA_CONFIGURE_OPTIONS_MAP = [:]
        EXTRA_MAKE_OPTIONS_MAP = [:]
        OPENJDK_CLONE_DIR_MAP = [:]

        def build_releases = get_build_releases(build_specs)
        build_releases.each { release ->
            def extra_getsource_options_by_specs = [:]
            def extra_configure_options_by_specs = [:]
            def extra_make_options_by_specs = [:]
            def clone_dir_by_specs = [:]

            build_specs.each { spec, releases ->
                if (releases.contains(release)) {
                    ['EXTRA_GETSOURCE_OPTIONS', 'EXTRA_CONFIGURE_OPTIONS', 'EXTRA_MAKE_OPTIONS'].each { it ->
                        buildspec = buildspec_manager.getSpec(spec)
                        // look up extra options by release and spec/platform provided as build parameters:
                        // e.g. EXTRA_GETSOURCE_OPTIONS_JDK<release>_<spec>
                        //      EXTRA_CONFIGURE_OPTIONS_JDK<release>_<spec>
                        //      EXTRA_MAKE_OPTIONS_JDK<release>_<spec>
                        def extraOptions = params."${it}_JDK${release}_${spec}"
                        if (!extraOptions && buildspec) {
                            extraOptions = buildspec.getVectorField("${it.toLowerCase()}", release).join(" ")
                        }

                        if (extraOptions) {
                            // populate maps by specs
                            switch (it) {
                                case 'EXTRA_GETSOURCE_OPTIONS':
                                    extra_getsource_options_by_specs.put(spec, extraOptions)
                                    break
                                case 'EXTRA_CONFIGURE_OPTIONS':
                                    extra_configure_options_by_specs.put(spec, extraOptions)
                                    break
                                case 'EXTRA_MAKE_OPTIONS':
                                    extra_make_options_by_specs.put(spec, extraOptions)
                                    break
                            }
                        }
                    }

                    def cloneDir = params."OPENJDK${release}_CLONE_DIR_${spec}"
                    if (!cloneDir) {
                        cloneDir = params."OPENJDK${release}_CLONE_DIR"
                    }
                    if (cloneDir) {
                        clone_dir_by_specs.put(spec, cloneDir)
                    }
                }
            }

            // populate maps by releases and specs
            if (extra_getsource_options_by_specs) {
                EXTRA_GETSOURCE_OPTIONS_MAP.put(release, extra_getsource_options_by_specs)
            }
            if (extra_configure_options_by_specs) {
                EXTRA_CONFIGURE_OPTIONS_MAP.put(release, extra_configure_options_by_specs)
            }
            if (extra_make_options_by_specs) {
                EXTRA_MAKE_OPTIONS_MAP.put(release, extra_make_options_by_specs)
            }
            if (clone_dir_by_specs) {
                OPENJDK_CLONE_DIR_MAP.put(release, clone_dir_by_specs)
            }
        }
    }
}

/*
 * Set the Git repository URL and branch for the AdoptOpenJDK Testing material.
 */
def set_adoptopenjdk_tests_repository(build_releases=null) {
    // fetch from the variables file
    def adoptOpenJdkByReleaseMap = VARIABLES.adoptopenjdk.default

    ADOPTOPENJDK_REPO = params.ADOPTOPENJDK_REPO
    if (!ADOPTOPENJDK_REPO && adoptOpenJdkByReleaseMap) {
        ADOPTOPENJDK_REPO = adoptOpenJdkByReleaseMap.get('repoUrl')
    }

    ADOPTOPENJDK_BRANCH = params.ADOPTOPENJDK_BRANCH
    if (!ADOPTOPENJDK_BRANCH && adoptOpenJdkByReleaseMap) {
        ADOPTOPENJDK_BRANCH = adoptOpenJdkByReleaseMap.get('branch')
    }

    echo "Using ADOPTOPENJDK_REPO = ${ADOPTOPENJDK_REPO} ADOPTOPENJDK_BRANCH = ${ADOPTOPENJDK_BRANCH}"
}

// Creates a job using the job DSL plugin on Jenkins
def create_job(JOB_NAME, SDK_VERSION, SPEC, downstreamJobType, id) {

    // NOTE: The following 4 DEFAULT variables can be configured in the Jenkins Global Config.
    // This allows us to know what the default values are without being told explicitly.
    // These DEFAULT values are typically constant per Jenkins Server.
    if (!env.VARIABLE_FILE_DEFAULT) {
        VARIABLE_FILE_DEFAULT = ''
    }
    if (!env.VENDOR_REPO_DEFAULT) {
        VENDOR_REPO_DEFAULT = ''
    }
    if (!env.VENDOR_BRANCH_DEFAULT) {
        VENDOR_BRANCH_DEFAULT = ''
    }
    if (!env.VENDOR_CREDENTIALS_ID_DEFAULT) {
        VENDOR_CREDENTIALS_ID_DEFAULT = ''
    }

    // Configuring the build discarder in the downstream project

    DISCARDER_NUM_BUILDS = get_value(VARIABLES.build_discarder.logs, id)

    def params = [:]
    params.put('JOB_NAME', JOB_NAME)
    params.put('SDK_VERSION', SDK_VERSION)
    params.put('PLATFORM', SPEC)
    params.put('VARIABLE_FILE_DEFAULT', VARIABLE_FILE_DEFAULT)
    params.put('VENDOR_REPO_DEFAULT', VENDOR_REPO_DEFAULT)
    params.put('VENDOR_BRANCH_DEFAULT', VENDOR_BRANCH_DEFAULT)
    params.put('VENDOR_CREDENTIALS_ID_DEFAULT', VENDOR_CREDENTIALS_ID_DEFAULT)
    params.put('jobType', downstreamJobType)
    params.put('DISCARDER_NUM_BUILDS', DISCARDER_NUM_BUILDS)
    params.put('SCM_REPO', SCM_REPO)
    params.put('SCM_BRANCH', SCM_BRANCH)
    if (USER_CREDENTIALS_ID) {
        params.put('USER_CREDENTIALS_ID', USER_CREDENTIALS_ID)
    }

    def templatePath = 'buildenv/jenkins/jobs/pipelines/Pipeline_Template.groovy'
    pipelineFunctions.retry_and_delay(
        { jobDsl targets: templatePath, ignoreExisting: false, additionalParameters: params },
        3, 120)
}

def set_build_variables_per_node() {
    def bootJDKVersion = buildspec.getScalarField('boot_jdk.version', SDK_VERSION)
    def bootJDKPath = buildspec.getScalarField('boot_jdk.location', SDK_VERSION)
    BOOT_JDK = "${bootJDKPath}/jdk${bootJDKVersion}"
    println("BOOT_JDK: ${BOOT_JDK}")
    if (!check_path("${BOOT_JDK}/bin/java")) {
        echo "BOOT_JDK: ${BOOT_JDK} does not exist! Attempt to download it..."
        download_boot_jdk(bootJDKVersion, BOOT_JDK)
    }

    FREEMARKER = buildspec.getScalarField('freemarker', SDK_VERSION)
    if (FREEMARKER) {
        println("FREEMARKER: ${FREEMARKER}")
        if (!check_path(FREEMARKER)) {
            error("FREEMARKER: ${FREEMARKER} does not exist!")
        }
    }

    OPENJDK_REFERENCE_REPO = buildspec.getScalarField("openjdk_reference_repo", SDK_VERSION)
    println("OPENJDK_REFERENCE_REPO: ${OPENJDK_REFERENCE_REPO}")
    if (!check_path(OPENJDK_REFERENCE_REPO)) {
        println("The git cache OPENJDK_REFERENCE_REPO: ${buildspec.getScalarField('openjdk_reference_repo', SDK_VERSION)} does not exist on ${NODE_NAME}!")
    }

    if (SPEC.contains('win')) {
        echo "Check for OpenSSL install..."
        def match = (EXTRA_CONFIGURE_OPTIONS =~ /--with-openssl=(\S+)\b/)
        if (!match.find()) {
            echo "No '--with-openssl' option found."
        } else {
            def opensslLocation = match.group(1)
            if (opensslLocation.equals('fetched') || opensslLocation.equals('system')) {
                echo "Using ${opensslLocation} OpenSSL"
            } else if (check_path("${opensslLocation}")) {
                echo "OpenSSL found at ${opensslLocation}"
            } else {
                echo "Downloading OpenSSL..."
                def opensslVersion = opensslLocation.substring(opensslLocation.lastIndexOf('/') + 1)
                def opensslParentFolder = opensslLocation.substring(0, opensslLocation.lastIndexOf('/'))
                dir('openssl') {
                    sh """
                        curl -Ok ${JENKINS_URL}userContent/${opensslVersion}.zip
                        unzip ${opensslVersion}.zip
                        rm ${opensslVersion}.zip
                        mkdir -p ${opensslParentFolder}
                        mv ${opensslVersion} ${opensslParentFolder}/
                    """
                }
                cleanWs()
            }
        }
    }

    FAIL_PATTERN = params.FAIL_PATTERN ?: buildspec.getScalarField('fail_pattern', SDK_VERSION)
}

def check_path(inPath) {
    if (!inPath) {
        return false
    }
    return sh (script: "test -e ${inPath} && echo true || echo false", returnStdout: true).trim().toBoolean()
}

def download_boot_jdk(bootJDKVersion, bootJDK) {
    def os = buildspec.getScalarField('boot_jdk', 'os')
    def arch = buildspec.getScalarField('boot_jdk', 'arch')
    def dirStrip = buildspec.getScalarField('boot_jdk.dir_strip', SDK_VERSION)
    def sdkUrl = buildspec.getScalarField('boot_jdk.url', bootJDKVersion)

    if (sdkUrl.contains('$')) {
        // Add all variables that could be used in a template
        def binding = ["os":os, "arch":arch, "bootJDKVersion":bootJDKVersion]
        def engine = new groovy.text.SimpleTemplateEngine()
        sdkUrl = engine.createTemplate(sdkUrl).make(binding)
    }
    /*
     * Download bootjdk
     * Windows are zips from Adopt. Unzip doesn't have strip dir so we have to manually move.
     * Remaining platforms are tarballs.
     * Mac has some extra dirs to strip off hence $dirStrip.
     */
    dir('bootjdk') {
        sh """
            curl -LJkO ${sdkUrl}
            mkdir -p ${bootJDK}
            sdkFile=`ls | grep semeru`
            if [[ "\$sdkFile" == *zip ]]; then
                unzip "\$sdkFile" -d .
                sdkFolder=`ls -d */`
                mv "\$sdkFolder"* ${bootJDK}/
                rm -r "\$sdkFolder"
            else
                gzip -cd "\$sdkFile" | tar xof - -C ${bootJDK} --strip=${dirStrip}
            fi
            ${bootJDK}/bin/java -version
        """
    }
    buildFile.cleanWorkspace(false)
}

def set_build_custom_options() {
    if (customFile) {
        customFile.set_extra_options()
    }
}

return this
