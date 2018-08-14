/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
package com.ibm.moduletools;

import java.io.Closeable;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.Reader;
import java.io.StreamTokenizer;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Objects;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * This class merges module-info source files, allowing:
 * <ul>
 * <li>multiple requires clauses referring to the same module</li>
 * <li>multiple exports clauses referring to the same package</li>
 * <li>multiple opens clauses referring to the same package</li>
 * <li>multiple uses clauses referring to the same class or interface</li>
 * <li>multiple provides clauses referring to the same implementation class</li>
 * </ul>
 */
@SuppressWarnings("nls")
public final class ModuleInfoMerger {

	/**
	 * A representation of the content of a set of merged module-info source files.
	 */
	private static final class ModuleInfo {

		/**
		 * An unrestricted exports or opens clause leads to this value
		 * in the exports or opens maps.
		 */
		private static final String AllModules = "*";

		/**
		 * The bit representing the 'static' modifier of a requires clause.
		 */
		private static final int STATIC = 2;

		/**
		 * The bit representing the 'transitive' modifier of a requires clause.
		 */
		private static final int TRANSITIVE = 1;

		/**
		 * This flag indicates that a deprecated level using 'requires public' is being processed.
		 * Following variable along with related code can be removed when b148+ is a default level.
		 */
		private static final String TransitiveKeyword = Boolean.getBoolean("usePublicKeyword")
				? "public" : "transitive";

		/*
		 * Write the leading whitespace to begin a new line in the body
		 * of the module-info declaration.
		 */
		private static Appendable indent(Appendable out) throws IOException {
			return out.append("\t");
		}

		/*
		 * With the given scanner, parse the portion of an exports or opens
		 * clause following the leading keyword.
		 */
		private static void parseTargets(ModuleInfoScanner scanner, Map<String, Set<String>> targetMap)
				throws IOException {
			String packageName = scanner.requireWord();
			Set<String> targets = targetMap.computeIfAbsent(packageName, key -> new TreeSet<>());
			String token = scanner.token();

			if ("to".equals(token)) {
				do {
					targets.add(scanner.requireWord());
					token = scanner.token();
				} while (",".equals(token));
			} else {
				// Unrestricted trumps targeted exports or opens.
				targets.add(AllModules);
			}

			if (!";".equals(token)) {
				throw scanner.expected(";", token);
			}
		}

		/*
		 * Write the 'targets' of an exports or opens clause.
		 */
		private static void writeTargets(Appendable out, String clause, Map<String, Set<String>> clauseMap)
				throws IOException {
			for (Entry<String, Set<String>> entry : clauseMap.entrySet()) {
				String packageName = entry.getKey();
				Set<String> targets = entry.getValue();

				indent(out).append(clause).append(" ").append(packageName);

				if (!(targets.isEmpty() || targets.contains(AllModules))) {
					String separator = " to ";

					for (String target : targets) {
						out.append(separator).append(target);
						separator = ", ";
					}
				}

				out.append(";\n");
			}
		}

		/**
		 * Keys of this map are exported package names; the values name the
		 * modules which have visibility (for an unrestricted export, the
		 * values set contains AllModules).
		 */
		private final Map<String, Set<String>> exports;

		/**
		 * Is this module open?
		 */
		private boolean isOpen;

		/**
		 * The name of the module.
		 */
		private String name;

		/**
		 * Keys of this map are exported package names; the values name the modules
		 * which have visibility (or empty for all modules).
		 */
		/**
		 * Keys of this map are open package names; the values name the
		 * modules which have reflective access (for an unrestricted opens,
		 * the values set contains AllModules).
		 */
		private final Map<String, Set<String>> opens;

		/**
		 * The keys of this map name the interfaces with implementations provided by
		 * this module; the values are the names of those implementation types.
		 */
		private final Map<String, Set<String>> provides;

		/**
		 * The keys of this map name required modules; the values express the modifiers
		 * (e.g. transitive).
		 */
		private final Map<String, Integer> requires;

		/**
		 * This set names the service interfaces types used by this module.
		 */
		private final Set<String> uses;

		/**
		 * Construct a new, unnamed module.
		 */
		ModuleInfo() {
			super();
			exports = new TreeMap<>();
			isOpen = false;
			name = null;
			opens = new TreeMap<>();
			provides = new TreeMap<>();
			requires = new TreeMap<>();
			uses = new TreeSet<>();
		}

		/*
		 * Parse a single module-info.java source file accessed via the given scanner.
		 */
		private void parse(ModuleInfoScanner scanner) throws IOException {
			boolean isExtra = false;
			String moduleName = null;
			String token = scanner.token();

			if ("open".equals(token)) {
				isOpen = true;
				scanner.require("module");
				moduleName = scanner.requireWord();
				scanner.require("{");
				token = scanner.token();
			} else if ("module".equals(token)) {
				moduleName = scanner.requireWord();
				scanner.require("{");
				token = scanner.token();
			} else {
				isExtra = true;
			}

			if (moduleName != null) {
				if (name == null) {
					name = moduleName;
				} else if (!name.equals(moduleName)) {
					throw new IOException(String.format("Cannot merge modules %s and %s", name, moduleName));
				}
			}

			for (; token != null; token = scanner.token()) {
				switch (token) {
				case "}":
					scanner.require(null);
					return;
				case "exports":
					parseTargets(scanner, exports);
					break;
				case "opens":
					parseTargets(scanner, opens);
					break;
				case "provides":
					parseProvides(scanner);
					break;
				case "requires":
					parseRequires(scanner);
					break;
				case "uses":
					parseUses(scanner);
					break;
				default:
					throw scanner.expected("}", token);
				}
			}

			if (!isExtra) {
				throw scanner.expected("}", token);
			}
		}

		/*
		 * With the given scanner, parse the portion of a provides clause following the 'provides' keyword.
		 */
		private void parseProvides(ModuleInfoScanner scanner) throws IOException {
			String interfaceName = scanner.requireWord();
			Set<String> implementationNames = provides.computeIfAbsent(interfaceName, key -> new TreeSet<>());
			String token;

			scanner.require("with");

			do {
				implementationNames.add(scanner.requireWord());
				token = scanner.token();
			} while (",".equals(token));

			if (!";".equals(token)) {
				throw scanner.expected(";", token);
			}
		}

		/*
		 * With the given scanner, parse the portion of a requires clause following the 'requires' keyword.
		 */
		private void parseRequires(ModuleInfoScanner scanner) throws IOException {
			int modifiers = 0;
			String requiredModule;

			for (;;) {
				String token = scanner.requireWord();

				if ("transitive".equals(token) || "public".equals(token)) {
					modifiers |= TRANSITIVE;
				} else if ("static".equals(token)) {
					modifiers |= STATIC;
				} else {
					requiredModule = token;
					break;
				}
			}

			updateModifiers(requiredModule, modifiers);

			scanner.require(";");
		}

		/*
		 * With the given scanner, parse the portion of a uses clause following the 'uses' keyword.
		 */
		private void parseUses(ModuleInfoScanner scanner) throws IOException {
			uses.add(scanner.requireWord());
			scanner.require(";");
		}

		/**
		 * Read a single module-info source file, merging its contents with this module.
		 *
		 * @param fileName the name of the source file to read
		 * @throws IOException if a problems occurs reading the source file
		 */
		void readFrom(String fileName) throws IOException {
			try (ModuleInfoScanner scanner = new ModuleInfoScanner(fileName)) {
				parse(scanner);
			}
		}

		/*
		 * Update the modifiers for a required module.
		 */
		private void updateModifiers(String requiredModule, int modifiers) {
			requires.compute(requiredModule, (k, v) -> Integer.valueOf((v == null ? 0 : v.intValue()) | modifiers));
		}

		/*
		 * Write the cononical source representation for this module.
		 * The requires, exports, uses and provides clauses are each written
		 * in turn with individual elements in lexicographical order.
		 */
		private void writeTo(Appendable out) throws IOException {
			if (name == null) {
				throw new IOException("Module has no name");
			}

			if (isOpen) {
				out.append("open ");
			}

			out.append("module ").append(name).append(" {\n");

			for (Entry<String, Integer> entry : requires.entrySet()) {
				int modifiers = entry.getValue().intValue();

				indent(out).append("requires ");

				if ((modifiers & TRANSITIVE) != 0) {
					out.append(TransitiveKeyword).append(" ");
				}
				if ((modifiers & STATIC) != 0) {
					out.append("static ");
				}

				out.append(entry.getKey()).append(";\n");
			}

			writeTargets(out, "exports", exports);

			if (!isOpen) {
				writeTargets(out, "opens", opens);
			}

			for (String use : uses) {
				indent(out).append("uses ").append(use).append(";\n");
			}

			for (Entry<String, Set<String>> entry : provides.entrySet()) {
				String interfaceName = entry.getKey();
				Set<String> implementationNames = entry.getValue();

				if (implementationNames.isEmpty()) {
					continue;
				}

				indent(out);
				out.append("provides ").append(interfaceName);

				String separator = " with ";

				for (String implementationName : implementationNames) {
					out.append(separator).append(implementationName);
					separator = ", ";
				}

				out.append(";\n");
			}

			out.append("}\n");
		}

		/**
		 * Write the cononical source representation for this module to the file of
		 * the given name. The requires, exports, uses and provides clauses are each
		 * written in turn with individual elements in lexicographical order.
		 *
		 * @param fileName the name of the file to write
		 * @throws IOException if a problems occurs writing the file
		 */
		void writeTo(String fileName) throws IOException {
			try (FileWriter writer = new FileWriter(fileName)) {
				writeTo(writer);
			}
		}

	}

	/**
	 * This class supports tokenization of Java module-info source files.
	 */
	private static final class ModuleInfoScanner implements AutoCloseable {

		/*
		 * Create a tokenizer configured for scanning module-info source files.
		 */
		private static StreamTokenizer createTokenizer(Reader reader) {
			StreamTokenizer tokenizer = new StreamTokenizer(reader);

			// handle both kinds of Java comments
			tokenizer.slashStarComments(true);
			tokenizer.slashSlashComments(true);

			// configure whitespace characters
			tokenizer.whitespaceChars(' ', ' ');
			tokenizer.whitespaceChars('\t', '\t');
			tokenizer.whitespaceChars('\n', '\n');
			tokenizer.whitespaceChars('\r', '\r');

			// configure identifier characters
			tokenizer.wordChars('A', 'Z');
			tokenizer.wordChars('a', 'z');
			tokenizer.wordChars('_', '_');
			tokenizer.wordChars('0', '9');
			tokenizer.wordChars('$', '$');
			tokenizer.wordChars('.', '.');

			return tokenizer;
		}

		/*
		 * Answer a human-readable representation for the given token (null means EOF).
		 */
		private static String tokenName(String token) {
			return token != null ? String.format("'%s'", token) : "<end-of-file>";
		}

		/**
		 * The name of the file being scanned.
		 */
		private final String fileName;

		/**
		 * The input to be closed.
		 */
		private final Closeable input;

		/**
		 * The tokenizer for the file being scanned.
		 */
		private final StreamTokenizer tokenizer;

		/**
		 * Create a new scanner for the file of the given name.
		 *
		 * @param fileName
		 * @throws IOException
		 */
		ModuleInfoScanner(String fileName) throws IOException {
			super();

			Reader reader = new FileReader(fileName);

			this.fileName = fileName;
			this.input = reader;
			this.tokenizer = createTokenizer(reader);
		}

		/**
		 * {@inheritDoc}
		 */
		@Override
		public void close() throws IOException {
			input.close();
		}

		/**
		 * Construct a new IOException indicating a parsing error.
		 *
		 * @param required the expected token
		 * @param actual the token actually present (or null representing end-of-file)
		 * @return a new exception object
		 */
		IOException expected(String required, String actual) {
			return new IOException(String.format("%s:%d: expected %s, but found %s", // <br/>
					fileName, Integer.valueOf(tokenizer.lineno()),
					tokenName(required), tokenName(actual)));
		}

		/**
		 * Consume the next token which is expected to be the given value.
		 *
		 * @param required the expected token, or null if expecting EOF
		 * @throws IOException if a problem occurs reading the input
		 * or if the required token is not found
		 */
		void require(String required) throws IOException {
			String actual = token();

			if (!Objects.equals(required, actual)) {
				throw expected(required, actual);
			}
		}

		/**
		 * Return the next token which is expected to be a 'word'.
		 *
		 * @throws IOException if a problem occurs reading the input
		 * or if the next token is not a 'word'
		 */
		String requireWord() throws IOException {
			String actual = token();

			if (tokenizer.ttype == StreamTokenizer.TT_WORD) {
				return actual;
			}

			throw expected("<word>", actual);
		}

		/**
		 * Return the next token or null (which signals end-of-file).
		 *
		 * @throws IOException if a problem occurs reading the input
		 */
		String token() throws IOException {
			int token = tokenizer.nextToken();

			switch (token) {
			case StreamTokenizer.TT_EOF:
				return null;

			case StreamTokenizer.TT_WORD:
				return tokenizer.sval;

			default:
				return String.valueOf((char) token);

			case StreamTokenizer.TT_EOL:
			case StreamTokenizer.TT_NUMBER:
				throw new IllegalStateException();
			}
		}

	}

	/*
	 * Print the given message to stderr and terminate the program.
	 */
	private static void fatal(String message) {
		System.err.println(message);
		System.exit(1);
	}

	/**
	 * Read one or more module-info source files and write the merged description.
	 * Arguments are expected to be file names; all but the last are inputs, the
	 * last names the output file.
	 *
	 * @param args the names of files to read/write
	 */
	public static void main(String[] args) {
		int argCount = args.length;

		if (argCount < 2) {
			usage();
		} else {
			List<String> inputFileNames = Arrays.asList(args).subList(0, argCount - 1);
			String outputFileName = args[argCount - 1];
			ModuleInfo moduleInfo = new ModuleInfo();

			for (String fileName : inputFileNames) {
				try {
					moduleInfo.readFrom(fileName);
				} catch (IOException e) {
					fatal("Error reading '" + fileName + "': " + e.getLocalizedMessage());
				}
			}

			try {
				moduleInfo.writeTo(outputFileName);
			} catch (IOException e) {
				fatal("Can't write " + outputFileName + ": " + e.getLocalizedMessage());
			}
		}
	}

	private static void usage() {
		System.err.println("Usage: " + ModuleInfoMerger.class.getName() // <br/>
				+ " module-info-1.java module-info-2.java ... module-info-out.java");
	}

	/*
	 * Prevent instantiation of this class.
	 */
	private ModuleInfoMerger() {
		super();
	}

}
