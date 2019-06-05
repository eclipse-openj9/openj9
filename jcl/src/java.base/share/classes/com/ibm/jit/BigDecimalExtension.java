/*[INCLUDE-IF]*/
package com.ibm.jit;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.math.MathContext;
import java.math.RoundingMode;
import java.util.Arrays;

/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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

public class BigDecimalExtension implements java.math.BigDecimal.BigDecimalExtension {
   // Used to keep behavior of DFPHWAvailable consistent between
   // jitted code and interpreter
   // DO NOT CHANGE OR MOVE THIS LINE
   // IT MUST BE THE FIRST THING IN THE INITIALIZATION 
   private static final boolean DFP_HW_AVAILABLE = DFPCheckHWAvailable();
   
   /*
    *    public static final int    ROUND_CEILING  2
    *       DFP hardware uses the value 010 == 2
    *    public static final int    ROUND_DOWN  1
    *      DFP hardware uses the value 001 = 1
    *    public static final int    ROUND_FLOOR    3
    *      DFP hardware uses the value 011 = 3
    *    public static final int    ROUND_HALF_DOWN   5
    *      DFP hardware uses the value 101 = 5
    *    public static final int    ROUND_HALF_EVEN   6     <--mismatch in HW/Classlib spec
    *      DFP hardware uses the value 000 = 0
    *    public static final int    ROUND_HALF_UP  4
    *      DFP hardware uses the value 100 = 4
    *    public static final int    ROUND_UNNECESSARY    7
    *       DFP hardware uses the value 111 = 7
    *    public static final int    ROUND_UP    0        <--mismatch in HW/Classlib spec
    *      DFP hardware uses the value 110 = 6
    */

   /**
    * LUT for DFP double combination field - gives us the leftmost
    * digit of the coefficient, as well as the first two bits of the
    * exponent in form:  0xXY where X = LMD, Y = first two bits
    */
   private static byte []doubleDFPComboField = comboinit();

   // LUT for converting DPD to BCD, 1024 elements
   private static short []DPD2BCD = dpd2bcdinit();

   // DFP 0 with exponent 0
   private static final long dfpZERO = 2465720795985346560L;
   
   private static final BigInteger MAXDFP64 = BigInteger.valueOf(9999999999999999L);
   private static final BigInteger MINDFP64 = BigInteger.valueOf(-9999999999999999L);

   /* Used for Hysteresis for mixed-representation
    * We want the BigDecimal class to choose the best representation
    * for construction and operations.  We start assuming the LL
    * is the best representation.  Over the course of time, using
    * hysterisis, we might alter this decision.
    *
    * The constructors are annotated with the checks on deciding
    * which representation to use, and other APIs contribute
    * to biasing towards or away from a representation.
    *
    * NOTE: Hysterisis only works on platforms that supports DFP
    * since we prepend a DFPHWAvailable check before performing
    * mods to the counters, and before basing decisions off them.
    */
   private static int hys_threshold = 1000; // threshold for representation change
   // above means that hys_counter must reach -/+ MAX_VALUE before changing rep
   private static boolean hys_type; // false = non-dfp, true = dfp
   private static int hys_counter; // increment for dfp, decrement for non-dfp

   private static BigDecimalExtension instance;

   protected BigDecimalExtension() {
   }

   public static BigDecimalExtension getInstance() {
      if (instance == null) {
         instance = new BigDecimalExtension();
      }
      return instance;
   }
   
   public boolean performHardwareUsageHeuristic(MathContext set, int bias) {
      if (DFPPerformHysteresis() && 
          set.getPrecision() == 16 && set.getRoundingMode().ordinal() == BigDecimal.ROUND_HALF_EVEN){
         performHardwareUsageHeuristic(bias);
         return true;
      }
      return false;
   }

   private void performHardwareUsageHeuristic(int bias) {
      int sum = hys_counter + bias;
      hys_counter += bias & ~(((sum^bias) & (sum^hys_counter)) >>> 31);

      if (hys_counter<-hys_threshold){
         hys_type = false; //nonDFP
         hys_counter=0;
      }
      else if (hys_counter>hys_threshold){
         hys_type = true; // DFP
         hys_counter=0;
      }
   }

   public boolean isAvailable() {
      return DFPHWAvailable();
   }
   
   public boolean useExtension() {
      return DFPUseDFP();
   }

   public boolean suitableForExtension(int nDigits, int scale) {
      return (nDigits < 17 && scale >= -398 && scale < 369);
   }

   public int add(BigDecimal res, BigDecimal lhs, BigDecimal rhs) {
      int resExp =-(Math.max(lhs.scale(), rhs.scale()));
      /* Precision = 0, rounding = UNNECESSARY */
      if (resExp >=-398 && resExp<=369){
         if(DFPScaledAdd(res, getlaside(rhs), getlaside(lhs), resExp+398)){
            // we can and out the flags because we don't have
            // a sense of the exponent/precision/sign
            // of this result, nor do we want to use DFP hw
            // to figure it out, so we do not cache anything

            //set res as DFP - (already set to 00)

            // because DFP add takes the exclusive or of the sign bits
            //for the result, need to make sure result of add 0 by -0
            //doesn't store a negative sign...
            long laside = getlaside(res);
            if(isDFPZero(laside)) {
               laside &= 0x7FFFFFFFFFFFFFFFL;
               setlaside(res, laside);
            }
            return HARDWARE_OPERATION_SUCCESS;
         }
      }
      return HARDWARE_OPERATION_FAIL;
   }
   
   public int add(BigDecimal res, BigDecimal lhs, BigDecimal rhs, MathContext set) {
      boolean passed = false;
      int prec = set.getPrecision();
      int rm = set.getRoundingMode().ordinal();
      long lhslaside = getlaside(lhs);
      long rhslaside = getlaside(rhs);

      // need to special case this since DFP hardware
      // doesn't conform to BigDecimal API when adding
      // to a 0..
      if (prec != 0 && set.getRoundingMode().ordinal() != BigDecimal.ROUND_UNNECESSARY){
         boolean lZero = isDFPZero(lhslaside);
         boolean rZero = isDFPZero(rhslaside);

         if(lZero || rZero) {
            if(!lZero) {
               clone(lhs, res); // rhs is zero 
               if (rhs.scale() > lhs.scale()){
                  if (set.getPrecision()-rhs.precision() >0) {
                     clone(res.setScale(Math.abs(-rhs.scale())), res); //might return BI
                  }
               }
            } else if (!rZero) {
               clone(rhs, res); // lhs is zero
               if (lhs.scale() > rhs.scale()){
                  if (set.getPrecision()-rhs.precision() >0) {
                     clone(res.setScale(Math.abs(-lhs.scale())), res);  //might return BI
                  }
               }
            } else {
               BigDecimal temp = BigDecimal.valueOf(0, Math.max(lhs.scale(), rhs.scale())); // both operands are zero
               clone(temp, res); //CMVC 136111 -- res could be one of statics (shared)
            }

            if (set != MathContext.UNLIMITED) {
               if(finish(res, set.getPrecision(), set.getRoundingMode().ordinal()) == HARDWARE_OPERATION_FAIL) {
                  return HARDWARE_OPERATION_FAIL;
               }

            }
            return HARDWARE_OPERATION_SUCCESS;
         }
      }

      // we can and out the flags because we don't have
      // a sense of the exponent/precision/sign
      // of this result, nor do we want to use DFP hw
      // to figure it out, so we do not cache anything

      // fast path for MathContext64
      if (prec == 16 && rm == BigDecimal.ROUND_HALF_EVEN){
         if(DFPAdd(res, rhslaside, lhslaside, 64, 0, 0)){
            passed =true;
         }
      } else if (prec == 0) {   // same as scaled DFPAddition
         int resExp =-(Math.max(lhs.scale(), rhs.scale()));
         if (resExp >=-398 && resExp <= 369){
            if(DFPScaledAdd(res, rhslaside, lhslaside, resExp+398)){
               //set res as DFP - (already set to 00)
               passed = true;
            }
         }
      } else if (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY){   // fast path for NO ROUNDING, as well as the ArithmeticException
         if(DFPAdd(res, rhslaside, lhslaside, 0, 0, 0)){
            performHardwareUsageHeuristic(set, -3);
            // set res as DFP - (already set to 00)
            if(finish(res, prec, rm) == HARDWARE_OPERATION_FAIL) {   // See if we need to throw an arithmetic exception
               return HARDWARE_OPERATION_FAIL;
            }
            passed = true;
         }
      } else if (prec <=16) {    // Otherwise, if a precision to round to is specified

         // NOTE:  We do the following two if statements
         // since the constants used for HALF_EVEN and ROUND_UP in
         // the classlib do not map correctly to the DFP hardware
         // rounding mode bits.  All other classlib RoundingModes, however, do.

         //the default DFP rounding mode
         if (rm == BigDecimal.ROUND_HALF_EVEN)
            rm = BigDecimal.ROUND_UP;  //correct DFP HW rounding mode for HALF_EVEN
         else if (rm == BigDecimal.ROUND_UP)
            rm = BigDecimal.ROUND_HALF_EVEN; //correct DFP HW rounding mode for HALF_UP

         // defect 144394
         boolean dfpPassed = prec == 16 ?
               DFPAdd(res, rhslaside, lhslaside, 1, 16, rm) :
               DFPAdd(res, rhslaside, lhslaside, 1, prec, rm);
         if(dfpPassed){
            performHardwareUsageHeuristic(set, -3);
            // set res as DFP - (already set to 00)
            passed=true;
         }
      }

      // because DFP add takes the exclusive or of the sign bits
      //for the result, need to make sure result of add 0 by -0
      //doesn't store a negative sign...
      if (passed){
         long reslaside = getlaside(res);
         if(isDFPZero(reslaside)) {
            reslaside &= 0x7FFFFFFFFFFFFFFFL;
            setlaside(res, reslaside);
         }
         return HARDWARE_OPERATION_SUCCESS;
      }
      return HARDWARE_OPERATION_FAIL;
   }

   public int subtract(BigDecimal res, BigDecimal lhs, BigDecimal rhs)  {

      int resExp =-(Math.max(lhs.scale(), rhs.scale()));
      // Precision = 0, rounding = UNNECESSARY
      if (resExp >=-398 && resExp<=369){
         if(DFPScaledSubtract(res, getlaside(rhs), getlaside(lhs), resExp+398)){
            // set res as DFP - (already set to 00)

            // because DFP subtract takes the exclusive or of the sign bits
            //for the result, need to make sure result of subtract 0 by -0
            //doesn't store a negative sign...
            long laside = getlaside(res);
            if(isDFPZero(laside)) {
               laside &= 0x7FFFFFFFFFFFFFFFL;
               setlaside(res, laside);
            }
            return HARDWARE_OPERATION_SUCCESS;

         }
      }
      return HARDWARE_OPERATION_FAIL;
   }

   public int subtract(BigDecimal res, BigDecimal lhs, BigDecimal rhs, MathContext set)  {
      boolean passed = false;
      int prec = set.getPrecision();
      int rm = set.getRoundingMode().ordinal();
      long lhslaside = getlaside(lhs);
      long rhslaside = getlaside(rhs);

      // need to special case this since DFP hardware
      // doesn't conform to BigDecimal API when adding
      // to a 0..
      if (prec != 0 && set.getRoundingMode().ordinal() != BigDecimal.ROUND_UNNECESSARY){
         boolean lZero = isDFPZero(lhslaside);
         boolean rZero = isDFPZero(rhslaside);
         if(lZero || rZero) {
            if(!lZero) {
               clone(lhs, res); // rhs is zero
                if (rhs.scale() > lhs.scale()){
                  if (set.getPrecision()-rhs.precision() >0) {
                     clone(res.setScale(Math.abs(-rhs.scale())), res); //might return BI
                  }
               }
            } else if (!rZero) {
               clone(rhs.negate(), res); // lhs is zero
               if (lhs.scale() > rhs.scale()){
                  if (set.getPrecision()-rhs.precision() >0) {
                     clone(res.setScale(Math.abs(-lhs.scale())), res);  //might return BI
                  }
               }
            } else {
               BigDecimal temp = BigDecimal.valueOf(0, Math.max(lhs.scale(), rhs.scale())); // both operands are zero
               clone(temp, res); //CMVC 136111 -- res could be one of statics (shared)
            }

            if (set != MathContext.UNLIMITED) {
               if(finish(res, set.getPrecision(), set.getRoundingMode().ordinal()) == HARDWARE_OPERATION_FAIL) {
                  return HARDWARE_OPERATION_FAIL;
               }
            }
            return HARDWARE_OPERATION_SUCCESS;
         }
      }



      // at this point, not dealing with 0s
      // fast path for MathContext64
      if (prec == 16 && rm == BigDecimal.ROUND_HALF_EVEN){
         if(DFPSubtract(res, rhslaside, lhslaside, 64, 0, 0)){
            // set res as DFP - (already set to 00)
            passed=true;
         }
      } else if (prec == 0){ // same as DFPScaledSubtract
         int resExp =-(Math.max(lhs.scale(), rhs.scale()));
         if (resExp >=-398 && resExp<=369){
            if(DFPScaledSubtract(res, rhslaside, lhslaside, resExp+398)){
               // we can and out the flags because we don't have
               // a sense of the exponent/precision/sign
               // of this result, nor do we want to use DFP hw
               // to figure it out, so we do not cache anything

               // set res as DFP - (already set to 00)
               passed = true;
            }
         }
      } else if (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY){   // fast path for NO ROUNDING, as well as the ArithmeticException
         if(DFPSubtract(res, rhslaside, lhslaside, 0, 0, 0)){
            performHardwareUsageHeuristic(set, -3); 
            // set res as DFP - (already set to 00)
            // See if we need to throw an arithmetic exception
            if(finish(res, prec, rm) == HARDWARE_OPERATION_FAIL) {
               return HARDWARE_OPERATION_FAIL;
            }
            passed=true;
         }
      } else if(prec <=16){   // Otherwise, if a precision to round to is specified

         /* NOTE:  We do the following two if statements
          * since the constants used for HALF_EVEN and ROUND_UP in
          * the classlib do not map correctly to the DFP hardware
          * rounding mode bits.  All other classlib RoundingModes, however, do.
          */

         //the default DFP rounding mode
         if (rm == BigDecimal.ROUND_HALF_EVEN) {
            rm = BigDecimal.ROUND_UP;  //correct DFP HW rounding mode for HALF_EVEN
         } else if (rm == BigDecimal.ROUND_UP) {
            rm = BigDecimal.ROUND_HALF_EVEN; //correct DFP HW rounding mode for HALF_UP
         }
         // defect 144394
         boolean dfpPassed = prec == 16 ?
               DFPSubtract(res, rhslaside, lhslaside, 1, 16, rm) :
               DFPSubtract(res, rhslaside, lhslaside, 1, prec, rm);
         if(dfpPassed){
            performHardwareUsageHeuristic(set, -3);
            // set res as DFP - (already set to 00)
            passed=true;
         }
      }

      // because DFP subtracts takes the exclusive or of the sign bits
      //for the result, need to make sure result of subtract 0 by -0
      //doesn't store a negative sign...
      if (passed){
         long reslaside = getlaside(res);
         if(isDFPZero(reslaside)) {
            reslaside &= 0x7FFFFFFFFFFFFFFFL;
            setlaside(res, reslaside);
         }
         return HARDWARE_OPERATION_SUCCESS;
      }
      return HARDWARE_OPERATION_FAIL;
   }

   public int multiply(BigDecimal res, BigDecimal lhs, BigDecimal rhs)  {

      int resExp =-(lhs.scale()+rhs.scale());

      /* Precision = 0, rounding = UNNECESSARY */
      if (resExp >=-398 && resExp<=369){
         if(DFPScaledMultiply(res, getlaside(rhs), getlaside(lhs), resExp+398)){
            // set res as DFP - (already set to 00)

            // because DFP subtract takes the exclusive or of the sign bits
            //for the result, need to make sure result of subtract 0 by -0
            //doesn't store a negative sign...
            long laside = getlaside(res);
            if(isDFPZero(laside)) {
               laside &= 0x7FFFFFFFFFFFFFFFL;
               setlaside(res, laside);
            }
            return HARDWARE_OPERATION_SUCCESS;
         }
      }
      return HARDWARE_OPERATION_FAIL;
   }

   public int multiply(BigDecimal res, BigDecimal lhs, BigDecimal rhs, MathContext set)  {

      boolean passed = false;
      int prec = set.getPrecision();
      int rm = set.getRoundingMode().ordinal();
      long lhslaside = getlaside(lhs);
      long rhslaside = getlaside(rhs);


      //  fast path for MathContext64
      if (prec == 16 && rm == BigDecimal.ROUND_HALF_EVEN){
         if(DFPMultiply(res, rhslaside, lhslaside, 64, 0, 0)){
            // set res as DFP - (already set to 00)
            passed = true;
         }
      } else if (prec == 0){ // same as DFPScaledMultiply
         int resExp =-(lhs.scale()+rhs.scale());
         if (resExp >=-398 && resExp<=369){
            if(DFPScaledMultiply(res, rhslaside, lhslaside, resExp+398)){
               // set res as DFP - (already set to 00)
               // because DFP multiply takes the exclusive or of the sign bits
               //for the result, need to make sure result of multiply by 0
               //doesn't store a negative sign...
               passed = true;
            }
         }
      } else if (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY){ // fast path for NO ROUNDING, as well as the ArithmeticException
         if(DFPMultiply(res, rhslaside, lhslaside, 0, 0, 0)){
            // set res as DFP - (already set to 00)

            // See if we need to throw an arithmetic exception
            if(finish(res, prec, rm) == HARDWARE_OPERATION_FAIL) {
               return HARDWARE_OPERATION_FAIL;
            }
            passed=true;
         }
      } else if (prec <= 16) { // Otherwise, if a precision to round to is specified
         /* NOTE:  We do the following two if statements
          * since the constants used for HALF_EVEN and ROUND_UP in
          * the classlib do not map correctly to the DFP hardware
          * rounding mode bits.  All other classlib RoundingModes, however, do.
          */

         //the default DFP rounding mode
         if (rm == BigDecimal.ROUND_HALF_EVEN) {
            rm = BigDecimal.ROUND_UP;  //correct DFP HW rounding mode for HALF_EVEN
         } else if (rm == BigDecimal.ROUND_UP) {
            rm = BigDecimal.ROUND_HALF_EVEN; //correct DFP HW rounding mode for HALF_UP
         }

         // defect 144394
         boolean dfpPassed = prec == 16 ?
               DFPMultiply(res, rhslaside, lhslaside, 1, 16, rm) :
               DFPMultiply(res, rhslaside, lhslaside, 1, prec, rm);
         if(dfpPassed) {
            // set res as DFP - (already set to 00)
            passed =true;
         }
      }

      // because DFP multiply takes the exclusive or of the sign bits
      // for the result, need to make sure result of multiply by 0
      // doesn't store a negative sign...
      if (passed){
         long reslaside = getlaside(res);
         if(isDFPZero(reslaside)) {
            reslaside &= 0x7FFFFFFFFFFFFFFFL;
            setlaside(res, reslaside);
         }
         return HARDWARE_OPERATION_SUCCESS;
      }
      return HARDWARE_OPERATION_FAIL;
   }

   public int divide(BigDecimal res, BigDecimal lhs, BigDecimal rhs)  {

      long rhslaside = getlaside(rhs);
      if(isDFPZero(rhslaside)) {
         badDivideByZero();
      }
      boolean passed = false;
      long lhslaside = getlaside(lhs);

      /*
       * Interpreted return value:
       * Returns -1 if not JIT compiled.
       *
       * JIT compiled return value:
       * Return 1 if JIT compiled and DFP divide is successful.
       * Return 0 if JIT compiled, but DFP divide was inexact
       * Return -2 if JIT compiled, but other exception (i.e. overflow)
       */

      // we need this in order to throw a "non-terminating decimal expansion" error
      int desiredPrecision = (int)Math.min(lhs.precision() + Math.ceil(10*rhs.precision()/3),Integer.MAX_VALUE);

      int ret = DFPDivide(res, rhslaside, lhslaside, true, 0, 0, 0);

      // we passed, just check for non-terminating decimal expansion
      if (ret == 1){
         if (res.precision() > desiredPrecision) {
            /*[MSG "K0460", "Non-terminating decimal expansion"]*/
            throw new ArithmeticException(com.ibm.oti.util.Msg.getString("K0460")); //$NON-NLS-1$
         }
         // set res as DFP - (already set to 00)
         passed=true;
      }

      //otherwise, we had an inexact, or failure... so we'll continue on slow path

      // because DFP divide takes the exclusive or of the sign bits
      //for the result, need to make sure result of multiply by 0
      //doesn't store a negative sign...
      if (passed){
         long reslaside = getlaside(res);
         if(isDFPZero(reslaside)) {
            reslaside &= 0x7FFFFFFFFFFFFFFFL;
            setlaside(res, reslaside);
         }
         return HARDWARE_OPERATION_SUCCESS;
      }

      return HARDWARE_OPERATION_FAIL;
   }

   public int divide(BigDecimal res, BigDecimal lhs, BigDecimal rhs, MathContext set)  {

      long rhslaside = getlaside(rhs);
      if(isDFPZero(rhslaside)) {
         badDivideByZero();
      }
      long lhslaside = getlaside(lhs);
      int prec = set.getPrecision();
      int rm = set.getRoundingMode().ordinal();
      boolean passed=false;

      // fast path for MathContext64
      if (prec == 16 && rm == BigDecimal.ROUND_HALF_EVEN){
         int ret = DFPDivide(res, rhslaside, lhslaside, false, 64, 0, 0);
         if (ret == 1){
            // set res as DFP - (already set to 00)
            passed = true;
         }
      } else if (prec <= 16) {

         //the default DFP rounding mode
         if (rm == BigDecimal.ROUND_HALF_EVEN) {
            rm = BigDecimal.ROUND_UP;  //correct DFP HW rounding mode for HALF_EVEN
         } else if (rm == BigDecimal.ROUND_UP) {
            rm = BigDecimal.ROUND_HALF_EVEN; //correct DFP HW rounding mode for HALF_UP
         }

         // defect 144394
         int ret = prec == 16 ?
               DFPDivide(res, rhslaside, lhslaside, true, 1, 16, rm) :
               DFPDivide(res, rhslaside, lhslaside, true, 1, prec, rm);
         if (ret == 0 && rm == BigDecimal.ROUND_UNNECESSARY) {
            /*[MSG "K0461", "Inexact result requires rounding"]*/
            throw new ArithmeticException(com.ibm.oti.util.Msg.getString("K0461")); //$NON-NLS-1$
         }
         //otherwise, we divide perfectly and returned 1, or divided
         //and got inexact (in the absence of checking for ROUND_UNNECESSARY
         //but that's ok because we had:
         // REREOUND + TGDT + CHECKINEXACT

         //d120228
         if (ret == 1){
            performHardwareUsageHeuristic(set, 10);
            // set res as DFP - (already set to 00)
            passed = true;
         }
      }
      // because DFP divide takes the exclusive or of the sign bits
      //for the result, need to make sure result of multiply by 0
      //doesn't store a negative sign...
      if (passed){
         long reslaside = getlaside(res);
         if(isDFPZero(reslaside)) {
            reslaside &= 0x7FFFFFFFFFFFFFFFL;
            setlaside(res, reslaside);
         }
         return HARDWARE_OPERATION_SUCCESS;
      }

      return HARDWARE_OPERATION_FAIL;
   }

   public int negate(BigDecimal res, MathContext set)  {

      int signum = res.signum();
      int flags = getflags(res);
      long laside = getlaside(res);
      // we're going to cache a new sign bit...
      
      flags |= 0x4;

      // need to flip DFP sign bit and cached sign
      if (signum == -1){
         laside &= 0x7FFFFFFFFFFFFFFFL; //flip DFP sign bit
         flags &= 0xFFFFFF9F; //clear the cached sign bits
         flags |= ( 1 << 5) & 0x60;// ispos
      }
      else if (signum == 1){
         laside |= 0x8000000000000000L; //flip DFP sign bit
         flags &= 0xFFFFFF9F; //clear the cached sign bits
         flags |= (3 << 5) & 0x60; // isneg
      }
      if (getbi(res) != null) {
         setbi(res, getbi(res).negate());
      }
      setflags(res, flags);
      setlaside(res, laside);

      return HARDWARE_OPERATION_SUCCESS;
   }

   public int compareTo(BigDecimal lhs, BigDecimal rhs)  {

      int res = DFPCompareTo(getlaside(lhs), getlaside(rhs));
      if (res != -2) {
         return res;
      }
      return HARDWARE_OPERATION_FAIL;
   }

   public int getScale(BigDecimal bd) {

      int myscale = getscale(bd);
      // have we cached it?
      if ((getflags(bd) & 0x8) != 0) {
         return myscale;
      }

      //caching it
      
      setflags(bd, getflags(bd) | 0x8); //cache on

      int newExp = DFPExponent(getlaside(bd));
      if (newExp == 1000) {
         myscale = -(extractDFPExponent(getlaside(bd))-398);
      } else {
         myscale = -(newExp-398);
      }
      setscale(bd, myscale);
      return myscale;
   }

   public int setScale(BigDecimal bd, int scale)  {

      if (-scale >= -398 && -scale <= 369){
         int ret = DFPSetScale(bd, getlaside(bd), -scale+398, false, 0, true);
         if (ret == 1){
            if(DFPPerformHysteresis()) {
               performHardwareUsageHeuristic(5);
            }

            /* cache - SGP ESRR */

            // set representation to DFP
            // (already 00)

            // because DFPSetScale maintains the sign of the DFP
            // -23 might get scaled down to -0
            long laside = getlaside(bd);
            int flags = getflags(bd);
            if(isDFPZero(laside)){
               laside &= 0x7FFFFFFFFFFFFFFFL;
               flags |= 0x4;
               flags &= 0xFFFFFF9F; //clear signum bits for 0
            }
            else{
               // cache the sign of the src
               flags|= 0x4;
               flags |= (bd.signum()<<5)&0x60;
            }

            // cache the exponent
            flags|=0x8;
            //NOTE:  We do not cache precision!
            flags&=0xFFFFFFEF; //clear prec cache bit

            setscale(bd, scale);
            setflags(bd, flags);
            setlaside(bd, laside);


            return HARDWARE_OPERATION_SUCCESS;
         }
         else if (ret == 0) {
            /*[MSG "K0455", "Requires rounding: {0}"]*/
            throw new ArithmeticException(com.ibm.oti.util.Msg.getString("K0455", Long.toString(scale))); //$NON-NLS-1$
         }
      }

      return HARDWARE_OPERATION_FAIL;
   }

   public int setScale(BigDecimal bd, int scale, int roundingMode)  {

      if (-scale >= -398 && -scale <= 369){

         // If the rounding mode is UNNECESSARY, then we can set
         // the scale as if we were setting in the previous API
         // i.e. with no concern to rounding (the 3rd parameter)
         if (roundingMode == BigDecimal.ROUND_UNNECESSARY){

            //120991 (changed last param from false to true)
            int ret = DFPSetScale(bd, getlaside(bd), -scale+398,false, 0, true);
            if (ret == 1){
               if (DFPPerformHysteresis()){
                  performHardwareUsageHeuristic(5);
               }

               /* cache - SGP ESRR */

               // set representation to DFP
               // (already 00)

               // because DFPSetScale maintains the sign of the DFP
               // -23 might get scaled down to -0
               long laside = getlaside(bd); 
               int flags = 0;
               
               if(isDFPZero(laside)){
                  laside &= 0x7FFFFFFFFFFFFFFFL;
                  flags|=0x4;
                  flags&=0xFFFFFF9F; //clear signum bits for 0
               }
               else{
                  // cache the sign of the src
                  flags|=0x4;
                  flags |=(bd.signum()<<5)&0x60;
               }

               // cache the exponent
               flags|=0x8;
                              
               setscale(bd, scale);
               setflags(bd, flags);
               setlaside(bd, laside);
               return HARDWARE_OPERATION_SUCCESS;

            }
         }
         else{

            //the default DFP rounding mode
            if (roundingMode == BigDecimal.ROUND_HALF_EVEN) {
               roundingMode = BigDecimal.ROUND_UP;  //correct DFP HW rounding mode for HALF_EVEN
            } else if (roundingMode == BigDecimal.ROUND_UP) {
               roundingMode = BigDecimal.ROUND_HALF_EVEN; //correct DFP HW rounding mode for HALF_UP
            }
            int ret = DFPSetScale(bd, getlaside(bd), -scale+398,true, roundingMode, false);
            if (ret == 1){
               if (DFPPerformHysteresis()){
                  performHardwareUsageHeuristic(5);
               }

               /* cache - SGP ESRR */

               // set representation to DFP
               // (already 00)

               // because DFPSetScale maintains the sign of the DFP
               // -23 might get scaled down to -0
               long laside = getlaside(bd);
               int flags = 0;

               if(isDFPZero(laside)){
                  laside &= 0x7FFFFFFFFFFFFFFFL;
                  flags |= 0x4;
                  flags &= 0xFFFFFF9F; //clear signum bits for 0
               }
               else{
                  // cache the sign of the src
                  flags |= 0x4;
                  flags |= (bd.signum()<<5)&0x60;
               }

               // cache the exponent
               flags |= 0x8;
               setscale(bd, scale);
               setflags(bd, flags);
               setlaside(bd, laside);

               return HARDWARE_OPERATION_SUCCESS;
            }
         }
      }
      return HARDWARE_OPERATION_FAIL;
   }

   public String toStringNoDecimal(BigDecimal bd)  {
      // Get the unscaled value;
      long laside = getlaside(bd);
      long lVal = DFPUnscaledValue(laside);
      if (lVal == Long.MAX_VALUE){
         lVal = DFPBCDDigits(laside);
         if (lVal == 10) {
            lVal = extractDFPDigitsBCD(laside);
         }
         // now extract each 4 bit BCD digit from left to right
         long val=0;  //to store the result
         int i=0;
         while (lVal!= 0){
            val += (lVal & 0xF) * powerOfTenLL(i++);
            lVal >>>= 4;
         }
         //is the sign negative?
         return Long.toString(val * bd.signum());
      } else {
         return Long.toString(lVal);
      }
   }

   public String toString(BigDecimal bd, int length)  {
      // Get the exponent
      // Need to store it as a long since we may have
      // stored Long.MIN_VALUE (which needs to be printed
      // to screen as Long.MIN_VALUE
      
      long actualExp = -(long)bd.scale();

      // Get the precision
      int precision = bd.precision();

      // Get the unscaled value;
      long laside = getlaside(bd);
      long bcd = DFPBCDDigits(laside);
      if (bcd == 10)
         bcd = extractDFPDigitsBCD(laside);

      long adjExp= actualExp + precision -1;

      // two cases to consider:
      /*
       * case 1: precision > 1
       *       singlenumber.remaining numbersE(+/-adjusted exponent)
       * case 2: else
       *       numberE(+/-adjusted exponent)
       */

      int expPrecision = numDigits(adjExp);

      // Fill 'er up

      // the character array to fill up
      char [] str;
      int index=0;
      
      if (length <=22)
         str = (char [])thLocalToString.get();
      else
         str = new char [ length ];

      // the sign
      if (bd.signum() == -1){
         str[index++] = '-';
      }

      // the first digit
      str[index++] = (char)(digitAtBCD(bcd,precision,0)|0x0030);

      if (precision > 1){

         // the decimal dot
         str[index++] = '.';

         // rest of the digits
         for (int i=0;i < precision-1 ;i++)
            str[index++] = (char)(digitAtBCD(bcd,precision,i+1)|0x0030);
      }

      // E
      str[index++] = 'E';

      // the +
      if (actualExp>0)
         str[index++] = '+';
      else if (actualExp<0)
         str[index++] = '-';

      // exponent digits
      for (int i=0; i < expPrecision; i++)
         str[index++] = (char)(digitAt(adjExp,expPrecision,i)|0x0030);

      return new String(str, 0, length);
   }

   public String toStringPadded(BigDecimal bd, int sign, int precision, int strlen)  {
      //NOTE :  unscaledValue is in BCD form
      char [] str;
      int actualExp = -bd.scale();
      int signLen=0;
      if (sign == -1)
         signLen = 1;
      long laside = getlaside(bd);
      // Get the unscaled value;
      long unscaledValue= DFPBCDDigits(laside);
      if (unscaledValue == 10) {
         unscaledValue = extractDFPDigitsBCD(laside);
      }
      // if scale is less than precision, won't have
      // any leading zeros, and our number will have a decimal
      // point somewhere in the middle...

      /*
       * 1234 scale 1 = 123.4
       * 1234 scale 2 = 12.34
       * 1234 scale 3 = 1.234
       * 123400 scale 3 = 123.400 <-- need to handle trailing zeros for BI rep
       * 123400 scale 5 = 12340.0 <-- need to handle trailing zeros for BI rep
       *
       * NOTE:  don't need to handle scale <= 0 since this is taken care of
       *         in other branches in toStringHelper
       */
      if (-actualExp < precision){
         int i=0;

         // for LL
         // 1 no need to fill with trailing zeros
         // 2 lay down sign
         // 3 lay down all digits after decimal point
         // 4 lay down digits before decimal point

         int length= signLen + 1 /*for .*/ + precision;

         if (length <=22)
            str = (char [])thLocalToString.get();
         else
            str = new char [ length ];
         int start=0;
         int decimalPointLoc = length-(-actualExp)-1;

         // 2 Place sign
         if (signLen !=0){
            str[start++]='-';
         }

         int curBCD = 0;

         //3 lay down all digits after decimal point
         for (i=(length-1); i > decimalPointLoc; i--){
            curBCD = (int)(unscaledValue & 0xF);
            unscaledValue >>>=4;
            str[i] = (char)(curBCD|0x0030);
         }

         // lay down decimal point
         str[i--]='.';

         //4 lay down all digits before decimal point
         for (;i >=start; i--){
            curBCD = (int) (unscaledValue & 0xF);
            unscaledValue >>>=4;
            str[i] = (char)(curBCD|0x0030);
         }
      }
      else{
         // easy case.. where scale >= precision

         /*
          * 1234 scale 4 = 0.1234
          * 1234 scale 5 = 0.01234
          * 1234 scale 6 = 0.001234
          */

         int numZeros = -actualExp - precision;

         // for both LL & BI
         // 1 fill with zeros
         // 2 lay down sign & lay down decimal point
         // 3 lay down all digits

         int length= signLen + 1 /*for 0*/ +
         1 /*for .*/+ numZeros + precision;

         if (length <=22)
            str = (char [])thLocalToString.get();
         else
            str = new char [ length ];
         int start=0;
         int i=0;

         //1 Fill with 0
         Arrays.fill(str,0,length,'0');

         //2 Place sign
         if (signLen !=0)
            str[start++]='-';

         //2 skip leading zero
         start++;

         //2 Place decimal point
         str[start++]='.';

         //2 skip more leading zeros that occur after decimal point
         start+=(-actualExp - precision);

         // fill up laside bytes (from back to front)
         int curBCD=0;
         for (i=length-1; i >= start; i--){
            curBCD = (int) (unscaledValue & 0xF);
            unscaledValue >>>=4;
            str[i] = (char)(curBCD|0x0030);
         }
      }
      
      return new String(str, 0, strlen);
   }

   public int valueOf(BigDecimal res, long value, int scale) {

      if (DFPUseDFP()) {
         if (value == 0 && scale == 0){
            createZero(res);
            return HARDWARE_OPERATION_SUCCESS;
         }

         if (value <= 9999999999999999L && value >= -9999999999999999L && -scale>= -398 && -scale<= 369){

            // NOTE: When we don't specify rounding in DFP, we'll never
            // fail, so don't bother checking the return value
            if (DFPLongExpConstructor(res, value, -scale + 398, 0, 0, 0, false)){
               int tempFlags = getflags(res);
               /* cache: SGP ESRR */

               // store DFP representation
               // (already 00)

               // cache the exponent for all DFP paths
               tempFlags|=0x8; //cache on
               setscale(res, scale);

               // cache the sign for DFP, on all paths
               tempFlags|=0x4; //cache on
               if (value < 0) {
                  tempFlags |= 0x60; //isneg
               } else if (value > 0) {
                  tempFlags |= 0x20; //ispos
               //iszero is already 00
               }
               setflags(res, tempFlags);
               return HARDWARE_OPERATION_SUCCESS;
            }
         }
      }

      return HARDWARE_OPERATION_FAIL;
   }

   public int round(BigDecimal res, int precision, int roundingMode)  {

      // Only enter here iff precision > 0
      if (res.precision() > precision){ // and only perform if request is shorter then us
         long laside = getlaside(res);
         int flags = getflags(res);
         long bcd = DFPBCDDigits(laside);
         if (bcd == 10) {
            bcd = extractDFPDigitsBCD(laside);
         }

         if (roundingMode== BigDecimal.ROUND_UNNECESSARY){
            if (!allzeroBCD(bcd, res.precision() - precision)) {
               /*[MSG "K0468", "Rounding mode unnecessary, but rounding changes value"]*/
               throw new ArithmeticException(com.ibm.oti.util.Msg.getString("K0468")); //$NON-NLS-1$
            }
         }

         //the default DFP rounding mode
         if (roundingMode == BigDecimal.ROUND_HALF_EVEN) {
            roundingMode = BigDecimal.ROUND_UP;  //correct DFP HW rounding mode for HALF_EVEN
         } else if (roundingMode == BigDecimal.ROUND_UP) {
            roundingMode = BigDecimal.ROUND_HALF_EVEN; //correct DFP HW rounding mode for HALF_UP
         }
         if(!DFPRound(res, laside, precision, roundingMode)) {
            return HARDWARE_OPERATION_FAIL;
         } else {
            /* cache - SGP ESRR */
            // if successful, update the precision cache...
            flags = getflags(res);
            flags |= 0x10; //cache on
            flags &= 0x7F;
            flags |= precision << 7;

            //exponent may have changed, but we don't know
            //to what...
            flags &= 0xFFFFFFF7; //clear exponent cache bit
            setflags(res, flags);
         }

         // if the result is still in DFP and is 0, make
         // sure the sign bit is off...
         laside = getlaside(res);
         if ((flags & 0x3) == 0x0 && isDFPZero(laside)) {
            laside &= 0x7FFFFFFFFFFFFFFFL;
            setlaside(res, laside);
         }
      }

      return HARDWARE_OPERATION_SUCCESS;
   }

   public int convertToLongLookaside(BigDecimal res)  {

      long laside = getlaside(res);
      int flags = getflags(res);
      //quick check for 0
      if (laside == dfpZERO){

         // set representation as long lookaside
         flags &= 0xFFFFFFFC; //clear bits
         flags |= 0x00000001;

         // reset exponent
         setscale(res, 0);
         // set long lookaside
         setlaside(res, 0);

         // cache the precision of 1
         flags &= 0x7F; // clear the cached precision
         flags |= 0x10; //caching the precision
         flags |= 1 << 7; //precision of 1
      }
      else{

         // need to store representation and new long lookaside last
         // since helper functions that are called
         // depend on the correct internal representation flag bit

         int signum = res.signum();;

         //store the exponent
         setscale(res, res.scale());

         //cache the precision
         int prec = res.precision();
         flags &= 0x7F; // clear the cached precision
         flags &= 0xFFFFFFEF;//clear bits
         flags |= 0x10; //cache on
         flags |= prec << 7;

         long lVal = DFPUnscaledValue(laside);
         
         if (lVal == Long.MAX_VALUE){
            lVal = DFPBCDDigits(laside);
            if (lVal == 10) {
               lVal = extractDFPDigitsBCD(laside);
            }
            // now extract each 4 bit BCD digit from left to right
            long val = 0;  //to store the result
            int i = 0;
            while (lVal!= 0) {
               val += (lVal & 0xF) * powerOfTenLL(i++);
               lVal >>>= 4;
            }

            //is the sign negative?
            laside = val * signum;
         } else {
            laside = lVal;
         }

         // store representation as long lookaside
         flags &= 0xFFFFFFFC;
         flags |= 0x00000001;
         setlaside(res, laside);
      }
      setflags(res, flags);
      // need to make sure bi is not cached by previous calls to unscaled value
      setbi(res, null);

      return HARDWARE_OPERATION_SUCCESS;
   }

   public int convertToBigInteger(BigDecimal res)  {

      long laside = getlaside(res);
      int flags = getflags(res);
      //quick check for 0
      if (laside == dfpZERO){

         //store BigInteger representation
         flags &= 0xFFFFFFFC; //clear bits
         flags |= 0x00000002;

         //clear exponent
         setscale(res, 0);

         //store BI
         setbi(res, BigInteger.ZERO);

         // cache the precision of 1
         flags&=0x7F; // clear the cached precision
         flags|=0x10; //caching the precision
         flags |= 1 << 7; //precision of 1
      } else {

         // need to store representation last
         // since helper functions that are called
         // depend on the correct internal representation flag bit

         //store the exponent
         setscale(res, res.scale());

         //store the BigInteger
         
         setbi(res, res.unscaledValue());

         /* cache - SGP ESRR */

         //cache the precision
         int prec = res.precision();
         flags &= 0x7F; // clear the cached precision
         flags |= 0x10; //cache on
         flags |= prec << 7;

         // store representation as BigInteger
         flags &= 0xFFFFFFFC; //clear bits
         flags |= 0x00000002;
      }
      setlaside(res, laside);
      setflags(res, flags);

      return HARDWARE_OPERATION_SUCCESS;
   }

   public int convertToExtension(BigDecimal res)  {

      int flags = getflags(res);
      BigInteger bi = getbi(res);
      int scale = getscale(res);
      long laside =getlaside(res);
      // from BI to DFP
      if ((flags & 0x3) == 0x2 &&
            bi.compareTo(MAXDFP64) <= 0 &&
            bi.compareTo(MINDFP64) >=0 &&
            scale > -369 && scale < 398){

         long lint = bi.longValue();

         // NOTE: When we don't specify rounding in DFP, we'll never
         // fail, so don't bother checking the return value
         if (DFPLongExpConstructor(res, lint, -scale + 398, 0, 0, 0, false)){

            /* cache: SGP ESRR */

            // store DFP representation
            flags &= 0xFFFFFFFC; // clear rep bits
            //sets the bits to (00)

            // cache the exponent
            flags |= 0x8; //cache on
            //res.exp already stores the value

            // cache the sign
            flags |= 0x4; //cache on
            flags &= 0xFFFFFF9F; //clear signum
            if (bi.signum() < 0) {
               flags |= 0x60; //isneg
            } else if (bi.signum() > 0) {
               flags |= 0x20; //ispos
            } else {
               flags &= 0xFFFFFF9F;
            }

            // fix to d124362
            setbi(res, null);
         }
      }

      // from LL to DFP
      else if ((flags & 0x3) == 0x1 &&
            laside <= 9999999999999999L &&
            laside >= -9999999999999999L &&
            scale > -369 && scale < 398){

         int signum = res.signum();

         // NOTE: When we don't specify rounding in DFP, we'll never
         // fail, so don't bother checking the return value
         if (DFPLongExpConstructor(res, laside, -scale + 398, 0, 0, 0, false)){

            /* cache: SGP ESRR */

            // store DFP representation
            flags &= 0xFFFFFFFC; // clear rep bits
            //sets the bits to (00)

            // cache the exponent
            flags |= 0x8; //cache on
            //res.exp already stores the value

            // cache the sign
            flags |= 0x4; //cache on
            flags &= 0xFFFFFF9F; //clear signum
            if (signum < 0) {
               flags |= 0x60; //isneg
            } else if (signum > 0) {
               flags |= 0x20; //ispos
            } else {
               flags &= 0xFFFFFF9F;
            }
         }
      }

      setflags(res, flags);

      return HARDWARE_OPERATION_SUCCESS;
   }

   public int significance(BigDecimal bd)  {

      int tempFlags = getflags(bd);
      long laside = getlaside(bd);
      
      // we're caching it
      tempFlags |= 0x10; // cache on
      tempFlags &= 0x7F; //clear pre-existing bits

      int sig = DFPSignificance(laside);
      if (sig < 0){
         long digits = DFPBCDDigits(laside);
         if (digits == 10){
            digits = extractDFPDigitsBCD(laside);
         }
         int nlz = Long.numberOfLeadingZeros(digits);
         nlz>>=2;
         nlz=16-nlz;

         // Preceding algorithm algorithm would return 0 for 0
         // and we need it to return a precision of 1
         if (nlz == 0)
            nlz++;


         tempFlags |= nlz << 7;
         setflags(bd, tempFlags);
         return nlz;
      }
      else{
         // DFPSignificance would return 0 for 0
         // and we need it to return a precision of 1
         if (sig ==0) {
            sig++;
         }
         tempFlags |= sig << 7;
         setflags(bd, tempFlags);
         return sig;
      }
   }

   public int signum(BigDecimal bd)  {

      int tempFlags = getflags(bd);
      long laside = getlaside(bd); 
      // is it cached?
      if ((tempFlags&0x4)!=0) {
         return (((tempFlags & 0x00000060) << 25) >>30);
      }

      //we're going to cache it
      tempFlags |= 0x4;  //cache on

      //check for negative first
      if ((laside & 0x8000000000000000L) == 0x8000000000000000L){
         tempFlags |= 0x60; //store negative
         setflags(bd, tempFlags);
         return -1;
      }

      //now we're checking for positive or zero
      long mask = DFPBCDDigits(laside);
      if (mask == 10){
         //still haven't jitted the method
         if (isDFPZero(laside)){
            tempFlags&=0xFFFFFF9F; //clear the signum cache (00)
            setflags(bd, tempFlags);
            return 0;
         } else {
            tempFlags&=0xFFFFFF9F; //clear the signum cache
            tempFlags|=0x20; //store positive
            setflags(bd, tempFlags);
            return 1;
         }
      } else if (mask !=0) {
         tempFlags&=0xFFFFFF9F; //clear the signum cache
         tempFlags|=0x20; //store positive
         setflags(bd, tempFlags);
         return 1;
      } else {
         tempFlags&=0xFFFFFF9F; //clear the signum cache (00)
      }
      setflags(bd, tempFlags);
      return 0;
   }

   public BigInteger unscaledValue(BigDecimal bd)  {
      long laside = getlaside(bd);
      long lVal = DFPUnscaledValue(laside);
      if (lVal != Long.MAX_VALUE) {
         setbi(bd, BigInteger.valueOf(lVal));
         return getbi(bd);
      } else {
         lVal = DFPBCDDigits(laside);
         if (lVal == 10) {
            lVal = extractDFPDigitsBCD(laside);
         }

         //check for zero
         if (lVal == 0) {
            setbi(bd, BigInteger.ZERO);
            return getbi(bd);
         }

         // now extract each 4 bit BCD digit from left to right
         long val = 0;  //to store the result
         int i = 0;
         while (lVal != 0){
            val += (lVal & 0xF) * powerOfTenLL(i++);
            lVal >>>= 4;
         }
         setbi(bd, BigInteger.valueOf(bd.signum() * val));
         return getbi(bd);
      }
   }

   public int createZero(BigDecimal res)  {
      setlaside(res, dfpZERO);
      setbi(res, null);
      setscale(res, 0);
      int flags = getflags(res);

      flags|=0x4;  //cache on

      // cache the precision of 1
      flags&=0x7F; // clear the cached precision
      flags|=0x10; //caching the precision
      flags |= 1 << 7; //precision of 1

      // cache the exponent (exp already 0)
      flags|=0x8;  //cache on
      setflags(res, flags);

      return HARDWARE_OPERATION_SUCCESS;
   }

   public int intConstructor(BigDecimal res, int value, MathContext set)  {
      // quick path for 0e0
      if (value == 0){
         createZero(res);
         return HARDWARE_OPERATION_SUCCESS;
      }

      // cache the exp for all DFP paths
      // exp remains 0
      int flags = getflags(res);
      flags |= 0x8; //cache on

      // cache the sign for DFP, on all paths
      flags|=0x4; //cache on
      if (value < 0) {
         flags|=0x60; //isneg
      } else if (value > 0) {
         flags|=0x20; //ispos
      }
      //iszero is already 00

      // we're going to take the full blown path to
      // constructing a DFP internal representation

      int prec = set.getPrecision();
      int rm = set.getRoundingMode().ordinal();

      // fast path for MathContext64
      if (prec == 16 && rm == BigDecimal.ROUND_HALF_EVEN){
         if (DFPIntConstructor(res, value, 64, 0, 0)){

            // We assume in the worst case that the precision and
            // exp remain the same since the number of digits
            // is at most 16.  If we passed in 999999999999999999be767
            // and rounded to +inf, we'd get overflow, fail and take
            // the slow path anyway.

            /* cache: SGP ESRR */

            // cache the precision
            flags |= 0x10;
            flags |= numDigits(value) << 7;

            // store DFP representation
            // (representation bits already 00)
            setflags(res, flags);
            return HARDWARE_OPERATION_SUCCESS;
         }
      }

      // fast path for NO ROUNDING, as well as the ArithmeticException
      if ((prec == 0) || (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY) ||
            (prec > 16)){

         /* This case catches:
          *    -when no rounding is required
          *  -if rounding is unnecessary and precision !=0, check
          *  to see that result wasn't inexact (via call to Finish)
          *  (the latter satisfies the API description of :
          *  ArithmeticException - if the result is inexact but
          *  the rounding mode is UNNECESSARY.
          */

         if (DFPIntConstructor(res, value, 0, 0, 0)){

            /* cache: SGP ESRR */

            // store DFP representation
            // (representation bits already 00)

            // See if we need to throw an arithmetic exception

            /*[IF]*/
            //TODO: Return a special value in DFP hardware to improve
            //      this case
            /*[ENDIF]*/

            if (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY) {
               setflags(res, flags);
               if(finish(res, prec, rm) == HARDWARE_OPERATION_FAIL) {
                  return HARDWARE_OPERATION_FAIL;
               }
            } else {
               // we can cache the precision and exp
               // since no rounding meant that (no rounding)

               // cache the precision
               flags |= 0x10; //cache on
               flags |= numDigits(value) << 7;
               setflags(res, flags);

            }
            return HARDWARE_OPERATION_SUCCESS;
         }
      // Otherwise, if a precision to round to is specified
      } else if (prec <= 16){

         /* NOTE:  We do the following two if statements
          * since the constants used for HALF_EVEN and ROUND_UP in
          * the classlib do not map correctly to the DFP hardware
          * rounding mode bits.  All other classlib RoundingModes, however, do.
          */

         //the default DFP rounding mode
         if (rm == BigDecimal.ROUND_HALF_EVEN) {
            rm = BigDecimal.ROUND_UP;  //correct DFP HW rounding mode for HALF_EVEN
         } else if (rm == BigDecimal.ROUND_UP) {
            rm = BigDecimal.ROUND_HALF_EVEN; //correct DFP HW rounding mode for HALF_UP
         }

         // now construct in hardware if possible
         if(DFPIntConstructor(res, value, 1, prec, rm)){

            /* cache: SGP ESRR */

            // store DFP representation
            // (representation bits already 00)

            //don't try to cache precision/exp since
            //prec might be different precision(val)

            //so turn cache of exp off...
            flags &= 0xFFFFFFF7;

            return HARDWARE_OPERATION_SUCCESS;
         }
      }
      return HARDWARE_OPERATION_FAIL;
   }

   public int longConstructor(BigDecimal res, long value, int scale, MathContext set)  {
      // don't want to send in 0 and then round with hardware
      // cause it might place a crummy exponent value...
      if (value == 0){
         createZero(res);
         return HARDWARE_OPERATION_SUCCESS;
      // otherwise, make sure the long is within 64-bit DFP range
      } else if (value <=9999999999999999L && value >= -9999999999999999L &&
            -scale>=-398 && -scale<369){
         int flags = getflags(res);
         /* cache: SGP ESRR */

         // cache the sign for DFP, on all paths
         flags|=0x4; //cache on

         if (value < 0) {
            flags|=0x60; //isneg
         } else if (value > 0) {
            flags|=0x20; //ispos
         }
         //iszero is already 00

         int prec = set.getPrecision();
         int rm = set.getRoundingMode().ordinal();

         // fast path for MathContext64
         if (prec == 16 && rm == BigDecimal.ROUND_HALF_EVEN){
            if(DFPLongExpConstructor(res, value, -scale + 398, 64, 0, 0, false)){

               // We assume in the worst case that the precision and
               // exponent remain the same since the number of digits
               // is at most 16.  If we passed in 999999999999999999be767
               // and rounded to +inf, we'd get overflow, fail and take
               // the slow path anyway.

               /* cache: SGP ESRR */

               // cache the precision
               flags |= 0x10; //cache on
               flags |= numDigits(value) << 7;

               // cache the exponent
               flags |= 0x8; //cache on

               setflags(res, flags);
               setscale(res, scale);


               //store DFP representation
               // (already set to 00)

               return HARDWARE_OPERATION_SUCCESS;
            }
         }

         // fast path for NO ROUNDING, as well as the ArithmeticException
         if ((prec == 0) || (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY)
               || (prec > 16)){

            /* This case catches:
             *    -when no rounding is required
             *  -if rounding is unnecessary and precision !=0, check
             *  to see that result wasn't inexact (via call to Finish)
             *  (the latter satisfies the API description of :
             *  ArithmeticException - if the result is inexact but
             *  the rounding mode is UNNECESSARY.
             */

            // NOTE: When we don't specify rounding in DFP, we'll never
            // fail, so don't bother checking the return value
            if (this.DFPLongExpConstructor(res, value, -scale + 398, 0, 0, 0, false)){

               // store DFP representation
               // (already set to 00)

               /*[IF]*/
               //TODO: Return a special value in DFP hardware to improve
               //      this case
               /*[ENDIF]*/

               // See if we need to throw an arithmetic exception
               if (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY) {
                  setflags(res, flags);
                  if(finish(res, prec, rm) == HARDWARE_OPERATION_FAIL) {
                     return HARDWARE_OPERATION_FAIL;
                  }
               } else {

                  // cache the exponent
                  flags|=0x8; //cache on

                  // cache the precision
                  flags |= 0x10; //cache on
                  flags |= numDigits(value) << 7;

                  setflags(res, flags);
                  setscale(res, scale);
               }
               return HARDWARE_OPERATION_SUCCESS;
            }
         // Otherwise, if a precision to round to is specified
         } else if (prec <=16){

            /* NOTE:  We do the following two if statements
             * since the constants used for HALF_EVEN and ROUND_UP in
             * the classlib do not map correctly to the DFP hardware
             * rounding mode bits.  All other classlib RoundingModes, however, do.
             */

            //the default DFP rounding mode
            if (rm == BigDecimal.ROUND_HALF_EVEN) {
               rm = BigDecimal.ROUND_UP;  //correct DFP HW rounding mode for HALF_EVEN
            } else if (rm == BigDecimal.ROUND_UP) {
               rm = BigDecimal.ROUND_HALF_EVEN; //correct DFP HW rounding mode for HALF_UP
            }

            // now construct in hardware if possible
            if(DFPLongExpConstructor(res, value, -scale + 398, 1, prec, rm, false)){

               //store DFP representation
               // (already 00)

               //don't try to cache precision/exponent since
               //prec might be different precision(val)

               // so turn caching of exponent off
               setflags(res, flags);

               return HARDWARE_OPERATION_SUCCESS;
            }
         }
      }

      return HARDWARE_OPERATION_FAIL;
   }

   public int longConstructor(BigDecimal res, long value, int scale, int nDigits, int sign, MathContext set)  {
      int prec = set.getPrecision();
      int rm = set.getRoundingMode().ordinal();

      // fast path for 0e0
      if (value == 0 && scale == 0){
         createZero(res);
         return HARDWARE_OPERATION_SUCCESS;
      }

      /* cache - SGP ESRR */

      int flags = getflags(res);

      // cache the sign for DFP on all paths..
      flags|=0x4; //cache on
      if (value != 0) { //if not-zero, then we set the val
         flags|=sign;
      }

      // cache the exponent for DFP on all paths..
      flags |= 0x8; //cache on
      setscale(res, -scale);

      // fast path for MathContext64
      if (prec == 16 && rm == BigDecimal.ROUND_HALF_EVEN){
         if (sign == 0x60){ //isneg
            if (DFPLongExpConstructor(res, value, scale + 398, 64, 0, 0, true)){

               // store DFP representation
               // (already 00)
               long laside = getlaside(res);
               // since we use unsigned BCDs to get full 16 digits worth
               // lets flip the DFP's sign bit to indicate the fact...
               if (sign == 0x60) { //inseg
                  laside |= 0x8000000000000000l;
               }
               setlaside(res, laside);
               /* cache - SGP ESRR */

               // cache the precision
               flags |= 0x10;
               flags |= nDigits << 7;

               setflags(res, flags);
               if(finish(res, prec, rm) == HARDWARE_OPERATION_FAIL) {
                  return HARDWARE_OPERATION_FAIL;
               }
               return HARDWARE_OPERATION_SUCCESS;
            }
         }
         else{
            if(DFPLongExpConstructor(res, value, scale + 398, 64, 0, 0, true)){

               // We assume in the worst case that the precision and
               // exponent remain the same since the number of digits
               // is at most 16.  If we passed in 999999999999999999be767
               // and rounded to +inf, we'd get overflow, fail and take
               // the slow path anyway.

               // cache the precision
               flags|=0x10;
               flags |= nDigits << 7;

               setflags(res, flags);

               //store DFP representation
               //(already 00)
               return HARDWARE_OPERATION_SUCCESS;
            }
         }
      }

      // fast path for NO ROUNDING, as well as the ArithmeticException
      if ((prec == 0) || (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY) ||
            (prec > 16)){

         /* This case catches:
          *    -when no rounding is required
          *  -if rounding is unnecessary and precision !=0, check
          *  to see that result wasn't inexact (via call to Finish)
          *  (the latter satisfies the API description of :
          *  ArithmeticException - if the result is inexact but
          *  the rounding mode is UNNECESSARY.
          */
         if (DFPLongExpConstructor(res, value, scale + 398, 0, 0, 0, true)){

            // store DFP representation
            // (already 00)

            // since we use unsigned BCDs to get full 16 digits worth
            // lets flip the DFP's sign bit to indicate the fact...
            long laside = getlaside(res);
            if (sign == 0x60) { //isneg
               laside |= 0x8000000000000000l;
            }
            setlaside(res, laside);
            // See if we need to throw an arithmetic exception
            if (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY){

               // since we use unsigned BCDs to get full 16 digits worth
               // lets flip the DFP's sign bit to indicate the fact...
               if (sign == 0x60) { //inseg
                  laside |= 0x8000000000000000l;
               }
               setlaside(res, laside);
               setflags(res, flags);

               if(finish(res, prec, rm) == HARDWARE_OPERATION_FAIL) {
                  return HARDWARE_OPERATION_FAIL;
               }
            }
            else{
               // we can cache the precision and exponent
               // since no rounding meant that (no rounding)
               // cache the precision
               flags|=0x10;
               flags |= nDigits << 7;
               setflags(res, flags);
               

            }
            return HARDWARE_OPERATION_SUCCESS;
         }
      }

      // Otherwise, if a precision to round to is specified
      else if (prec <=16){

         /* NOTE:  We do the following two if statements
          * since the constants used for HALF_EVEN and ROUND_UP in
          * the classlib do not map correctly to the DFP hardware
          * rounding mode bits.  All other classlib RoundingModes, however, do.
          */

         // for negative BCDs
         if (sign == 0x60){
            if (DFPLongExpConstructor(res, value, scale + 398, 0, 0, 0, true)){

               // store DFP representation
               // (already 00)

               // since we use unsigned BCDs to get full 16 digits worth
               // lets flip the DFP's sign bit to indicate the fact...
               long laside = getlaside(res);
               laside |= 0x8000000000000000l;
               setlaside(res, laside);

               /* cache - SGP ESRR */

               // can't store precision since it's going to be prec,
               // but isn't just yet...
               setflags(res, flags);
               if(finish(res, prec, rm) == HARDWARE_OPERATION_FAIL) {
                  return HARDWARE_OPERATION_FAIL;
               }
               return HARDWARE_OPERATION_SUCCESS;
            }
         }
         else{

            // NOTE:  We do the following rm reversal here because
            // doing so in common code above would cause the eventual
            // call to roundDFP to switch them back.

            // the default DFP rounding mode
            if (rm == BigDecimal.ROUND_HALF_EVEN) {
               rm = BigDecimal.ROUND_UP;  //correct DFP HW rounding mode for HALF_EVEN
            } else if (rm == BigDecimal.ROUND_UP) {
               rm = BigDecimal.ROUND_HALF_EVEN; //correct DFP HW rounding mode for HALF_UP
            }
            // for positive BCDs
            if(DFPLongExpConstructor(res, value, scale + 398, 1, prec, rm, true)){

               //store DFP representation
               //(already 00)

               //don't try to cache precision/exponent since
               //exp might be diff due to rounding, and precision
               //may be larger than actual

               /* cache - SGP ESRR */

               //reset caching of exponent
               flags&=0xFFFFFFF7;
               setflags(res, flags);


               return HARDWARE_OPERATION_SUCCESS;
            }
         }
      }
      //at this point, DFP paths were unsuccessful, want to stick with BI
      return HARDWARE_OPERATION_FAIL;
   }

   public int bigIntegerConstructor(BigDecimal res, BigInteger bi, int scale, MathContext set)  {

      int biPrecision = bi.toString().length() + ((bi.signum() ==-1) ? -1 : 0);

      if (biPrecision < 17 && -scale <= 369 && -scale >= -398){

         // get the long value with the appropriate sign
         long val = bi.longValue();

         // quick path for 0e0
         if (val == 0 && scale == 0){
            createZero(res);
            return HARDWARE_OPERATION_SUCCESS;
         }

         // otherwise, we'll need to perform DFP construction
         // using the correct precision/roundingmodes

         int prec = set.getPrecision();
         int rm = set.getRoundingMode().ordinal();

         int flags = getflags(res);
         // fast path for MathContext64
         if (prec == 16 && rm == BigDecimal.ROUND_HALF_EVEN){
            if(DFPLongExpConstructor(res, val, -scale + 398, 64, 0, 0, false)){

               // We assume in the worst case that the precision and
               // exponent remain the same since the number of digits
               // is at most 16.  If we passed in 999999999999999999be767
               // and rounded to +inf, we'd get overflow, fail and take
               // the slow path anyway.

               /* cache - SGP ESRR */

               // cache the precision
               flags |= 0x10; //cache on
               flags |= numDigits(val) << 7;

               // cache the sign
               flags|=0x4; //cache on
               flags|= ((bi.signum() <<5) & 0x60);

               //cache the exponent
               flags|=0x8; //cache on

               setflags(res, flags);
               setscale(res, scale);

               // store DFP representation
               // (representation bits already 00)

               return HARDWARE_OPERATION_SUCCESS;
            }
         }

         // fast path for NO ROUNDING, as well as the ArithmeticException
         if ((prec == 0) || (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY)
               || (prec > 16)){

            /* This case catches:
             *    -when no rounding is required
             *  -if rounding is unnecessary and precision !=0, check
             *  to see that result wasn't inexact (via call to Finish)
             *  (the latter satisfies the API description of :
             *  ArithmeticException - if the result is inexact but
             *  the rounding mode is UNNECESSARY.
             */

            // NOTE: When we don't specify rounding in DFP, we'll never
            // fail, so don't bother checking the return value
            if (DFPLongExpConstructor(res, val, -scale + 398, 0, 0, 0, false)){
               /* cache - SGP ESRR */

               // cache the sign
               flags |= 0x4; // cache on
               flags |= ((bi.signum() <<5) & 0x60);

               // store DFP representation
               // (representation bits already 00)

               // See if we need to throw an arithmetic exception

               /*[IF]*/
               //TODO: Return a special value in DFP hardware to improve
               //      this case
               /*[ENDIF]*/

               if (prec > 0 && rm == BigDecimal.ROUND_UNNECESSARY) {
                  setflags(res, flags);
                  if(finish(res, prec, rm) == HARDWARE_OPERATION_FAIL) {
                     return HARDWARE_OPERATION_FAIL;
                  }
               } else {
                  // we can cache the precision and exponent
                  // since no rounding left things as is

                  // cache the precision
                  flags|=0x10; // cache on
                  flags |= numDigits(val) << 7;

                  //cache the exponent
                  flags|=0x8; // cache on

                  setflags(res, flags);
                  setscale(res, scale);
               }
               return HARDWARE_OPERATION_SUCCESS;
            }
         }

         // Otherwise, if a precision to round to is specified
         else if (prec <=16){

            /* NOTE:  We do the following two if statements
             * since the constants used for HALF_EVEN and ROUND_UP in
             * the classlib do not map correctly to the DFP hardware
             * rounding mode bits.  All other classlib RoundingModes, however, do.
             */

            //the default DFP rounding mode
            if (rm == BigDecimal.ROUND_HALF_EVEN) {
               rm = BigDecimal.ROUND_UP;  //correct DFP HW rounding mode for HALF_EVEN
            } else if (rm == BigDecimal.ROUND_UP) {
               rm = BigDecimal.ROUND_HALF_EVEN; //correct DFP HW rounding mode for HALF_UP
            }

            // now construct in hardware if possible
            if(DFPLongExpConstructor(res, val,-scale + 398, 1, prec, rm, false)){

               /* cache - SGP ESRR */

               // cache the sign
               flags|=0x4; //cache on
               flags|=((bi.signum() <<5) & 0x60);
               setflags(res, flags);

               //store DFP representation
               // (representation bits already 00)

               //don't try to cache precision/exponent since
               //prec might be different after rounding
               return HARDWARE_OPERATION_SUCCESS;
            }
         }
      }
      return HARDWARE_OPERATION_FAIL;
   }
   
   





















   // DO NOT MODIFY THIS METHOD
   /* The method is only called once to setup the flag DFP_HW_AVAILABLE
    *  Return value
    *    true - when JIT compiled this method, replaces it with loadconst 1
    *       if user -Xnodfp hasn't been supplied
    *    false - if still interpreting this method or disabled by VM option
    */
   private final static boolean DFPCheckHWAvailable() {
      return false;
   }
   
   // DO NOT MODIFY THIS METHOD
   /*
    *  Return value
    *    true - when JIT compiled this method, replaces it with loadconst 1
    *       if user -Xnodfp hasn't been supplied
    *    false - if still interpreting this method or disabled by VM option
    */
   private final static boolean DFPHWAvailable(){
      return DFP_HW_AVAILABLE;
   }

   /*
    *  Return value
    *    true - when JIT compiled this method, replaces it with loadconst 1
    *      if user -Xjit:disableDFPHys hasn't been supplied
    *    false - if still interpreting this method or disabled by JIT option
    */
   private final static boolean DFPPerformHysteresis(){
      return false;
   }

   /*
    *  Return value
    *    true - when JIT compiled this method, replaces it with loadconst 1
    *    if -Xjit:disableDFPHys has been supplied OR when hys_type becomes true
    *    false - when hys_type is false
    */
   private final static boolean DFPUseDFP(){
      return hys_type;
   }

   // DO NOT MODIFY THIS METHOD

   /* NOTE:  The fact we return a boolean means we kill a register
    *        in the cases we don't want to perform rounding.
    *        i.e. rndFlag = 0 or 64 since we must load and
    *        return a 1 value in the generated JIT code.
    */
   private final static boolean DFPIntConstructor(BigDecimal bd, int val, int rndFlag, int prec, int rm){
      return false;
    }

   // DO NOT MODIFY THIS METHOD
   /*[IF]*/
   //TODO:  Use this for the longConstructor when it's called with scale=0
   /*[ENDIF]*/
   private final static boolean DFPLongConstructor(BigDecimal bd, long val, int rndFlag, int prec, int rm){
      return false;
   }

   // DO NOT MODIFY THIS METHOD
   /*
    *  Interpreted return value:
    *    true - never
    *    false - always
    *
    *  Jitted return value:
    *    true - if rounding succeeded
    *    false - if rounding failed
    *
    *  Parameters:
    *    val - long to be converted to DFP
    *    biasedExp - biased exponent to be inserted into DFP
    *    rndFlag
    *          =0 - no rounding
    *       =1 - rounding according to prec, rm
    *       =64 - rounding according to MathContext64
    *    prec - precision to round constructed DFP to
    *    rm - rm to use
    *    bcd - whether long is in bcd form
    *
    */
   private final static boolean DFPLongExpConstructor(BigDecimal bd, long val, int biasedExp, int rndFlag, int prec, int rm, boolean bcd){
      return false;
   }

   // DO NOT MODIFY THIS METHOD
   private final static boolean DFPScaledAdd(BigDecimal bd, long lhsDFP,long rhsDFP,int biasedExp){
      return false;
   }

   // DO NOT MODIFY THIS METHOD
   /*
    *  Parameters:
    *    rndFlag
    *          =0 - no rounding
    *       =1 - rounding according to prec, rm
    *       =64 - rounding according to MathContext64
    */
   private final static boolean DFPAdd(BigDecimal bd, long lhsDFP,long rhsDFP, int rndFlag, int precision, int rm){
      return false;
   }

   // DO NOT MODIFY THIS METHOD
   private final static boolean DFPScaledSubtract(BigDecimal bd, long lhsDFP,long rhsDFP,int biasedExp){
      return false;
   }

   // DO NOT MODIFY THIS METHOD
   /*
    *  Parameters:
    *    rndFlag
    *          =0 - no rounding
    *       =1 - rounding according to prec, rm
    *       =64 - rounding according to MathContext64
    */
   private final static boolean DFPSubtract(BigDecimal res, long lhsDFP, long rhsDFP, int rndFlag, int precision, int rm){
      return false;
   }

   // DO NOT MODIFY THIS METHOD
   private final static boolean DFPScaledMultiply(BigDecimal res, long lhsDFP,long rhsDFP,int biasedExp){
      return false;
   }

   // DO NOT MODIFY THIS METHOD
   /*
    *  Parameters:
    *    rndFlag
    *          =0 - no rounding
    *       =1 - rounding according to prec, rm
    *       =64 - rounding according to MathContext64
    */
   private final static boolean DFPMultiply(BigDecimal res, long lhsDFP, long rhsDFP, int rndFlag, int precision, int rm){
      return false;
   }

   // DO NOT MODIFY THIS METHOD
   /*
    * Interpreted return value:
    * Returns -1 if not JIT compiled.
    *
    * JIT compiled return value:
    * 0 - ok, but inexact exception --> check UNNECESSARY
    * 1 - ok, no inexact
    * -1 - not ok --> try slow path
    *
    *  rndFlag
    *    =0 - rounding according to MathContext64
    *  =1 - rounding according to prec, rm
    */
   private final static int DFPScaledDivide(BigDecimal res, long lhsDFP, long rhsDFP, int scale, int rndFlg, int rm){
      return -1;
   }

   // DO NOT MODIFY THIS METHOD
   /*
    * Interpreted return value:
    *       -1 = always
    *
    * JIT compiled return value:
    * Return 1 if JIT compiled and DFP divide is successful.
    * Return 0 if JIT compiled, but DFP divide was inexact
    * Return -1 if JIT compiled, but other exception (i.e. overflow)
    *
    * rndFlag
    *    =0 - no rounding
    *  =1 - rounding according to prec, rm
    *  =64 - rounding according to MathContext64
    */
   private final static int DFPDivide(BigDecimal bd, long lhsDFP, long rhsDFP, boolean checkForInexact, int rndFlag, int prec, int rm){
      return -1;  //return an invalid result
   }

   // DO NOT MODIFY THIS METHOD
   private final static boolean DFPRound(BigDecimal bd, long dfpSrc, int precision, int rm){
      return false;
   }

   // DO NOT MODIFY THIS METHOD
   private final static int DFPCompareTo(long lhsDFP, long rhsDFP){
      return -2; //return an invalid result
   }

   // DO NOT MODIFY THIS METHOD
   private final static long DFPBCDDigits(long dfp){
      return 10; //since each digit in a BCD is from 0 to 9
   }

   // DO NOT MODIFY THIS METHOD
   private final static int DFPSignificance(long dfp){
      return -1; //illegal significance
   }

   // DO NOT MODIFY THIS METHOD
   private final static int DFPExponent(long dfp){
      return 1000; //return a value out of the range of Double DFP
   }

   // DO NOT MODIFY THIS METHOD
   /*
    * Interpreted return value:
    *       -1 = always
    *
    * JIT compiled return value:
    * Return 1 if passed
    * Return 0 if JIT compiled, but inexact result
    * Return -1 if JIT compiled, but other failure
    *
    */
   private final static int DFPSetScale(BigDecimal bd, long srcDFP, int biasedExp, boolean round, int rm, boolean checkInexact){
      return -1;
   }

   // DO NOT MODIFY THIS METHOD
   private final static long DFPUnscaledValue(long srcDFP){
      return Long.MAX_VALUE;
   }   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   private static final ThreadLocal thLocalToString = new ThreadLocal(){
      /* 22 is the best number for the fastest path
       * [-]digits.digits when scale > 0 & scale <=19
       */

      protected synchronized Object initialValue() {
            return new char[22];
        }
   }; 
   
   private final static byte [] comboinit(){
      final byte comboCode[]={
            0, 4, 8, 12, 16, 20, 24, 28,
            1, 5, 9, 13, 17, 21, 25, 29,
            2, 6, 10, 14, 18, 22, 26, 30,
            32, 36, 33, 37, 34, 38
      };
      return comboCode;
   }
   
   private final static short [] dpd2bcdinit(){
      final short[] dpd2bcd = {
            0,    1,    2,    3,    4,    5,    6,    7,
            8,    9,  128,  129, 2048, 2049, 2176, 2177,   16,   17,   18,   19,   20,
            21,   22,   23,   24,   25,  144,  145, 2064, 2065, 2192, 2193,   32,   33,
            34,   35,   36,   37,   38,   39,   40,   41,  130,  131, 2080, 2081, 2056,
            2057,   48,   49,   50,   51,   52,   53,   54,   55,   56,   57,  146,  147,
            2096, 2097, 2072, 2073,   64,   65,   66,   67,   68,   69,   70,   71,   72,
            73,  132,  133, 2112, 2113,  136,  137,   80,   81,   82,   83,   84,   85,
            86,   87,   88,   89,  148,  149, 2128, 2129,  152,  153,   96,   97,   98,
            99,  100,  101,  102,  103,  104,  105,  134,  135, 2144, 2145, 2184, 2185,
            112,  113,  114,  115,  116,  117,  118,  119,  120,  121,  150,  151, 2160,
            2161, 2200, 2201,  256,  257,  258,  259,  260,  261,  262,  263,  264,  265,
            384,  385, 2304, 2305, 2432, 2433,  272,  273,  274,  275,  276,  277,  278,
            279,  280,  281,  400,  401, 2320, 2321, 2448, 2449,  288,  289,  290,  291,
            292,  293,  294,  295,  296,  297,  386,  387, 2336, 2337, 2312, 2313,  304,
            305,  306,  307,  308,  309,  310,  311,  312,  313,  402,  403, 2352, 2353,
            2328, 2329,  320,  321,  322,  323,  324,  325,  326,  327,  328,  329,  388,
            389, 2368, 2369,  392,  393,  336,  337,  338,  339,  340,  341,  342,  343,
            344,  345,  404,  405, 2384, 2385,  408,  409,  352,  353,  354,  355,  356,
            357,  358,  359,  360,  361,  390,  391, 2400, 2401, 2440, 2441,  368,  369,
            370,  371,  372,  373,  374,  375,  376,  377,  406,  407, 2416, 2417, 2456,
            2457,  512,  513,  514,  515,  516,  517,  518,  519,  520,  521,  640,  641,
            2050, 2051, 2178, 2179,  528,  529,  530,  531,  532,  533,  534,  535,  536,
            537,  656,  657, 2066, 2067, 2194, 2195,  544,  545,  546,  547,  548,  549,
            550,  551,  552,  553,  642,  643, 2082, 2083, 2088, 2089,  560,  561,  562,
            563,  564,  565,  566,  567,  568,  569,  658,  659, 2098, 2099, 2104, 2105,
            576,  577,  578,  579,  580,  581,  582,  583,  584,  585,  644,  645, 2114,
            2115,  648,  649,  592,  593,  594,  595,  596,  597,  598,  599,  600,  601,
            660,  661, 2130, 2131,  664,  665,  608,  609,  610,  611,  612,  613,  614,
            615,  616,  617,  646,  647, 2146, 2147, 2184, 2185,  624,  625,  626,  627,
            628,  629,  630,  631,  632,  633,  662,  663, 2162, 2163, 2200, 2201,  768,
            769,  770,  771,  772,  773,  774,  775,  776,  777,  896,  897, 2306, 2307,
            2434, 2435,  784,  785,  786,  787,  788,  789,  790,  791,  792,  793,  912,
            913, 2322, 2323, 2450, 2451,  800,  801,  802,  803,  804,  805,  806,  807,
            808,  809,  898,  899, 2338, 2339, 2344, 2345,  816,  817,  818,  819,  820,
            821,  822,  823,  824,  825,  914,  915, 2354, 2355, 2360, 2361,  832,  833,
            834,  835,  836,  837,  838,  839,  840,  841,  900,  901, 2370, 2371,  904,
            905,  848,  849,  850,  851,  852,  853,  854,  855,  856,  857,  916,  917,
            2386, 2387,  920,  921,  864,  865,  866,  867,  868,  869,  870,  871,  872,
            873,  902,  903, 2402, 2403, 2440, 2441,  880,  881,  882,  883,  884,  885,
            886,  887,  888,  889,  918,  919, 2418, 2419, 2456, 2457, 1024, 1025, 1026,
            1027, 1028, 1029, 1030, 1031, 1032, 1033, 1152, 1153, 2052, 2053, 2180, 2181,
            1040, 1041, 1042, 1043, 1044, 1045, 1046, 1047, 1048, 1049, 1168, 1169, 2068,
            2069, 2196, 2197, 1056, 1057, 1058, 1059, 1060, 1061, 1062, 1063, 1064, 1065,
            1154, 1155, 2084, 2085, 2120, 2121, 1072, 1073, 1074, 1075, 1076, 1077, 1078,
            1079, 1080, 1081, 1170, 1171, 2100, 2101, 2136, 2137, 1088, 1089, 1090, 1091,
            1092, 1093, 1094, 1095, 1096, 1097, 1156, 1157, 2116, 2117, 1160, 1161, 1104,
            1105, 1106, 1107, 1108, 1109, 1110, 1111, 1112, 1113, 1172, 1173, 2132, 2133,
            1176, 1177, 1120, 1121, 1122, 1123, 1124, 1125, 1126, 1127, 1128, 1129, 1158,
            1159, 2148, 2149, 2184, 2185, 1136, 1137, 1138, 1139, 1140, 1141, 1142, 1143,
            1144, 1145, 1174, 1175, 2164, 2165, 2200, 2201, 1280, 1281, 1282, 1283, 1284,
            1285, 1286, 1287, 1288, 1289, 1408, 1409, 2308, 2309, 2436, 2437, 1296, 1297,
            1298, 1299, 1300, 1301, 1302, 1303, 1304, 1305, 1424, 1425, 2324, 2325, 2452,
            2453, 1312, 1313, 1314, 1315, 1316, 1317, 1318, 1319, 1320, 1321, 1410, 1411,
            2340, 2341, 2376, 2377, 1328, 1329, 1330, 1331, 1332, 1333, 1334, 1335, 1336,
            1337, 1426, 1427, 2356, 2357, 2392, 2393, 1344, 1345, 1346, 1347, 1348, 1349,
            1350, 1351, 1352, 1353, 1412, 1413, 2372, 2373, 1416, 1417, 1360, 1361, 1362,
            1363, 1364, 1365, 1366, 1367, 1368, 1369, 1428, 1429, 2388, 2389, 1432, 1433,
            1376, 1377, 1378, 1379, 1380, 1381, 1382, 1383, 1384, 1385, 1414, 1415, 2404,
            2405, 2440, 2441, 1392, 1393, 1394, 1395, 1396, 1397, 1398, 1399, 1400, 1401,
            1430, 1431, 2420, 2421, 2456, 2457, 1536, 1537, 1538, 1539, 1540, 1541, 1542,
            1543, 1544, 1545, 1664, 1665, 2054, 2055, 2182, 2183, 1552, 1553, 1554, 1555,
            1556, 1557, 1558, 1559, 1560, 1561, 1680, 1681, 2070, 2071, 2198, 2199, 1568,
            1569, 1570, 1571, 1572, 1573, 1574, 1575, 1576, 1577, 1666, 1667, 2086, 2087,
            2152, 2153, 1584, 1585, 1586, 1587, 1588, 1589, 1590, 1591, 1592, 1593, 1682,
            1683, 2102, 2103, 2168, 2169, 1600, 1601, 1602, 1603, 1604, 1605, 1606, 1607,
            1608, 1609, 1668, 1669, 2118, 2119, 1672, 1673, 1616, 1617, 1618, 1619, 1620,
            1621, 1622, 1623, 1624, 1625, 1684, 1685, 2134, 2135, 1688, 1689, 1632, 1633,
            1634, 1635, 1636, 1637, 1638, 1639, 1640, 1641, 1670, 1671, 2150, 2151, 2184,
            2185, 1648, 1649, 1650, 1651, 1652, 1653, 1654, 1655, 1656, 1657, 1686, 1687,
            2166, 2167, 2200, 2201, 1792, 1793, 1794, 1795, 1796, 1797, 1798, 1799, 1800,
            1801, 1920, 1921, 2310, 2311, 2438, 2439, 1808, 1809, 1810, 1811, 1812, 1813,
            1814, 1815, 1816, 1817, 1936, 1937, 2326, 2327, 2454, 2455, 1824, 1825, 1826,
            1827, 1828, 1829, 1830, 1831, 1832, 1833, 1922, 1923, 2342, 2343, 2408, 2409,
            1840, 1841, 1842, 1843, 1844, 1845, 1846, 1847, 1848, 1849, 1938, 1939, 2358,
            2359, 2424, 2425, 1856, 1857, 1858, 1859, 1860, 1861, 1862, 1863, 1864, 1865,
            1924, 1925, 2374, 2375, 1928, 1929, 1872, 1873, 1874, 1875, 1876, 1877, 1878,
            1879, 1880, 1881, 1940, 1941, 2390, 2391, 1944, 1945, 1888, 1889, 1890, 1891,
            1892, 1893, 1894, 1895, 1896, 1897, 1926, 1927, 2406, 2407, 2440, 2441, 1904,
            1905, 1906, 1907, 1908, 1909, 1910, 1911, 1912, 1913, 1942, 1943, 2422, 2423,
            2456, 2457};
      return dpd2bcd;
   }

   /**
    *  LUT for powers of ten
    */
   private static final long [/*19*/]powersOfTenLL={
      1L, 10L, 100L, 1000L, /*0 to 4 */
      10000L, 100000L, 1000000L, 10000000L, /*5 to 8*/
      100000000L, 1000000000L, 10000000000L, 100000000000L, /*9 to 12*/
      1000000000000L, 10000000000000L, 100000000000000L, 1000000000000000L, /*13 to 16 */
      10000000000000000L, 100000000000000000L, 1000000000000000000L /*17 to 18 */
   };

   private final static boolean isDFPZero(long laside){
      // quick check for 0
      if (laside == dfpZERO) {
         return true;
      }
      // if DPF is ZERO, then the combo field will be XX000 where XX is I don't care
      // values, and then coefficient continuation field will be all 0s
      return ((laside & 0x0003FFFFFFFFFFFFL) == 0 && ((laside >>> 58) & 0x7) == 0);
   }

   private final void clone(BigDecimal src, BigDecimal tar) {
      int tempFlags = getflags(src);
      setflags(tar, tempFlags);
      setlaside(tar, getlaside(src));
      setscale(tar, getscale(src));
      if ((tempFlags & 0x3) == 0x2) {
         setbi(tar, getbi(src));
      }
   }

   private final int finish(BigDecimal bd, int prec, int rm){
      if (prec > 0 && bd.precision() > prec) {
         if ((getflags(bd) & 0x3) == 0x0) {
            return round(bd, prec, rm);
         } else {
            MathContext mc = new MathContext(prec, RoundingMode.valueOf(rm));
            clone(bd.round(mc), bd);
         }
      }
      return HARDWARE_OPERATION_SUCCESS;
   }

   protected final static int getflags(BigDecimal bd) {
      throw new RuntimeException("getflags not compiled\n"); //$NON-NLS-1$
   }

   protected final static long getlaside(BigDecimal bd) {
      throw new RuntimeException("getlaside not compiled\n"); //$NON-NLS-1$
   }

   protected final static int getscale(BigDecimal bd) {
      throw new RuntimeException("getscale not compiled\n"); //$NON-NLS-1$
   }

   protected final static BigInteger getbi(BigDecimal bd) {
      throw new RuntimeException("getbi not compiled\n"); //$NON-NLS-1$
   }

   protected final static void setflags(BigDecimal bd, int flags) {
      throw new RuntimeException("setflags not compiled\n"); //$NON-NLS-1$
   }

   protected final static void setlaside(BigDecimal bd, long laside) {
      throw new RuntimeException("setlaside not compiled\n"); //$NON-NLS-1$
   }

   protected final static void setscale(BigDecimal bd, int scale) {
      throw new RuntimeException("setscale not compiled\n"); //$NON-NLS-1$
   }

   protected final static void setbi(BigDecimal bd, BigInteger bi) {
      throw new RuntimeException("setbi not compiled\n"); //$NON-NLS-1$
   }

   private final static long extractDFPDigitsBCD(long dfpNum){

       int combo=0; // 5 bit combo field

       //quick check for 0
       if (dfpNum == dfpZERO)
          return 0;

       // store the combo field bits
       combo = (int)(dfpNum >>> 58); //shift out extraneous bits
       combo &= 0x1F; //and out sign bit

       // MANTISSA

       // store the mantissa continuation field bits 14 to 62 (50 bits in total)
       long ccf = dfpNum & 0x0003FFFFFFFFFFFFL;

       // Convert each set of 10 DPD bits from continuation bits to 12 BCD digits
       long ccBCD=0;
       long bcd=0;
       for (int i=0; i <5; i++){ //5 groups of 10 bits
          bcd = DPD2BCD[(int)((ccf & 0x3FF0000000000L)>>>40)];
          ccBCD <<=12;
          ccBCD|=bcd;
          ccf <<= 10; //shift out top 10 bits
       }

       //ccBCD contains 15 BCD digits, now need to prepend the first one
       ccBCD|=(((long)(doubleDFPComboField[combo]>>>2))<<60);
       return ccBCD;
   }


   private final static boolean allzeroBCD(long num, int numDigsToCheck){

      // isolate numDigsToCheck rightmost digits...
      long mask = 0xFFFFFFFFFFFFFFFFL;
      mask >>>= (64-numDigsToCheck)*4;
      return ((mask & num) == 0);
   }

   private final static int extractDFPExponent(long dfpNum){

      byte combo=0; // 5 bit combo field

      // store the combo field bits
      combo = (byte)(dfpNum >>> 58); //shift out extraneous bits
      //combo &= 0x1F; //and out sign bit - not sure why I was doing this

      // store the biased exponent field bits 6 to 13
      short bxf  = (short)(dfpNum >>> 50); //shift out extraneous bits
      bxf &= 0x00FF; //and out sign and combo field

      // parse the combo field
      byte exp2Digits = (byte)(doubleDFPComboField[combo] & 0x03);

      // unbias the exponent
      short unbExp = (short)(exp2Digits);
      unbExp <<= 8;
      unbExp|=bxf;
      return unbExp;
   }

   private static final long powerOfTenLL(long i){
      if (i > -1 && i <= 18)
         return powersOfTenLL[(int)i];
      else
         return -1;
   }

   /* Return number of digits in lon
    * The value '0' has numDigits = 1.
    */
   private final static int numDigits(long lon){
      //if (lon <0) lon*=-1; //need to make it positive for this to work
      lon = Math.abs(lon);

      //hardcode powers of ten to avoid LUT lookups

      //rhs of the tree
      if (lon < 1000000000L /*powerOfTenLL[9]*/){
         if (lon < 100000000L /*powerOfTenLL[8]*/){
            if (lon < 10000L /*powerOfTenLL[4]*/){
               if (lon < 100L /* powerOfTenLL[2]*/){
                  if (lon < 10L /*powerOfTenLL[1]*/)
                     return 1;
                  else
                     return 2;
               }
               else if (lon < 1000L /*powerOfTenLL[3]*/)
                  return 3;
               else
                  return 4;
            }
            else if (lon < 1000000L /*powerOfTenLL[6]*/){
               if ( lon < 100000L /*powerOfTenLL[5]*/)
                  return 5;
               else
                  return 6;
            }
            else if (lon < 10000000L /*powerOfTenLL[7]*/)
               return 7;
            else return 8;
         }
         else
            return 9;
      }
      //lhs of the tree
      else{
         if (lon < 10000000000L /*powerOfTenLL[10]*/)
            return 10;
         else if (lon < 100000000000000L /*powerOfTenLL[14]*/){
            if (lon < 1000000000000L /*powerOfTenLL[12]*/){
               if (lon < 100000000000L /*powerOfTenLL[11]*/)
                  return 11;
               else
                  return 12;
            }
            else if (lon < 10000000000000L /*powerOfTenLL[13]*/)
               return 13;
            else
               return 14;
         }
         else if (lon < 10000000000000000L /*powerOfTenLL[16]*/){
            if (lon < 1000000000000000L /*powerOfTenLL[15]*/)
               return 15;
            else
               return 16;
         }
         else if (lon < 100000000000000000L /*powerOfTenLL[17]*/)
            return 17;
         else if (lon < 1000000000000000000L /*powerOfTenLL[18]*/)
            return 18;
         return 19;
      }
   }

   private final static int digitAtBCD(long bcd, int numDigits, int indexFromLeft){

      int indexFromRight = numDigits-indexFromLeft-1;
      switch (indexFromRight){
      case 0:
         return (int)(bcd &  0x000000000000000FL);
      case 1:
         return (int)((bcd & 0x00000000000000F0L)>>>4);
      case 2:
         return (int)((bcd & 0x0000000000000F00L)>>>8);
      case 3:
         return (int)((bcd & 0x000000000000F000L)>>>12);
      case 4:
         return (int)((bcd & 0x00000000000F0000L)>>>16);
      case 5:
         return (int)((bcd & 0x0000000000F00000L)>>>20);
      case 6:
         return (int)((bcd & 0x000000000F000000L)>>>24);
      case 7:
         return (int)((bcd & 0x00000000F0000000L)>>>28);
      case 8:
         return (int)((bcd & 0xF00000000L)>>>32);
      case 9:
         return (int)((bcd & 0xF000000000L)>>>36);
      case 10:
         return (int)((bcd & 0xF0000000000L)>>>40);
      case 11:
         return (int)((bcd & 0xF00000000000L)>>44);
      case 12:
         return (int)((bcd & 0xF000000000000L)>>>48);
      case 13:
         return (int)((bcd & 0xF0000000000000L)>>>52);
      case 14:
         return (int)((bcd & 0xF00000000000000L)>>>56);
      case 15:
         return (int)((bcd & 0xF000000000000000L)>>>60);
      default:
         return 0;
      }
   }

   private final static int digitAt(long lon, int numdigits, int loc){

      lon = Math.abs(lon);
      if (loc > numdigits-1)return -1;
      if (loc < 0) return -1;
      int indexFromRight = numdigits-loc-1;
      if (lon <= Integer.MAX_VALUE){
         int temp=(int)lon;
         switch (indexFromRight){
         case 0:
            break;
         case 1:
            temp /=10;
            break;
         case 2:
            temp /=100;
            break;
         case 3:
            temp /=1000;
            break;
         case 4:
            temp /=10000;
            break;
         case 5:
            temp /=100000;
            break;
         case 6:
            temp /=1000000;
            break;
         case 7:
            temp /=10000000;
            break;
         case 8:
            temp /=100000000;
            break;
         case 9:
            temp /=1000000000;      //unsure whether remaining cases would
            break;               //ever be taken in the Integer case
         case 10:
            temp /=10000000000L;
            break;
         case 11:
            temp /=100000000000L;
            break;
         case 12:
            temp /=1000000000000L;
            break;
         case 13:
            temp /=10000000000000L;
            break;
         case 14:
            temp /=100000000000000L;
            break;
         case 15:
            temp /=1000000000000000L;
            break;
         case 16:
            temp /=10000000000000000L;
            break;
         case 17:
            temp /=100000000000000000L;
            break;
         case 18:
            temp /=1000000000000000000L;
            break;
         }

         // find remainder
         if (temp <= Integer.MAX_VALUE){
            int intTmp = (int)temp;
            int tmpVal = intTmp;
            intTmp = uDivideByTen(intTmp);
            return (tmpVal - ((intTmp << 3) + (intTmp << 1)));
         }
         else{
            long tmpVal = temp;
            temp/=10;
            return (int)(tmpVal - (((temp << 3) + (temp << 1))));
         }
      }
      else{
         long temp=lon;
         switch (indexFromRight){
         case 0:
            break;
         case 1:
            temp /= 10;
            break;
         case 2:
            temp /= 100;
            break;
         case 3:
            temp /= 1000;
            break;
         case 4:
            temp /= 10000;
            break;
         case 5:
            temp /= 100000;
            break;
         case 6:
            temp /= 1000000;
            break;
         case 7:
            temp /= 10000000;
            break;
         case 8:
            temp /= 100000000;
            break;
         case 9:
            temp /= 1000000000;
            break;
         case 10:
            temp /= 10000000000L;
            break;
         case 11:
            temp /= 100000000000L;
            break;
         case 12:
            temp /= 1000000000000L;
            break;
         case 13:
            temp /= 10000000000000L;
            break;
         case 14:
            temp /= 100000000000000L;
            break;
         case 15:
            temp /= 1000000000000000L;
            break;
         case 16:
            temp /= 10000000000000000L;
            break;
         case 17:
            temp /= 100000000000000000L;
            break;
         case 18:
            temp /= 1000000000000000000L;
            break;
         }

         // find remainder
         if (temp <= Integer.MAX_VALUE){
            int intTmp = (int)temp;
            int tmpVal = intTmp;
            intTmp = uDivideByTen(intTmp);
            return (tmpVal - ((intTmp << 3) + (intTmp << 1)));
         }
         else{
            long tmpVal = temp;
            temp/=10;
            return (int)(tmpVal - (((temp << 3) + (temp << 1))));
         }
      }
   }

   private final static int uDivideByTen(int x){
      int q = (x >> 1) + (x >> 2);
      q = q + (q >> 4);
      q = q + (q >> 8);
      q = q + (q >> 16);
      q >>=3;
      x -= q*10;
      return q + ((x + 6) >> 4);
   }

   private final void badDivideByZero() {
      /*[MSG "K0407", "division by zero"]*/
      throw new ArithmeticException(com.ibm.oti.util.Msg.getString("K0407")); //$NON-NLS-1$
   }
   private final void conversionOverflow(BigDecimal bd) {
      /*[MSG "K0458", "Conversion overflow: {0}"]*/
      throw new ArithmeticException(com.ibm.oti.util.Msg.getString("K0458", bd)); //$NON-NLS-1$
   }
   private final void nonZeroDecimals(BigDecimal bd) {
      /*[MSG "K0457", "Non-zero decimal digits: {0}"]*/
      throw new ArithmeticException(com.ibm.oti.util.Msg.getString("K0457", bd)); //$NON-NLS-1$
   }
   private final void scaleOutOfRange(long s) {
      /*[MSG "K0451", "BigDecimal scale outside legal range: {0}"]*/
      throw new ArithmeticException(com.ibm.oti.util.Msg.getString("K0451", Long.toString(s))); //$NON-NLS-1$
   }
   private static final void scaleOverflow() {
      /*[MSG "K0453", "Scale overflow"]*/
      throw new ArithmeticException(com.ibm.oti.util.Msg.getString("K0453")); //$NON-NLS-1$
   }
}

/*[ENDIF] #INCLUDE */
/*[REM]  do not remove following blank line*/

