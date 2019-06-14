/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

public class TestKitGen {
    private static final String usage = "Searches `projectRootDir` specified to find/parse playlist.xml to generate a makefile per project.\n\n"
            + "Usage:\n"
            + "    java TestKitGen --graphSpecs=[linux_x86-64|linux_x86-64_cmprssptrs|...] --jdkVersion=[8|9|...] --impl=[openj9|ibm|hotspot|sap] [options]\n\n"
            + "Options:\n" + "    --graphSpecs=<specs>      Comma separated specs that the build will run on\n"
            + "    --jdkVersion=<version>    JDK version that the build will run on, e.g. 8, 9, 10, etc.\n"
            + "    --impl=<implementation>   Java implementation, e.g. openj9, ibm, hotspot, sap\n"
            + "    --projectRootDir=<path>   Root path for searching playlist.xml\n"
            + "                              Defaults to openj9/test\n"
            + "    --output=<path>           Path to output makefiles\n"
            + "                              Defaults to projectRootDir\n"
            + "    --modesXml=<path>         Path to the modes.xml file\n"
            + "                              Defaults to projectRootDir/TestConfig/resources\n"
            + "    --ottawaCsv=<path>        Path to the ottawa.csv file\n"
            + "                              Defaults to projectRootDir/TestConfig/resources\n"
            + "    --buildList=<paths>       Comma separated project paths (relative to projectRootDir) to search for playlist.xml\n"
            + "                              Defaults to projectRootDir\n"
            + "    --iterations=<number>     Repeatedly generate test command based on iteration number\n"
            + "                              Defaults to 1\n"
            + "    --testFlag=<string>       Comma separated string to specify different test flags\n"
            + "                              Defaults to \"\"\n";

    private static final String headerComments = "########################################################\n"
            + "# This is an auto generated file. Please do NOT modify!\n"
            + "########################################################\n\n";

    private static final String suffixForTesting = ".tkgj";
    private static final String mkName = "autoGen.mk";
    private static final String dependmk = "dependencies.mk";
    private static final String utilsmk = "utils.mk";
    private static final String countmk = "count.mk";
    private static final String settings = "settings.mk";

    private static final List<String> allGroups = Arrays.asList("functional", "openjdk", "external", "perf", "jck",
            "system");
    private static final List<String> allImpls = Arrays.asList("openj9", "ibm", "hotspot", "sap");
    private static final List<String> allLevels = Arrays.asList("sanity", "extended", "special");
    private static final List<String> allTypes = Arrays.asList("regular", "native");

    private static final HashMap<String, String> sp_hs = new HashMap<String, String>();
    private static final HashMap<String, Integer> targetCount = new HashMap<String, Integer>();

    private static Document modes = null;

    private static final Map<String, String> options = new HashMap<>();

    public static void main(String[] args) throws Exception {
        for (int i = 0; i < args.length; i++) {
            String arg = args[i];

            int indexOfDashes = arg.indexOf("--");
            int indexOfEquals = arg.indexOf("=");
            if (indexOfDashes == 0 && indexOfEquals > 1) {
                options.put(arg.substring(indexOfDashes + 2, indexOfEquals), arg.substring(indexOfEquals + 1));
            } else {
                System.err.println("Error: Invalid option " + arg + " specified");
                System.err.println();
                System.err.println(usage);
                System.exit(1);
            }
        }

        if (options.get("projectRootDir") == null) {
            File projectRootDir = new File(Paths.get(".").toAbsolutePath().normalize().toString() + "/../../..");

            if (projectRootDir.exists() && projectRootDir.isDirectory()) {
                System.out.println("Using projectRootDir: " + projectRootDir + "\n");
                options.put("projectRootDir", projectRootDir.getPath());
            } else {
                System.err
                        .println("Error: Please specify a valid project directory using option \"--projectRootDir=\"");
                System.err.println();
                System.err.println(usage);
                System.exit(1);
            }
        }

        if (options.get("graphSpecs") == null) {
            System.err.println("Error: Please specify graphSpecs");
            System.err.println();
            System.err.println(usage);
            System.exit(1);
        }

        if (options.get("jdkVersion") == null) {
            System.err.println("Error: Please specify jdkVersion");
            System.err.println();
            System.err.println(usage);
            System.exit(1);
        }

        if (options.get("impl") == null) {
            System.err.println("Error: Please specify impl");
            System.err.println();
            System.err.println(usage);
            System.exit(1);
        }

        if (options.get("output") == null) {
            options.put("output", options.get("projectRootDir"));
        }

        if (options.get("modesXml") == null) {
            options.put("modesXml", options.get("projectRootDir") + "/TestConfig/resources/modes.xml");
        }

        if (options.get("ottawaCsv") == null) {
            options.put("ottawaCsv", options.get("projectRootDir") + "/TestConfig/resources/ottawa.csv");
        }

        if (options.get("buildList") == null) {
            options.put("buildList", options.get("projectRootDir"));
        }

        if (options.get("iterations") == null) {
            options.put("iterations", "1");
        }

        if (options.get("testFlag") == null) {
            options.put("testFlag", "");
        }

        System.out.println("Getting modes data from modes.xml and ottawa.csv...");

        modes = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(options.get("modesXml"));

        processModes(modes.getDocumentElement());

        runmkgen();

        System.out.println("\nTEST AUTO GEN SUCCESSFUL\n");
    }

    public static void runmkgen() throws Exception {
        if (!options.get("output").isEmpty()) {
            File f = new File(options.get("output") + "/TestConfig");
            f.mkdir();
        }

        // Initialize counting map targetCount
        targetCount.put("all", 0);

        List<String> allDisHead = Arrays.asList("", "disabled.", "echo.disabled.");

        for (String eachDisHead : allDisHead) {
            for (String eachLevel : allLevels) {
                if (!targetCount.containsKey(eachDisHead + eachLevel)) {
                    targetCount.put(eachDisHead + eachLevel, 0);
                }

                for (String eachGroup : allGroups) {
                    if (!targetCount.containsKey(eachDisHead + eachGroup)) {
                        targetCount.put(eachDisHead + eachGroup, 0);
                    }

                    String lgKey = eachLevel + '.' + eachGroup;
                    if (!targetCount.containsKey(eachDisHead + lgKey)) {
                        targetCount.put(eachDisHead + lgKey, 0);
                    }

                    for (String eachType : allTypes) {
                        if (!targetCount.containsKey(eachDisHead + eachType)) {
                            targetCount.put(eachDisHead + eachType, 0);
                        }

                        String ltKey = eachLevel + '.' + eachType;
                        if (!targetCount.containsKey(eachDisHead + ltKey)) {
                            targetCount.put(eachDisHead + ltKey, 0);
                        }

                        String gtKey = eachGroup + '.' + eachType;
                        if (!targetCount.containsKey(eachDisHead + gtKey)) {
                            targetCount.put(eachDisHead + gtKey, 0);
                        }

                        String lgtKey = eachLevel + '.' + eachGroup + '.' + eachType;
                        targetCount.put(eachDisHead + lgtKey, 0);
                    }
                }
            }
        }

        generateOnDir(new ArrayList<String>());
        dependGen();
        utilsGen();
        countGen();
    }

    public static int generateOnDir(ArrayList<String> currentdirs) throws Exception {
        String absolutedir = options.get("projectRootDir");
        String currentdir = String.join("/", currentdirs);
        if (!currentdirs.isEmpty()) {
            absolutedir = absolutedir + '/' + currentdir;
        }

        // Only generate make files for projects that are specificed in the build list.
        // If the build list is empty then generate on every project.
        if (!options.get("buildList").isEmpty()) {
            boolean inList = false;
            String[] buildListArr = options.get("buildList").split(",");
            for (String build : buildListArr) {
                build = build.replaceAll("\\+", "/");
                if ((currentdir.indexOf(build) == 0) || (build.indexOf(currentdir) == 0)) {
                    inList = true;
                    break;
                }
            }
            if (!inList) {
                return 0;
            }
        }

        File playlistXML = null;
        List<String> subdirsHavePlaylist = new ArrayList<String>();

        File directory = new File(absolutedir);
        File[] dir = directory.listFiles();

        for (File entry : dir) {
            if (entry.getName().equals(".") || entry.getName().equals("..")) {
                continue;
            }

            boolean tempExclude = false;

            // Temporary exclusion, remove this block when JCL_VERSION separation is removed
            if ((options.get("jdkVersion") != "Panama") && (options.get("jdkVersion") != "Valhalla")) {
                String JCL_VERSION = "";
                if (System.getenv("JCL_VERSION") != null) {
                    JCL_VERSION = System.getenv("JCL_VERSION");
                } else {
                    JCL_VERSION = "latest";
                }

                // Temporarily exclude projects for CCM build (i.e., when JCL_VERSION is latest)
                String latestDisabledDir = "proxyFieldAccess Panama";

                // Temporarily exclude SVT_Modularity tests from integration build where we are
                // still using b148 JCL level
                String currentDisableDir = "SVT_Modularity OpenJ9_Jsr_292_API";

                tempExclude = ((JCL_VERSION.equals("latest")) && (latestDisabledDir.indexOf(entry.getName()) != -1))
                        || ((JCL_VERSION.equals("current")) && (currentDisableDir.indexOf(entry.getName()) != -1));
            }

            if (!tempExclude) {
                File projectDir = new File(absolutedir + '/' + entry.getName());

                if (projectDir.isFile() && entry.getName().equals("playlist.xml")) {
                    playlistXML = projectDir;
                } else if (projectDir.isDirectory()) {
                    currentdirs.add(entry.getName());
                    if (generateOnDir(currentdirs) == 1) {
                        subdirsHavePlaylist.add(entry.getName());
                    }
                    currentdirs.remove(currentdirs.size() - 1);
                }
            }
        }

        return generateMk(playlistXML, absolutedir, currentdirs, subdirsHavePlaylist);
    }

    private static int generateMk(File playlistXML, String absolutedir, ArrayList<String> currentdirs,
            List<String> subdirsHavePlaylist) throws Exception {
        // Construct makefile path
        String makeFile = absolutedir + "/" + mkName;

        if (!options.get("output").isEmpty()) {
            String outputdir = options.get("output");

            if (!currentdirs.isEmpty()) {
                outputdir = outputdir + '/' + String.join("/", currentdirs);
            }

            File f = new File(outputdir);
            f.mkdir();

            makeFile = outputdir + "/" + mkName;
        }

        int rt = 0;

        if (playlistXML != null || !subdirsHavePlaylist.isEmpty()) {
            writeVars(makeFile, subdirsHavePlaylist, currentdirs);

            if (!subdirsHavePlaylist.isEmpty()) {
                rt = 1;
            }

            if (playlistXML != null) {
                rt |= xml2mk(makeFile, playlistXML, currentdirs, subdirsHavePlaylist);
            }
        }

        return rt;
    }

    private static int xml2mk(String makeFile, File playlistXML, ArrayList<String> currentdirs,
            List<String> subdirsHavePlaylist) {
        try {
            Document xml = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(playlistXML);

            processPlaylist(xml.getDocumentElement());

            writeTargets(makeFile, xml, currentdirs);
        } catch (SAXException | IOException | ParserConfigurationException e) {
            return 0;
        }

        return 1;
    }

    private static void processModes(Element modes) throws IOException {
        ArrayList<String> specs = new ArrayList<String>();
        ArrayList<String> capabilities = new ArrayList<String>();
        int lineNum = 0;

        List<String> lines = Files.newBufferedReader(Paths.get(options.get("ottawaCsv"))).lines()
                .collect(Collectors.toList());

        for (String line : lines) {
            String[] fields = line.split(",");

            // Since the spec line has an empty title, we cannot do string match. We assume
            // the second line is spec.
            if (lineNum++ == 1) {
                specs.addAll(Arrays.asList(fields));
            } else if (fields[0].equals("plat")) {
                for (int i = 1; i < fields.length; i++) {
                    sp_hs.put(specs.get(i), fields[i]);
                }
            } else if (fields[0].equals("capabilities")) {
                capabilities.addAll(Arrays.asList(fields));
            } else if (fields[0].startsWith("variation:")) {
                String modeNum = fields[0].substring("variation:".length());

                // Remove string Mode if it exists
                modeNum = modeNum.replace("Mode", "");

                NodeList modesList = modes.getElementsByTagName("mode");

                for (int i = 0; i < modesList.getLength(); i++) {
                    Element mode = (Element) modesList.item(i);

                    if (mode.getAttribute("number").equals(modeNum)) {
                        ArrayList<String> invalidSpecs = new ArrayList<String>();
                        for (int j = 1; j < fields.length; j++) {
                            if (fields[j].equals("no")) {
                                invalidSpecs.add(specs.get(j));
                            }
                        }

                        Set<String> ignoredSpecs = Arrays.asList("linux_x86-32_hrt", "linux_x86-64_cmprssptrs_gcnext",
                                "linux_ppc_purec", "linux_ppc-64_purec", "linux_ppc-64_cmprssptrs_purec",
                                "linux_x86-64_cmprssptrs_cloud").stream().collect(Collectors.toSet());

                        // Filter ignoredSpecs from invalidSpecs array
                        invalidSpecs = new ArrayList<String>(invalidSpecs.stream()
                                .filter(c -> !ignoredSpecs.contains(c)).collect(Collectors.toList()));

                        if (invalidSpecs.size() == 0) {
                            invalidSpecs.add("none");
                        }

                        Element invalidSpecsElement = mode.getOwnerDocument().createElement("invalidSpecs");
                        invalidSpecsElement.setTextContent(String.join(" ", invalidSpecs));

                        // Update invalidSpecs values
                        mode.appendChild(invalidSpecsElement);
                        break;
                    }
                }
            }
        }
    }

    private static void processPlaylist(Node node) {
        NodeList childNodes = node.getChildNodes();

        for (int i = 0; i < childNodes.getLength(); i++) {
            Node currentNode = childNodes.item(i);

            switch (currentNode.getNodeType()) {
            case Node.ELEMENT_NODE:
                Element currentElement = ((Element) currentNode);

                boolean removeTest = false;

                if (currentNode.getNodeName().equals("test")) {
                    // Do not generate make taget if impl doesn't match the exported impl
                    NodeList impls = currentElement.getElementsByTagName("impl");

                    boolean isValidImpl = impls.getLength() == 0;

                    for (int j = 0; j < impls.getLength(); j++) {
                        if (impls.item(j).getTextContent().trim().equals(options.get("impl"))) {
                            isValidImpl = true;
                            break;
                        }
                    }

                    removeTest |= !isValidImpl;

                    // Do not generate make target if subset doesn't match the exported jdk_version
                    NodeList subsets = currentElement.getElementsByTagName("subset");

                    boolean isValidSubset = subsets.getLength() == 0;

                    for (int j = 0; j < subsets.getLength(); j++) {
                        String subset = subsets.item(j).getTextContent().trim();

                        if (subset.equals(options.get("jdkVersion"))) {
                            isValidSubset = true;
                            break;
                        } else {
                            try {
                                Pattern pattern = Pattern.compile("^(.*)\\+$");
                                Matcher matcher = pattern.matcher(subset);

                                if (matcher.matches()) {
                                    if (Integer.parseInt(matcher.group(1)) <= Integer.parseInt(options.get("jdkVersion"))) {
                                        isValidSubset = true;
                                        break;
                                    }
                                }
                            } catch (NumberFormatException e) {
                                // Nothing to do
                            }
                        }
                    }

                    removeTest |= !isValidSubset;

                    // Do not generate make taget if the test is AOT not applicable when test flag
                    // is set to AOT
                    NodeList aot = currentElement.getElementsByTagName("aot");

                    if (aot.getLength() > 0 && options.get("testFlag").contains("AOT")) {
                        removeTest |= aot.item(0).getTextContent().trim().equals("nonapplicable");
                    }

                    if (removeTest) {
                        currentNode.getParentNode().removeChild(currentNode);
                    }

                    Document xml = currentNode.getOwnerDocument();

                    // variation defaults to "NoOptions"
                    if (currentElement.getElementsByTagName("variation").getLength() == 0) {
                        Element variations = xml.createElement("variations");
                        Element variation = xml.createElement("variation");
                        variation.setTextContent("NoOptions");
                        variations.appendChild(variation);
                        currentElement.appendChild(variations);
                    }

                    // level defaults to "extended"
                    if (currentElement.getElementsByTagName("level").getLength() == 0) {
                        Element levels = xml.createElement("levels");
                        Element level = xml.createElement("level");
                        level.setTextContent("extended");
                        levels.appendChild(level);
                        currentElement.appendChild(levels);
                    }

                    // group defaults to "functional"
                    if (currentElement.getElementsByTagName("group").getLength() == 0) {
                        Element groups = xml.createElement("groups");
                        Element group = xml.createElement("group");
                        group.setTextContent("functional");
                        groups.appendChild(group);
                        currentElement.appendChild(groups);
                    }

                    // type defaults to "regular"
                    if (currentElement.getElementsByTagName("type").getLength() == 0) {
                        Element types = xml.createElement("types");
                        Element type = xml.createElement("type");
                        type.setTextContent("regular");
                        types.appendChild(type);
                        currentElement.appendChild(types);
                    }

                    // impl defaults to all
                    if (impls.getLength() == 0) {
                        Element implsElement = xml.createElement("impls");

                        for (String impl : allImpls) {
                            Element implElement = xml.createElement("impl");
                            implElement.setTextContent(impl);
                            implsElement.appendChild(implElement);
                        }

                        currentElement.appendChild(implsElement);
                    }

                    // aot defaults to "applicable" when testFlag contains AOT
                    if (aot.getLength() == 0 && options.get("testFlag").contains("AOT")) {
                        Element aots = xml.createElement("aot");
                        aots.setTextContent("applicable");
                        currentElement.appendChild(aots);
                    }
                }

                processPlaylist(currentNode);
                break;

            default:
                break;
            }

            if (currentNode.getNodeType() == Node.ELEMENT_NODE) {
                processPlaylist(currentNode);
            }
        }
    }

    private static void writeVars(String makeFile, List<String> subdirsHavePlaylist, ArrayList<String> currentdirs)
            throws Exception {
        System.out.println();
        System.out.println("Generating make file " + makeFile);

        FileWriter f = new FileWriter(makeFile + suffixForTesting);

        String realtiveRoot = "";
        int subdirlevel = currentdirs.size();
        if (subdirlevel == 0) {
            realtiveRoot = ".";
        } else {
            for (int i = 1; i <= subdirlevel; i++) {
                realtiveRoot = realtiveRoot + ((i == subdirlevel) ? ".." : "..$(D)");
            }
        }

        f.write(headerComments + "\n");
        f.write("D=/\n\n");
        f.write("ifndef TEST_ROOT\n");

        // TODO: This needs to be removed before final delivery. It is used so we can
        // get byte-for-byte identical files to what the perl version generates.
        // The paths on Windows are generated using cygwin and instead of being
        // "C:\Users\bla" on the perl version they're "/cygdrive/c/Users/bla". The
        // following block of code translates from the former to the latter on Windows.
        if (options.get("graphSpecs").contains("win")) {
            String projectRootDir = options.get("output").replace("C:", "/cygdrive/c")
                    .replace("c:", "/cygdrive/c").replace('\\', '/');

            f.write("\tTEST_ROOT := " + projectRootDir + "\n");
        } else {
            f.write("\tTEST_ROOT := " + options.get("output") + "\n");
        }

        f.write("endif\n\n");
        f.write("SUBDIRS = " + String.join(" ", subdirsHavePlaylist) + "\n\n");
        f.write("include $(TEST_ROOT)$(D)TestConfig$(D)" + settings + "\n\n");

        f.close();
    }

    private static void writeTargets(String makeFile, Document xml, ArrayList<String> currentdirs) throws IOException {
        FileWriter f = new FileWriter(makeFile + suffixForTesting, true);

        HashMap<String, ArrayList<String>> groupTargets = new HashMap<String, ArrayList<String>>();

        NodeList includes = xml.getElementsByTagName("include");

        if (includes.getLength() > 0) {
            f.write("-include " + includes.item(0).getTextContent() + "\n\n");
        }

        f.write("include $(TEST_ROOT)$(D)TestConfig$(D)" + dependmk + "\n\n");

        NodeList tests = xml.getElementsByTagName("test");

        for (int i = 0; i < tests.getLength(); i++) {
            Element test = (Element) tests.item(i);

            NodeList testCaseNames = test.getElementsByTagName("testCaseName");

            int count = 0;
            ArrayList<String> subtests = new ArrayList<String>();
            int testIterations = Integer.parseInt(options.get("iterations"));

            NodeList variations = test.getElementsByTagName("variation");

            for (int j = 0; j < variations.getLength(); j++) {
                Element variation = (Element) variations.item(j);

                String jvmoptions = " " + variation.getTextContent().trim() + " ";
                jvmoptions = jvmoptions.replaceAll(" NoOptions ", "");

                ArrayList<String> invalidSpecs_hs = new ArrayList<String>();
                Pattern pattern = Pattern.compile(" Mode([^ ]*?) .*");
                Matcher matcher = pattern.matcher(jvmoptions);

                if (matcher.matches()) {
                    // TODO: Can we really have more than one mode in a variation? I presume not,
                    // but I'm not 100% certain
                    for (int k = 1; k <= matcher.groupCount(); k++) {
                        String clArgs = getModeClArgs(matcher.group(k));
                        jvmoptions = jvmoptions.replace("Mode" + matcher.group(k), clArgs);

                        String invalidSpecs = getModeInvalidSpecs(matcher.group(k));
                        invalidSpecs_hs.add(invalidSpecs);
                    }
                }

                jvmoptions = jvmoptions.trim();

                invalidSpecs_hs.sort(null);
                String invalidSpecs = String.join(" ", invalidSpecs_hs).trim();

                NodeList platformRequirements = test.getElementsByTagName("platformRequirements");
                ArrayList<String> allInvalidSpecs = getAllInvalidSpecs(test, invalidSpecs, platformRequirements.item(0),
                        options.get("graphSpecs"));

                NodeList capabilities = test.getElementsByTagName("capabilities");
                String[] capabilityReqs_Arr = null;
                HashMap<String, String> capabilityReqs_Hash = new HashMap<String, String>();
                if (capabilities.getLength() > 0) {
                    capabilityReqs_Arr = capabilities.item(0).getTextContent().split(",");
                    for (String capabilityReq : capabilityReqs_Arr) {
                        String capabilityReqTrimmed = capabilityReq.trim();
                        String[] colonSplit = capabilityReqTrimmed.split(":");
                        capabilityReqs_Hash.put(colonSplit[0], colonSplit[1]);
                    }
                }

                // Generate make target
                String name = testCaseNames.item(0).getTextContent() + "_" + count;
                subtests.add(name);

                String condition_platform = null;
                if (allInvalidSpecs.size() > 0) {
                    String string = String.join(" ", allInvalidSpecs);
                    condition_platform = name + "_INVALID_PLATFORM_CHECK";
                    f.write(condition_platform + "=$(filter " + string + ", $(SPEC))\n");
                }

                if (capabilityReqs_Arr != null) {
                    List<String> capabilityReqs_HashKeys = new ArrayList<>(capabilityReqs_Hash.keySet());
                    Collections.sort(capabilityReqs_HashKeys);

                    for (String capa_key : capabilityReqs_HashKeys) {
                        String condition_capsReqs = name + "_" + capa_key + "_CHECK";
                        f.write(condition_capsReqs + "=$(" + capa_key + ")\n");
                    }
                }

                String jvmtestroot = "$(JVM_TEST_ROOT)$(D)" + String.join("$(D)", currentdirs);
                f.write(name + ": TEST_RESROOT=" + jvmtestroot + "\n");

                String aotOptions = "";
                // AOT_OPTIONS only needs to be appended when TEST_FLAG contains AOT and the
                // test is AOT applicable
                NodeList aot = test.getElementsByTagName("aot");
                if (aot.getLength() > 0 && options.get("testFlag").contains("AOT")) {
                    if (aot.item(0).getTextContent().equals("applicable")) {
                        aotOptions = "$(AOT_OPTIONS) ";
                    } else if (aot.item(0).getTextContent().equals("explicit")) {
                        // When test tagged with AOT explicit, its test command has AOT options and runs
                        // multiple times explicitly
                        testIterations = 1;
                        f.write(name + ": TEST_ITERATIONS=1\n");
                    }
                }

                if (!jvmoptions.isEmpty()) {
                    f.write(name + ": JVM_OPTIONS?=" + aotOptions + "$(RESERVED_OPTIONS) " + jvmoptions
                            + " $(EXTRA_OPTIONS)\n");
                } else {
                    f.write(name + ": JVM_OPTIONS?=" + aotOptions + "$(RESERVED_OPTIONS) $(EXTRA_OPTIONS)\n");
                }

                String levelStr = "";
                NodeList levels = test.getElementsByTagName("level");
                for (int k = 0; k < levels.getLength(); k++) {
                    if (!levelStr.isEmpty()) {
                        levelStr = levelStr + ",";
                    }

                    levelStr = levelStr + "level." + levels.item(k).getTextContent();
                }

                f.write(name + ": TEST_GROUP=" + levelStr + "\n");
                String indent = "\t";
                f.write(name + ":\n");
                f.write(indent + "@echo \"\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                f.write(indent
                        + "@echo \"===============================================\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                f.write(indent + "@echo \"Running test $@ ...\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                f.write(indent
                        + "@echo \"===============================================\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                f.write(indent + "@perl '-MTime::HiRes=gettimeofday' -e 'print \"" + name
                        + " Start Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"' | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");

                NodeList disabled = test.getElementsByTagName("disabled");
                if (disabled.getLength() > 0) {
                    // This line is also the key words to match runningDisabled
                    f.write(indent
                            + "@echo \"Test is disabled due to:\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                    String[] disabledReasons = disabled.item(0).getTextContent().split("[\t\n]");
                    for (String dReason : disabledReasons) {
                        if (!dReason.isEmpty()) {
                            f.write(indent + "@echo \"" + dReason
                                    + "\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                        }
                    }
                }
                if (condition_platform != null) {
                    f.write("ifeq ($(" + condition_platform + "),)\n");
                }
                if (capabilities.getLength() > 0) {
                    for (Map.Entry<String, String> entry : capabilityReqs_Hash.entrySet()) {
                        String condition_capsReqs = name + "_" + entry.getKey() + "_CHECK";
                        f.write("ifeq ($(" + condition_capsReqs + "), " + entry.getValue() + ")\n");
                    }
                }

                f.write(indent + "$(TEST_SETUP);\n");

                f.write(indent + "@echo \"variation: " + variation.getTextContent()
                        + "\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                f.write(indent
                        + "@echo \"JVM_OPTIONS: $(JVM_OPTIONS)\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");

                NodeList command = test.getElementsByTagName("command");

                f.write(indent + "{ ");
                for (int k = 1; k <= testIterations; k++) {
                    f.write("itercnt=" + k + "; \\\n" + indent + "$(MKTREE) $(REPORTDIR); \\\n" + indent
                            + "$(CD) $(REPORTDIR); \\\n");
                    f.write(indent + command.item(0).getTextContent().trim() + ";");
                    if (k != testIterations) {
                        f.write(" \\\n" + indent);
                    }
                }
                f.write(" } 2>&1 | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");

                f.write(indent + "$(TEST_TEARDOWN);\n");

                if (capabilities.getLength() > 0) {
                    for (Map.Entry<String, String> entry : capabilityReqs_Hash.entrySet()) {
                        f.write("else\n");
                        f.write(indent + "@echo \"Skipped due to capabilities (" + capabilities.item(0).getTextContent()
                                + ") => $(TEST_SKIP_STATUS)\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                        f.write("endif\n");
                    }
                }
                if (condition_platform != null) {
                    f.write("else\n");
                    if (platformRequirements.getLength() > 0) {
                        f.write(indent
                                + "@echo \"Skipped due to jvm options ($(JVM_OPTIONS)) and/or platform requirements ("
                                + platformRequirements.item(0).getTextContent()
                                + ") => $(TEST_SKIP_STATUS)\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                    } else {
                        f.write(indent
                                + "@echo \"Skipped due to jvm options ($(JVM_OPTIONS)) => $(TEST_SKIP_STATUS)\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                    }
                    f.write("endif\n");
                }
                f.write(indent + "@perl '-MTime::HiRes=gettimeofday' -e 'print \"" + name
                        + " Finish Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"' | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q)\n");

                f.write("\n.PHONY: " + name + "\n\n");

                NodeList groups = test.getElementsByTagName("group");
                for (int k = 0; k < groups.getLength(); k++) {
                    String eachGroup = groups.item(k).getTextContent();
                    for (int l = 0; l < levels.getLength(); l++) {
                        String eachLevel = levels.item(l).getTextContent();
                        NodeList types = test.getElementsByTagName("type");
                        for (int m = 0; m < types.getLength(); m++) {
                            String eachType = types.item(m).getTextContent();
                            if (disabled.getLength() == 0) {
                                String lgtKey = eachLevel + '.' + eachGroup + '.' + eachType;
                                groupTargets.putIfAbsent(lgtKey, new ArrayList<String>());
                                groupTargets.get(lgtKey).add(name);
                            } else {
                                String lgtKey = eachLevel + '.' + eachGroup + '.' + eachType;
                                String dlgtKey = "disabled." + lgtKey;
                                String echodlgtKey = "echo." + dlgtKey;
                                groupTargets.putIfAbsent(dlgtKey, new ArrayList<String>());
                                groupTargets.get(dlgtKey).add(name);
                                groupTargets.putIfAbsent(echodlgtKey, new ArrayList<String>());
                                groupTargets.get(echodlgtKey).add("echo.disabled." + name);
                                f.write("echo.disabled." + name + ":\n");
                                f.write(indent + "@echo \"\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                                f.write(indent
                                        + "@echo \"===============================================\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                                f.write(indent + "@echo \"Running test " + name
                                        + " ...\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                                f.write(indent
                                        + "@echo \"===============================================\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                                f.write(indent + "@perl '-MTime::HiRes=gettimeofday' -e 'print \"" + name
                                        + " Start Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"' | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                                f.write(indent + "@echo \"" + name
                                        + "_DISABLED\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                                f.write(indent + "@echo \"Disabled Reason:\"\n");

                                String[] disabledReasons = disabled.item(0).getTextContent().split("[\t\n]");
                                for (String dReason : disabledReasons) {
                                    if (!dReason.isEmpty()) {
                                        f.write(indent + "@echo \"" + dReason
                                                + "\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
                                    }
                                }

                                f.write(indent + "@perl '-MTime::HiRes=gettimeofday' -e 'print \"" + name
                                        + " Finish Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"' | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q)\n");
                                f.write("\n.PHONY: echo.disabled." + name + "\n\n");
                            }
                        }
                    }
                }

                count++;
            }

            f.write(testCaseNames.item(0).getTextContent() + ":");
            for (String subTest : subtests) {
                f.write(" \\\n" + subTest);
            }
            f.write("\n\n.PHONY: " + testCaseNames.item(0).getTextContent() + "\n\n");
        }

        List<String> allDisHead = Arrays.asList("", "disabled.", "echo.disabled.");

        allLevels.sort(null);
        allGroups.sort(null);
        allTypes.sort(null);

        for (String eachDisHead : allDisHead) {
            for (String eachLevel : allLevels) {
                for (String eachGroup : allGroups) {
                    for (String eachType : allTypes) {
                        String lgtKey = eachLevel + '.' + eachGroup + '.' + eachType;
                        String hlgtKey = eachDisHead + eachLevel + '.' + eachGroup + '.' + eachType;
                        groupTargets.putIfAbsent(hlgtKey, new ArrayList<String>());
                        ArrayList<String> groupTests = groupTargets.get(hlgtKey);
                        f.write(hlgtKey + ":");
                        for (String groupTest : groupTests) {
                            f.write(" \\\n" + groupTest);
                        }
                        // The 'all' / lgtKey contain normal and echo.disabled.
                        targetCount.merge(hlgtKey, groupTests.size(), Integer::sum);
                        if (eachDisHead.isEmpty()) {
                            f.write(" \\\necho.disabled." + lgtKey);
                            targetCount.merge("all", groupTests.size(), Integer::sum);
                        }
                        if (eachDisHead.equals("echo.disabled.")) {
                            // Normal key contains echo.disabled key
                            targetCount.merge(lgtKey, groupTests.size(), Integer::sum);
                            targetCount.merge("all", groupTests.size(), Integer::sum);
                        }
                        f.write("\n\n.PHONY: " + hlgtKey + "\n\n");
                    }
                }
            }
        }

        f.close();
    }

    private static String getModeInvalidSpecs(String group) {
        NodeList modesList = modes.getElementsByTagName("mode");

        for (int i = 0; i < modesList.getLength(); i++) {
            Element mode = (Element) modesList.item(i);

            if (mode.getAttribute("number").equals(group)) {
                Node invalidSpec = mode.getElementsByTagName("invalidSpecs").item(0);

                if (invalidSpec != null) {
                    return invalidSpec.getTextContent();
                }
            }
        }

        System.err.println("\nWarning: cannot find mode " + group + " in ottawa.csv to fetch invalid specs");

        return "";
    }

    private static String getModeClArgs(String group) {
        NodeList modesList = modes.getElementsByTagName("mode");

        for (int i = 0; i < modesList.getLength(); i++) {
            Element mode = (Element) modesList.item(i);

            if (mode.getAttribute("number").equals(group)) {
                NodeList clArgs = mode.getElementsByTagName("clArg");

                ArrayList<String> clArgsList = new ArrayList<String>(clArgs.getLength());

                for (int j = 0; j < clArgs.getLength(); j++) {
                    clArgsList.add(clArgs.item(j).getTextContent());
                }

                return String.join(" ", clArgsList);
            }
        }

        System.err.println("\nWarning: cannot find mode " + group);

        return "";
    }

    private static ArrayList<String> getAllInvalidSpecs(Element test, String invsp, Node preq, String graphSpecs) {
        ArrayList<String> invalidSpecs = new ArrayList<String>();
        ArrayList<String> specs = new ArrayList<String>(Arrays.asList(graphSpecs.split(",")));
        List<String> invalidSpecPerMode = Arrays.asList(invsp.split(" "));

        if (preq != null) {
            String[] platformRequirements = preq.getTextContent().split(",");

            for (String pr : platformRequirements) {
                // TODO: Had to add this because we have tests which have empty
                // platformRequirements elements. We should modify such tests
                // so this doesn't happen and remove this if block.
                if (pr.isEmpty()) {
                    continue;
                }

                pr = pr.trim();
                String[] prSplitOnDot = pr.split("\\.");

                // Copy specs into specArray
                ArrayList<String> specArray = (ArrayList<String>) specs.clone();
                for (int i = 0; i < specArray.size(); i++) {
                    if (!specArray.get(i).isEmpty()) {
                        String tmpVal = specArray.get(i);

                        // Special case 32/31-bit specs which do not have 32 or 31 in the name (i.e.
                        // aix_ppc)
                        if (!tmpVal.contains("-64")) {
                            if (tmpVal.contains("390")) {
                                tmpVal = specArray.get(i) + "-31";
                            } else {
                                tmpVal = specArray.get(i) + "-32";
                            }
                        }

                        if (prSplitOnDot[0].charAt(0) == '^') {
                            if (tmpVal.contains(prSplitOnDot[1])) {
                                invalidSpecs.add(specArray.get(i));

                                // Remove the element from specs
                                for (int j = 0; j < specs.size(); j++) {
                                    if (specs.get(j).equals(specArray.get(i))) {
                                        specs.remove(j);
                                    }
                                }
                            }
                        } else {
                            if (!tmpVal.contains(prSplitOnDot[1])) {
                                invalidSpecs.add(specArray.get(i));

                                // Remove the element from specs
                                for (int j = 0; j < specs.size(); j++) {
                                    if (specs.get(j).equals(specArray.get(i))) {
                                        specs.remove(j);
                                    }
                                }
                            }
                        }
                    }
                }

                // Reset specsArray
                specArray = specs;
            }
        }

        // Find intersection of runable specs and invalid specs
        Set<String> intersect = specs.stream().distinct().filter(invalidSpecPerMode::contains)
                .collect(Collectors.toSet());

        invalidSpecs.addAll(intersect);

        return invalidSpecs;
    }

    private static void dependGen() throws IOException {
        FileWriter f = new FileWriter(options.get("output") + "/TestConfig/" + dependmk + suffixForTesting);

        f.write(headerComments);

        allLevels.sort(null);
        allGroups.sort(null);
        allTypes.sort(null);

        List<String> allDisHead = Arrays.asList("", "disabled.", "echo.disabled.");
        for (String eachDisHead : allDisHead) {
            for (String eachLevel : allLevels) {
                for (String eachGroup : allGroups) {
                    String hlgKey = eachDisHead + eachLevel + '.' + eachGroup;
                    f.write(hlgKey + ":");
                    for (String eachType : allTypes) {
                        String hlgtKey = eachDisHead + eachLevel + '.' + eachGroup + '.' + eachType;
                        f.write(" \\\n" + hlgtKey);
                        targetCount.putIfAbsent(hlgtKey, 0);
                        targetCount.merge(hlgKey, targetCount.get(hlgtKey), Integer::sum);
                        targetCount.merge(eachDisHead + eachLevel, targetCount.get(hlgtKey), Integer::sum);
                    }
                    f.write("\n\n.PHONY: " + hlgKey + "\n\n");
                }
            }

            for (String eachGroup : allGroups) {
                for (String eachType : allTypes) {
                    String gtKey = eachDisHead + eachGroup + '.' + eachType;
                    f.write(gtKey + ":");
                    for (String eachLevel : allLevels) {
                        String lgtKey = eachDisHead + eachLevel + '.' + eachGroup + '.' + eachType;
                        f.write(" \\\n" + lgtKey);
                        targetCount.putIfAbsent(lgtKey, 0);
                        targetCount.merge(gtKey, targetCount.get(lgtKey), Integer::sum);
                        targetCount.merge(eachDisHead + eachGroup, targetCount.get(lgtKey), Integer::sum);
                    }
                    f.write("\n\n.PHONY: " + gtKey + "\n\n");
                }
            }

            for (String eachType : allTypes) {
                for (String eachLevel : allLevels) {
                    String ltKey = eachDisHead + eachLevel + '.' + eachType;
                    f.write(ltKey + ":");
                    for (String eachGroup : allGroups) {
                        String lgtKey = eachDisHead + eachLevel + '.' + eachGroup + '.' + eachType;
                        f.write(" \\\n" + lgtKey);
                        targetCount.putIfAbsent(lgtKey, 0);
                        targetCount.merge(ltKey, targetCount.get(lgtKey), Integer::sum);
                        targetCount.merge(eachDisHead + eachType, targetCount.get(lgtKey), Integer::sum);
                    }
                    f.write("\n\n.PHONY: " + ltKey + "\n\n");
                }
            }

            for (String eachLevel : allLevels) {
                String lKey = eachDisHead + eachLevel;
                f.write(lKey + ":");
                for (String eachGroup : allGroups) {
                    f.write(" \\\n" + eachDisHead + eachLevel + '.' + eachGroup);
                }
                f.write("\n\n.PHONY: " + lKey + "\n\n");
            }

            for (String eachGroup : allGroups) {
                String gKey = eachDisHead + eachGroup;
                f.write(gKey + ":");
                for (String eachLevel : allLevels) {
                    f.write(" \\\n" + eachDisHead + eachLevel + '.' + eachGroup);
                }
                f.write("\n\n.PHONY: " + gKey + "\n\n");
            }

            for (String eachType : allTypes) {
                String tKey = eachDisHead + eachType;
                f.write(tKey + ":");
                for (String eachLevel : allLevels) {
                    f.write(" \\\n" + eachDisHead + eachLevel + '.' + eachType);
                }
                f.write("\n\n.PHONY: " + tKey + "\n\n");
            }

            String allKey = eachDisHead + "all";
            f.write(allKey + ":");
            for (String eachLevel : allLevels) {
                f.write(" \\\n" + eachDisHead + eachLevel);
            }
            f.write("\n\n.PHONY: " + allKey + "\n\n");
        }

        f.close();

        System.out.println();
        System.out.println("Generated " + dependmk);
    }

    private static void utilsGen() throws IOException {
        FileWriter f = new FileWriter(options.get("output") + "/TestConfig/" + utilsmk + suffixForTesting);

        f.write(headerComments);
        f.write("PLATFORM=\n");
        String spec2platform = "";
        for (String graphSpec : options.get("graphSpecs").split(",")) {
            if (sp_hs.containsKey(graphSpec)) {
                spec2platform = "ifeq" + " ($(SPEC)," + graphSpec + ")\n\tPLATFORM=" + sp_hs.get(graphSpec)
                        + "\nendif\n\n";
            } else {
                System.err.println(
                        "\nWarning: cannot find spec " + graphSpec + " in ModesDictionaryService or ottawa.csv file");
            }
        }
        f.write(spec2platform);

        f.close();

        System.out.println();
        System.out.println("Generated " + utilsmk);
    }

    private static void countGen() throws IOException {
        FileWriter f = new FileWriter(options.get("output") + "/TestConfig/" + countmk + suffixForTesting);

        f.write(headerComments);

        List<String> targetCountKeys = new ArrayList<>(targetCount.keySet());
        Collections.sort(targetCountKeys);

        f.write("_GROUPTARGET = $(firstword $(MAKECMDGOALS))\n\n");
        f.write("GROUPTARGET = $(patsubst _%,%,$(_GROUPTARGET))\n\n");
        for (String key : targetCountKeys) {
            f.write("ifeq ($(GROUPTARGET)," + key + ")\n");
            f.write("\tTOTALCOUNT := " + targetCount.get(key) + "\n");
            f.write("endif\n\n");
        }

        f.close();

        System.out.println();
        System.out.println("Generated " + countmk);
    }
}
