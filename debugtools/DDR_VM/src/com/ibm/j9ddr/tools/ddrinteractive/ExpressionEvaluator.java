/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.util.ArrayList;

/**
 * @author Manqing Li, IBM Canada
 */
public class ExpressionEvaluator {
	private static final String _UNARY_OPERATORS = "+-";
	private static final String[] _BINARY_OPERATORS_IN_PRECEDENCE = { "*/%", "+-" };
	private static final String _LEFT_PARENTHESES = "{[(<";
	private static final String _RIGHT_PARENTHESES = "}])>";
	private static final String _HEXADECIMAL_INDICATOR = "0x";
	private static final String _DECIMAL_INDICATOR = "d";
	private static final String _BINARY_INDICATOR = "bi";

	private final ArrayList<String> _al;

	public ExpressionEvaluator(final String s) {
		this(new String[] { s });
	}

	public ExpressionEvaluator(final String[] as) {
		_al = format(as);
	}

	public ExpressionEvaluator(final ArrayList<String> al) {
		_al = format(al);
	}

	/**
	 * Evaluate expression with support for operator precedence and associativity
	 * 
	 * @param exp
	 *            arithmetic expression.
	 * @return result or evaluation
	 * @throws ExpressionEvaluatorException
	 *             if expression is in invalid format.
	 */
	public static long eval(String exp) throws ExpressionEvaluatorException {
		return eval(exp, CommandUtils.RADIX_DECIMAL);
	}

	public static long eval(String exp, int defaultRadix) throws ExpressionEvaluatorException {
		return new ExpressionEvaluator(exp).calculate(defaultRadix);
	}
	
	public long calculate(int defaultRadix)
			throws ExpressionEvaluatorException {
		try {
			return calculate(0, _al.size() - 1, defaultRadix);
		} catch (NumberFormatException e) {
			throw new ExpressionEvaluatorException(e);
		} catch (IndexOutOfBoundsException e) {
			throw new ExpressionEvaluatorException(e);
		} catch (NullPointerException e) {
			throw new ExpressionEvaluatorException(e);
		}
	}

	private long calculate(int startIndex, int endIndex, int defaultRadix)
			throws ExpressionEvaluatorException {
		// If only one token, we simply assume it is an operand.
		if (startIndex == endIndex) {
			return evaluateToken(_al.get(startIndex), defaultRadix);
		}
		ArrayList<Range> topLevelRanges = topLevelRanges(startIndex, endIndex);

		ArrayList<String> topLevel = topLevel(topLevelRanges, defaultRadix);

		return calculateTopLevel(topLevel);
	}

	/**
	 * The top level contains a list of ranges. Operators and operands are
	 * considered ranges by themselves; The content between a left parenthesis
	 * to its matching parenthesis, excluding both parentheses, is a range too.
	 */
	private ArrayList<Range> topLevelRanges(int startIndex, int endIndex)
			throws ExpressionEvaluatorException {
		ArrayList<Range> topLevelRanges = new ArrayList<ExpressionEvaluator.Range>();
		int index = startIndex;
		while (index <= endIndex) {
			String s = _al.get(index);
			if (isLeftParenthesis(s)) {
				int rightIndex = findRightParenthesisIndex(index + 1, endIndex);
				topLevelRanges.add(new Range(index + 1, rightIndex - 1));
				index = rightIndex + 1;
			} else {
				topLevelRanges.add(new Range(index, index));
				index++;
			}
		}
		return topLevelRanges;
	}

	/**
	 * Evaluate all individual ranges in the top level.
	 */
	private ArrayList<String> topLevel(ArrayList<Range> topLevelRanges,
			int defaultRadix) throws ExpressionEvaluatorException {
		// Calculate all ranges
		ArrayList<String> topLevel = new ArrayList<String>(
				topLevelRanges.size());
		for (int i = 0; i < topLevelRanges.size(); i++) {
			Range range = topLevelRanges.get(i);
			if (range.start == range.end) {
				int location = range.start;
				if (isOperator(_al.get(location))) {
					topLevel.add(_al.get(location));
				} else {
					topLevel.add(""
							+ evaluateToken(_al.get(location), defaultRadix));
				}
			} else {
				topLevel.add(""
						+ calculate(range.start, range.end, defaultRadix));
			}
		}
		return topLevel;
	}

	/**
	 * Evaluate the top level according to the binary operation precedence.
	 */
	private long calculateTopLevel(ArrayList<String> topLevel)
			throws ExpressionEvaluatorException {
		if (topLevel.size() == 1) {
			return evaluateToken(topLevel.get(0), 10);
		}

		// Unary operator should only be possible at the start.
		topLevel = evaluateWithUnaryOperator(topLevel);

		for (int i = 0; i < _BINARY_OPERATORS_IN_PRECEDENCE.length; i++) {
			String sTargetBinaryOperators = _BINARY_OPERATORS_IN_PRECEDENCE[i];
			topLevel = evaluateWithBinaryOperator(topLevel,
					sTargetBinaryOperators);
		}
		return evaluateToken(topLevel.get(0), 10);
	}

	private long evaluateToken(String token, int defaultRadix) {
		if (startsWithCaseInsensitively(token,
				_HEXADECIMAL_INDICATOR.toLowerCase())) {
			return stripPrefixParseLong(token, 16, _HEXADECIMAL_INDICATOR);
		}
		if (startsWithCaseInsensitively(token, _BINARY_INDICATOR.toLowerCase())) {
			return stripPrefixParseLong(token, 2, _BINARY_INDICATOR);
		}
		if (startsWithCaseInsensitively(token, _DECIMAL_INDICATOR.toLowerCase())) {
			return stripPrefixParseLong(token, 10, _DECIMAL_INDICATOR);
		}

		return Long.parseLong(token, defaultRadix);
	}

	private long stripPrefixParseLong(String token, int radix, String prefix) {
		/* Strip off multiple occurring prefixes that may happen due to copy-paste. */
		while (startsWithCaseInsensitively(token, prefix)) {
			token = token.substring(prefix.length());
		}
		return Long.parseLong(token, radix);
	}
	
	private boolean startsWithCaseInsensitively(String s, String head) {
		return s.length() >= head.length()
				&& s.substring(0, head.length()).equalsIgnoreCase(head);
	}

	private ArrayList<String> evaluateWithUnaryOperator(ArrayList<String> al)
			throws ExpressionEvaluatorException {
		String first = al.get(0);
		if (isUnaryOperator(first) == false) {
			return al;
		}
		al.remove(0);
		String operand = al.remove(0);
		String result = unaryCalculate(first, operand);
		al.add(0, result);
		return al;
	}

	private ArrayList<String> evaluateWithBinaryOperator(ArrayList<String> al,
			String sTargetBinaryOperators)
			throws ExpressionEvaluatorException {

		String operator = null;
		String right = null;
		String result = null;
		String left = al.get(0);

		int startIndex = 0;
		if (isUnaryOperator(left)) {
			startIndex = 1;
		}

		while (startIndex < al.size() - 1) {
			left = al.get(startIndex);
			operator = al.get(startIndex + 1);
			if (sTargetBinaryOperators.indexOf(operator) < 0) {
				startIndex += 2; // skip left and operation, but the one right
									// after is the left for the next operation.
				left = null;
			} else {
				right = al.get(startIndex + 2);
				result = "" + binaryCalculate(left, operator, right);
				al.remove(startIndex); // cut the left operand;
				al.remove(startIndex); // cut the operation;
				al.remove(startIndex); // cut the right operand;
				al.add(startIndex, result);
			}
		}
		return al;
	}

	private long binaryCalculate(String left, String operator, String right)
			throws ExpressionEvaluatorException {
		char c = operator.charAt(0);
		if (c == '+') {
			return Long.parseLong(left) + Long.parseLong(right);
		}
		if (c == '-') {
			return Long.parseLong(left) - Long.parseLong(right);
		}
		if (c == '*') {
			return Long.parseLong(left) * Long.parseLong(right);
		}
		if (c == '/') {
			return Long.parseLong(left) / Long.parseLong(right);
		}
		if (c == '%') {
			return Long.parseLong(left) % Long.parseLong(right);
		}
		throw new ExpressionEvaluatorException("unsupported binary operator:"
				+ operator);
	}

	private String unaryCalculate(String unary, String operand)
			throws ExpressionEvaluatorException {
		if (unary.charAt(0) == '+') {
			return operand;
		} else if (unary.charAt(0) == '-') {
			return "" + (-1 * Long.parseLong(operand));
		}
		throw new ExpressionEvaluatorException(
				"Unrecognized unary operator: " + unary);
	}

	private int findRightParenthesisIndex(int startIndex, int endIndex)
			throws ExpressionEvaluatorException {
		int count = 0;
		for (int i = startIndex; i <= endIndex; i++) {
			String s = _al.get(i);
			if (isLeftParenthesis(s)) {
				count--;
			}
			if (isRightParenthesis(s)) {
				count++;
			}
			if (count == 1) {
				return i;
			}
		}
		throw new ExpressionEvaluatorException("Mis-matched parentheses.");
	}

	private ArrayList<String> format(final String[] as) {
		ArrayList<String> al = new ArrayList<String>();
		for (int i = 0; i < as.length; i++) {
			al.addAll(format(as[i]));
		}
		return al;
	}

	private ArrayList<String> format(final ArrayList<String> al) {
		ArrayList<String> alReturn = new ArrayList<String>();
		for (int i = 0; i < al.size(); i++) {
			alReturn.addAll(format(al.get(i)));
		}
		return alReturn;
	}

	private ArrayList<String> format(String s) {
		ArrayList<String> al = new ArrayList<String>();
		StringBuffer sbTemp = new StringBuffer();
		for (int i = 0; i < s.length(); i++) {
			char c = s.charAt(i);
			if (c == ' ' || c == '\t') {
				continue;
			}
			if (isParenthesis(c) || isOperator(c)) {
				if (sbTemp.length() > 0) {
					al.add(sbTemp.toString());
				}
				al.add("" + c);
				sbTemp = new StringBuffer();
			} else {
				sbTemp.append(c);
			}
		}
		if (sbTemp.length() > 0) {
			al.add(sbTemp.toString());
		}
		return al;
	}

	private boolean isOperator(String s) {
		return s.length() == 1 && isOperator(s.charAt(0));
	}

	private boolean isOperator(char c) {
		return isBinaryOperator(c) || isUnaryOperator(c);
	}

	private boolean isBinaryOperator(char c) {
		for (int i = 0; i < _BINARY_OPERATORS_IN_PRECEDENCE.length; i++) {
			if (_BINARY_OPERATORS_IN_PRECEDENCE[i].indexOf(c) >= 0) {
				return true;
			}
		}
		return false;
	}

	private boolean isUnaryOperator(String s) {
		return s.length() == 1 && isUnaryOperator(s.charAt(0));
	}

	private boolean isUnaryOperator(char c) {
		return _UNARY_OPERATORS.indexOf(c) >= 0;
	}

	private boolean isParenthesis(char c) {
		return isLeftParenthesis(c) || isRightParenthesis(c);
	}

	private boolean isLeftParenthesis(String s) {
		return s.length() == 1 && isLeftParenthesis(s.charAt(0));
	}

	private boolean isLeftParenthesis(char c) {
		return _LEFT_PARENTHESES.indexOf(c) >= 0;
	}

	private boolean isRightParenthesis(String s) {
		return s.length() == 1 && isRightParenthesis(s.charAt(0));
	}

	private boolean isRightParenthesis(char c) {
		return _RIGHT_PARENTHESES.indexOf(c) >= 0;
	}

	private class Range {
		public Range(int startIndex, int endIndex) {
			start = startIndex;
			end = endIndex;
		}

		public int start;
		public int end;
	};
}
