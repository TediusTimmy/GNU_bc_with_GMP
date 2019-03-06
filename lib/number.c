/* number.c: Implements arbitrary-precision decimal fixed-point numbers. */
/*
    Copyright (C) 1991, 1992, 1993, 1994, 1997, 2000, 2012-2017 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to:

      The Free Software Foundation, Inc.
      51 Franklin Street, Fifth Floor
      Boston, MA 02110-1301  USA


    You may contact the author by:
       e-mail:  philnelson@acm.org
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062



    This is a modification of the original bc_num library to use GMP as
    the back-end for number handling. It was begun by Thomas DiModica
    in August of 2011, and was finally released to the world in December
    that same year.

    Thomas's Note: So, the bc FAQ tries to explain why bc originally
    didn't use GMP. The crux of the argument is that GMP doesn't have a
    fixed-point type [Unmentioned is that even if it did, GMP would
    implement a BINARY fixed-point type rather than a DECIMAL fixed-point
    type]. My answer is here: this code. Phil (Or Dr. Nelson or Prof.
    Nelson, whichever he prefers) had to write a layer over GMP
    implementing decimal fixed-point arithmetic, so why not write the
    whole shebang? Which is what he did.

*************************************************************************/

#include <stdio.h>
#include <config.h>
#include <number.h>
#include <assert.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <ctype.h>

/* Prototypes needed for external utility routines. */

#define bc_rt_warn rt_warn
#define bc_rt_error rt_error

void rt_warn (const char *mesg ,...);
void rt_error (const char *mesg ,...);

void *bc_num_malloc (size_t);
void *bc_num_realloc (void *, size_t, size_t);

/* Storage used for special numbers. */
bc_num _zero_;
bc_num _one_;
bc_num _two_;

static bc_num _bc_Free_list = NULL;

/* new_num allocates a number and sets fields to known values. */

bc_num
bc_new_num (int length, int scale)
{
  bc_num temp;

  if (_bc_Free_list != NULL) {
    temp = _bc_Free_list;
    _bc_Free_list = temp->n_next;
  } else {
    temp = (bc_num) bc_num_malloc (sizeof(bc_struct));
  }
  length = 0; /* To silence the compiler without changing the API. */
  temp->n_scale = scale;
  temp->n_refs = 1;
  mpz_init (temp->n_value);
  return temp;
}

/* "Frees" a bc_num NUM.  Actually decreases reference count and only
   frees the storage if reference count is zero. */

void
bc_free_num (bc_num *num)
{
  if (*num == NULL) return;
  (*num)->n_refs--;
  if ((*num)->n_refs == 0) {
    mpz_clear ((*num)->n_value);
    (*num)->n_next = _bc_Free_list;
    _bc_Free_list = *num;
  }
  *num = NULL;
}


/* Intitialize the number package! */

void
bc_init_numbers (void)
{
  /* Initialize gmp to use our routines. */
  mp_set_memory_functions(&bc_num_malloc, &bc_num_realloc, NULL);

  _zero_ = bc_new_num (1,0);
  _one_  = bc_new_num (1,0);
  mpz_set_si (_one_->n_value, 1);
  _two_  = bc_new_num (1,0);
  mpz_set_si (_two_->n_value, 2);
}


/* Make a copy of a number!  Just increments the reference count! */

bc_num
bc_copy_num (bc_num num)
{
  num->n_refs++;
  return num;
}


/* Initialize a number NUM by making it a copy of zero. */

void
bc_init_num (bc_num *num)
{
  *num = bc_copy_num (_zero_);
}



/* Compare two bc numbers.  Return value is 0 if equal, -1 if N1 is less
   than N2 and +1 if N1 is greater than N2.  If USE_SIGN is false, just
   compare the magnitudes. 

   TD: ignore_last was unused, so it was removed. */

static int
_bc_do_compare ( bc_num n1, bc_num n2, int use_sign )
{
  int result;
  mpz_t copy, step;

  if (n1->n_scale > n2->n_scale)
    { /* Step-up n2 and compare */
      mpz_init_set (copy, n2->n_value);
      mpz_init_set_si (step, 10);
      mpz_pow_ui (step, step, n1->n_scale - n2->n_scale);
      mpz_mul (copy, copy, step);

      if (use_sign)
	result = mpz_cmp (n1->n_value, copy);
      else
	result = mpz_cmpabs (n1->n_value, copy);

      mpz_clear (copy);
      mpz_clear (step);
    }
  else if (n1->n_scale < n2->n_scale)
    { /* Step-up n1 and compare */
      mpz_init_set (copy, n1->n_value);
      mpz_init_set_si (step, 10);
      mpz_pow_ui (step, step, n2->n_scale - n1->n_scale);
      mpz_mul (copy, copy, step);

      if (use_sign)
	result = mpz_cmp (copy, n2->n_value);
      else
	result = mpz_cmpabs (copy, n2->n_value);

      mpz_clear (copy);
      mpz_clear (step);
    }
  else /* n1->n_scale == n2->n_scale */
    { /* Just compare */
      if (use_sign)
	result = mpz_cmp (n1->n_value, n2->n_value);
      else
	result = mpz_cmpabs (n1->n_value, n2->n_value);
    }

  /* Fix the value of result, just in case.... */
  if (result < 0)
    result = -1;
  if (result > 0)
    result = 1;

  return result;
}


/* This is the "user callable" routine to compare numbers N1 and N2. */

int
bc_compare ( bc_num n1, bc_num n2 )
{
  return _bc_do_compare (n1, n2, TRUE);
}

/* In some places we need to check if the number is negative. */

char
bc_is_neg (bc_num num)
{
  return mpz_sgn (num->n_value) == -1;
}

/* In some places we need to check if the number NUM is zero. */

char
bc_is_zero (bc_num num)
{
  return mpz_sgn (num->n_value) == 0;
}

/* N2 is subtracted from N1 and the result placed in RESULT.  SCALE_MIN
   is the minimum scale for the result. */

void
bc_sub (bc_num n1, bc_num n2, bc_num *result, int scale_min)
{
  bc_num diff = NULL;
  int diff_scale;
  mpz_t copy, step;

  diff_scale = MAX (n1->n_scale, n2->n_scale);
  diff = bc_new_num (1, MAX (diff_scale, scale_min));

  if (n1->n_scale > n2->n_scale)
    { /* Step-up n2 and subtract */
      mpz_init_set (copy, n2->n_value);
      mpz_init_set_si (step, 10);
      mpz_pow_ui (step, step, n1->n_scale - n2->n_scale);
      mpz_mul (copy, copy, step);

      mpz_sub (diff->n_value, n1->n_value, copy);

      mpz_clear (copy);
      mpz_clear (step);
    }
  else if (n1->n_scale < n2->n_scale)
    { /* Step-up n1 and subtract */
      mpz_init_set (copy, n1->n_value);
      mpz_init_set_si (step, 10);
      mpz_pow_ui (step, step, n2->n_scale - n1->n_scale);
      mpz_mul (copy, copy, step);

      mpz_sub (diff->n_value, copy, n2->n_value);

      mpz_clear (copy);
      mpz_clear (step);
    }
  else /* n1->n_scale == n2->n_scale */
    { /* Just subtract */
      mpz_sub (diff->n_value, n1->n_value, n2->n_value);
    }

  if (diff_scale < scale_min)
    {
      /* Step-up the result */
      mpz_init_set_si (step, 10);
      mpz_pow_ui (step, step, scale_min - diff_scale);
      mpz_mul (diff->n_value, diff->n_value, step);
      mpz_clear (step);
    }

  /* Clean up and return. */
  bc_free_num (result);
  *result = diff;
}


/* N1 is added to N2 and the result placed into RESULT.  SCALE_MIN
   is the minimum scale for the result. */

void
bc_add (bc_num n1, bc_num n2, bc_num *result, int scale_min)
{
  bc_num sum = NULL;
  int sum_scale;
  mpz_t copy, step;

  sum_scale = MAX (n1->n_scale, n2->n_scale);
  sum = bc_new_num (1, MAX (sum_scale, scale_min));

  if (n1->n_scale > n2->n_scale)
    { /* Step-up n2 and add */
      mpz_init_set (copy, n2->n_value);
      mpz_init_set_si (step, 10);
      mpz_pow_ui (step, step, n1->n_scale - n2->n_scale);
      mpz_mul (copy, copy, step);

      mpz_add (sum->n_value, n1->n_value, copy);

      mpz_clear (copy);
      mpz_clear (step);
    }
  else if (n1->n_scale < n2->n_scale)
    { /* Step-up n1 and add */
      mpz_init_set (copy, n1->n_value);
      mpz_init_set_si (step, 10);
      mpz_pow_ui (step, step, n2->n_scale - n1->n_scale);
      mpz_mul (copy, copy, step);

      mpz_add (sum->n_value, copy, n2->n_value);

      mpz_clear (copy);
      mpz_clear (step);
    }
  else /* n1->n_scale == n2->n_scale */
    { /* Just add */
      mpz_add (sum->n_value, n1->n_value, n2->n_value);
    }

  if (sum_scale < scale_min)
    {
      /* Step-up the result */
      mpz_init_set_si (step, 10);
      mpz_pow_ui (step, step, scale_min - sum_scale);
      mpz_mul (sum->n_value, sum->n_value, step);
      mpz_clear (step);
    }

  /* Clean up and return. */
  bc_free_num (result);
  *result = sum;
}


/* The multiply routine.  N2 times N1 is put int PROD with the scale of
   the result being MIN(N2 scale+N1 scale, MAX (SCALE, N2 scale, N1 scale)).

   TD: Thankfully, how this was coded gave an effective rounding mode of
   truncate (or so I believe), making this easier than it could have been. */

void
bc_multiply (bc_num n1, bc_num n2, bc_num *prod, int scale)
{
  bc_num pval = NULL;
  int full_scale, prod_scale;
  mpz_t step;

  /* Initialize things. */
  full_scale = n1->n_scale + n2->n_scale;
  prod_scale = MIN(full_scale,MAX(scale,MAX(n1->n_scale,n2->n_scale)));
  pval = bc_new_num (1, prod_scale);

  /* Do the multiply */
  mpz_mul (pval->n_value, n1->n_value, n2->n_value);

  /* Clean up the number. */
  if (full_scale > prod_scale)
   { /* Step-down the result */
      mpz_init_set_si (step, 10);
      mpz_pow_ui (step, step, full_scale - prod_scale);
      mpz_tdiv_q (pval->n_value, pval->n_value, step);
      mpz_clear (step);
   }

  bc_free_num (prod);
  *prod = pval;
}


/* The full division routine. This computes N1 / N2.  It returns
   0 if the division is ok and the result is in QUOT.  The number of
   digits after the decimal point is SCALE. It returns -1 if division
   by zero is tried. */

int
bc_divide (bc_num n1, bc_num n2, bc_num *quot,  int scale)
{
  bc_num qval;
  mpz_t step;
  int step_amt;

  /* Test for divide by zero. */
  if (bc_is_zero (n2)) return -1;

  /* Do the divide. */
  qval = bc_new_num (1, scale);

  /* Step the dividend to have scale after the divide. */
  step_amt = n2->n_scale + scale - n1->n_scale;

  if (step_amt != 0)
    {
      mpz_init_set_si (step, 10);

      if (step_amt > 0)
	{
	  mpz_pow_ui (step, step, step_amt);
	  mpz_mul (qval->n_value, n1->n_value, step);
	}
      else
	{
	  mpz_pow_ui (step, step, -step_amt);
	  mpz_tdiv_q (qval->n_value, n1->n_value, step);
	}

      mpz_tdiv_q (qval->n_value, qval->n_value, n2->n_value);

      mpz_clear (step);
    }
  else
    mpz_tdiv_q (qval->n_value, n1->n_value, n2->n_value);

  bc_free_num (quot);
  *quot = qval;

  return 0;	/* Everything is OK. */
}


/* Division *and* modulo for numbers.  This computes both NUM1 / NUM2 and
   NUM1 % NUM2  and puts the results in QUOT and REM, except that if QUOT
   is NULL then that store will be omitted.
 */

int
bc_divmod (bc_num num1, bc_num num2, bc_num *quot, bc_num *rem, int scale)
{
  bc_num quotient = NULL;
  bc_num temp;
  int rscale;

  /* Check for correct numbers. */
  if (bc_is_zero (num2)) return -1;

  /* Calculate final scale. */
  rscale = MAX (num1->n_scale, num2->n_scale+scale);
  bc_init_num(&temp);

  /* Calculate it. */
  bc_divide (num1, num2, &temp, scale);
  if (quot)
    quotient = bc_copy_num (temp);
  bc_multiply (temp, num2, &temp, rscale);
  bc_sub (num1, temp, rem, rscale);
  bc_free_num (&temp);

  if (quot)
    {
      bc_free_num (quot);
      *quot = quotient;
    }

  return 0;	/* Everything is OK. */
}


/* Modulo for numbers.  This computes NUM1 % NUM2  and puts the
   result in RESULT.   */

int
bc_modulo ( bc_num num1, bc_num num2, bc_num *result, int scale)
{
  return bc_divmod (num1, num2, NULL, result, scale);
}

/* Raise BASE to the EXPO power, reduced modulo MOD.  The result is
   placed in RESULT.  If a EXPO is not an integer,
   only the integer part is used.  */

int
bc_raisemod (bc_num base, bc_num expo, bc_num mod, bc_num *result, int scale)
{
  bc_num power, exponent, parity, temp;
  int rscale;

  /* Check for correct numbers. */
  if (bc_is_zero(mod)) return -1;
  if (bc_is_neg(expo)) return -1;

  /* Set initial values.  */
  power = bc_copy_num (base);
  exponent = bc_copy_num (expo);
  temp = bc_copy_num (_one_);
  bc_init_num(&parity);

  /* Check the base for scale digits. */
  if (base->n_scale != 0)
      bc_rt_warn ("non-zero scale in base");

  /* Check the exponent for scale digits. */
  if (exponent->n_scale != 0)
    {
      bc_rt_warn ("non-zero scale in exponent");
      bc_divide (exponent, _one_, &exponent, 0); /*truncate */
    }

  /* Check the modulus for scale digits. */
  if (mod->n_scale != 0)
      bc_rt_warn ("non-zero scale in modulus");

  /* Do the calculation. */
  rscale = MAX(scale, base->n_scale);
  while ( !bc_is_zero(exponent) )
    {
      (void) bc_divmod (exponent, _two_, &exponent, &parity, 0);
      if ( !bc_is_zero(parity) )
	{
	  bc_multiply (temp, power, &temp, rscale);
	  (void) bc_modulo (temp, mod, &temp, scale);
	}

      bc_multiply (power, power, &power, rscale);
      (void) bc_modulo (power, mod, &power, scale);
    }

  /* Assign the value. */
  bc_free_num (&power);
  bc_free_num (&exponent);
  bc_free_num (result);
  *result = temp;
  return 0;	/* Everything is OK. */
}

/* Raise NUM1 to the NUM2 power.  The result is placed in RESULT.
   Maximum exponent is LONG_MAX.  If a NUM2 is not an integer,
   only the integer part is used.  */

void
bc_raise (bc_num num1, bc_num num2, bc_num *result, int scale)
{
  bc_num temp;
  long exponent;
  int rscale;
  int diffscale;
  char neg;
  mpz_t step;

  /* Check the exponent for scale digits and convert to a long. */
  if (num2->n_scale != 0)
    bc_rt_warn ("non-zero scale in exponent");
  exponent = bc_num2long (num2);
  if (exponent == 0 && (_bc_do_compare (num2, _one_, FALSE) > 0))
    bc_rt_error ("exponent too large in raise");

  /* Special case if exponent is a zero. */
  if (exponent == 0)
    {
      bc_free_num (result);
      *result = bc_copy_num (_one_);
      return;
    }

  /* Other initializations. */
  if (exponent < 0)
    {
      neg = TRUE;
      exponent = -exponent;
      rscale = scale;
    }
  else
    {
      neg = FALSE;
      rscale = MIN (num1->n_scale*exponent, MAX(scale, num1->n_scale));
    }

  temp = bc_new_num(1, rscale);

  diffscale = num1->n_scale*exponent - rscale;

  /* Compute the power.  */
  mpz_pow_ui(temp->n_value, num1->n_value, exponent);

  /* Step it correctly. */
  if (diffscale != 0)
    {
      mpz_init_set_si (step, 10);

      if (diffscale < 0)
	{
	  mpz_pow_ui (step, step, -diffscale);
	  mpz_mul (temp->n_value, temp->n_value, step);
	}
      else
	{
	  mpz_pow_ui (step, step, diffscale);
	  mpz_tdiv_q (temp->n_value, temp->n_value, step);
	}

      mpz_clear (step);
    }

  /* Assign the value. */
  if (neg)
    bc_divide (_one_, temp, result, rscale);
  else
    {
      bc_free_num(result);
      *result = temp;
    }
}

/* Take the square root NUM and return it in NUM with the MAX of NUM's scale
   and SCALE digits after the decimal place. */

int
bc_sqrt (bc_num *num, int scale)
{
  int step_amt, rscale, cmp_res;
  bc_num result = NULL;
  mpz_t step;

  /* Initial checks. */
  cmp_res = bc_compare (*num, _zero_);
  if (cmp_res < 0)
    return 0;		/* error */
  else
    {
      if (cmp_res == 0)
	{
	  bc_free_num (num);
	  *num = bc_copy_num (_zero_);
	  return 1;
	}
    }
  cmp_res = bc_compare (*num, _one_);
  if (cmp_res == 0)
    {
      bc_free_num (num);
      *num = bc_copy_num (_one_);
      return 1;
    }

  /* Initialize the variables. */
  rscale = MAX (scale, (*num)->n_scale);
  step_amt = (*num)->n_scale + 2 * (rscale - (*num)->n_scale);
  result = bc_new_num (1, rscale);

  if (step_amt != 0)
    {
      mpz_init_set_si (step, 10);

      if (step_amt > 0)
	{
	  mpz_pow_ui (step, step, step_amt);
	  mpz_mul (result->n_value, (*num)->n_value, step);
	}
      else
	{
	  mpz_pow_ui (step, step, -step_amt);
	  mpz_tdiv_q (result->n_value, (*num)->n_value, step);
	}

      mpz_clear (step);

      mpz_sqrt (result->n_value, result->n_value);
    }
  else
    mpz_sqrt (result->n_value, (*num)->n_value);

  bc_free_num (num);
  *num = result;
  return 1;
}


/* The following routines provide output for bcd numbers package
   using the rules of POSIX bc for output. */

/* This structure is used for saving digits in the conversion process. */
typedef struct stk_rec {
	long  digit;
	struct stk_rec *next;
} stk_rec;

/* The reference string for digits. */
static char ref_str[] = "0123456789ABCDEF";


/* A special output routine for "multi-character digits."  Exactly
   SIZE characters must be output for the value VAL.  If SPACE is
   non-zero, we must output one space before the number.  OUT_CHAR
   is the actual routine for writing the characters. */

void
bc_out_long (long val, int size, int space, void (*out_char)(int))
{
  char digits[40];
  int len, ix;

  if (space) (*out_char) (' ');
  snprintf (digits, sizeof(digits), "%ld", val);
  len = strlen (digits);
  while (size > len)
    {
      (*out_char) ('0');
      size--;
    }
  for (ix=0; ix < len; ix++)
    (*out_char) (digits[ix]);
}

/* Output of a bcd number.  NUM is written in base O_BASE using OUT_CHAR
   as the routine to do the actual output of the characters. */

void
bc_out_num (bc_num num, int o_base, void (*out_char)(int), int leading_zero)
{
  char *nptr, *iptr;
  int  ix, fdigit, pre_space, t_len;
  stk_rec *digits, *temp;
  bc_num int_part, frac_part, base, cur_dig, t_num, max_o_digit;

  /* The negative sign if needed. */
  if (bc_is_neg(num)) (*out_char) ('-');

  /* Output the number. */
  if (bc_is_zero (num))
    (*out_char) ('0');
  else
    if (o_base == 10)
      {
	/* The number is in base 10, do it the fast way. */
	nptr = bc_num2str(num);
	iptr = nptr;
	if (*iptr == '-') iptr++;
	while (*iptr) (*out_char) (*iptr++);

	free (nptr);
      }
    else
      {
	/* special case ... */
	if (leading_zero && bc_is_zero (num))
	  (*out_char) ('0');

	/* The number is some other base. */
	digits = NULL;
	bc_init_num (&int_part);
	bc_divide (num, _one_, &int_part, 0);
	bc_init_num (&frac_part);
	bc_init_num (&cur_dig);
	bc_init_num (&base);
	bc_sub (num, int_part, &frac_part, 0);
	/* Make the INT_PART and FRAC_PART positive. */
	mpz_abs (int_part->n_value, int_part->n_value);
	mpz_abs (frac_part->n_value, frac_part->n_value);
	bc_int2num (&base, o_base);
	bc_init_num (&max_o_digit);
	bc_int2num (&max_o_digit, o_base-1);

	/* Number of digits in max_o_digit. */
	nptr = bc_num2str(max_o_digit);
	iptr = nptr;
	ix = 0;
	while (*iptr) iptr++, ix++;

	free (nptr);

	/* Get the digits of the integer part and push them on a stack. */
	while (!bc_is_zero (int_part))
	  {
	    bc_modulo (int_part, base, &cur_dig, 0);
	    temp = (stk_rec *) bc_num_malloc (sizeof(stk_rec));
	    temp->digit = bc_num2long (cur_dig);
	    temp->next = digits;
	    digits = temp;
	    bc_divide (int_part, base, &int_part, 0);
	  }

	/* Print the digits on the stack. */
	if (digits != NULL)
	  {
	    /* Output the digits. */
	    while (digits != NULL)
	      {
		temp = digits;
		digits = digits->next;
		if (o_base <= 16)
		  (*out_char) (ref_str[ (int) temp->digit]);
		else
		  bc_out_long (temp->digit, ix, 1, out_char);
		free (temp);
	      }
	  }

	/* Get and print the digits of the fraction part. */
	if (num->n_scale > 0)
	  {
	    (*out_char) ('.');
	    pre_space = 0;
	    t_num = bc_copy_num (_one_);
	    t_len = bc_num_length (t_num);
	    while (t_len <= num->n_scale)
	      {
		bc_multiply (frac_part, base, &frac_part, num->n_scale);
		fdigit = bc_num2long (frac_part);
		bc_int2num (&int_part, fdigit);
		bc_sub (frac_part, int_part, &frac_part, 0);
		if (o_base <= 16)
		  (*out_char) (ref_str[fdigit]);
		else
		  {
		    bc_out_long (fdigit, ix, pre_space, out_char);
		    pre_space = 1;
		  }
		bc_multiply (t_num, base, &t_num, 0);
		t_len = bc_num_length (t_num);
	      }
	    bc_free_num (&t_num);
	  }

	/* Clean up. */
	bc_free_num (&int_part);
	bc_free_num (&frac_part);
	bc_free_num (&base);
	bc_free_num (&cur_dig);
	bc_free_num (&max_o_digit);
      }
}
/* Convert a number NUM to a long.  The function returns only the integer
   part of the number.  For numbers that are too large to represent as
   a long, this function returns a zero.  This can be detected by checking
   the NUM for zero after having a zero returned. */

long
bc_num2long (bc_num num)
{
  long val;
  mpz_t copy, step;

  if (num->n_scale > 0)
    {
      mpz_init_set (copy, num->n_value);
      mpz_init_set_si (step, 10);
      mpz_pow_ui (step, step, num->n_scale);

      mpz_tdiv_q (copy, copy, step);

      mpz_clear (step);

      /* Test if it fits. */
      if (!mpz_fits_slong_p (copy))
	{
          mpz_clear (copy);
	  return 0;
	}

      /* Extract the int value. */
      val = mpz_get_si (copy);
      mpz_clear (copy);
    }
  else
    {
      /* Test if it fits. */
      if (!mpz_fits_slong_p (num->n_value))
	return 0;

      /* Extract the int value. */
      val = mpz_get_si (num->n_value);
    }

  if (val == (-LONG_MAX - 1))
    return 0; /* This would kill bc_raise */
  return val;
}


/* Convert an integer VAL to a bc number NUM. */

void
bc_int2num (bc_num *num, int val)
{
  /* Make the number. */
  bc_free_num (num);
  *num = bc_new_num (1, 0);
  mpz_set_si ((*num)->n_value, val);
}

/* Convert a number to a string.  Base 10 only.*/

char
*bc_num2str (bc_num num)
{
  char *str, *sptr;
  char *nptr, *iptr;
  int  i, signch, s_len, n_len;

  /* Allocate the string memory. */
  signch = ( bc_is_neg(num) ? 1 : 0 );  /* Number of sign chars. */
  s_len = mpz_sizeinbase (num->n_value, 10); /* Number of digits. */
  if (num->n_scale > 0)
    str = (char *) malloc (s_len + num->n_scale + 2 + signch);
  else
    str = (char *) malloc (s_len + 1 + signch);

  /* Convert the string to decimal. */
  n_len = gmp_asprintf(&nptr, "%Zd", num->n_value);

  /* The negative sign if needed. */
  sptr = str;
  iptr = nptr;
  if (signch)
    {
      *sptr++ = '-';
      iptr++;
      n_len--;
    }

  if (n_len >= num->n_scale)
    {
      /* Load the whole number. */
      for (i = n_len - num->n_scale; i > 0; i--)
	*sptr++ = *iptr++;

      /* Now the fraction. */
      if (num->n_scale > 0)
	{
	  *sptr++ = '.';
	  for (i=0; i<num->n_scale; i++)
	    *sptr++ = *iptr++;
	}
    }
  else
    {
      /* The number is less than 1. */
      *sptr++ = '.';
      for (i = 0; i < num->n_scale - n_len; i++)
	*sptr++ = '0';
      for (i=0; i<n_len; i++)
	*sptr++ = *iptr++;
    }

  /* Free memory. */
  free (nptr);

  /* Terminate the string and return it! */
  *sptr = '\0';
  return (str);
}
/* Convert a string to a bc number.  Base 10 only.*/

void
bc_str2num (bc_num *num, char *str, int scale)
{
  int digits, strscale;
  char *ptr, *nptr, *iptr;

  /* Prepare num. */
  bc_free_num (num);

  /* Check for valid number and count digits. */
  ptr = str;
  digits = 0;
  strscale = 0;
  if ( (*ptr == '+') || (*ptr == '-'))  ptr++;  /* Sign */
  while (*ptr == '0') ptr++;			/* Skip leading zeros. */
  while (isdigit((int)*ptr)) ptr++, digits++;	/* digits */
  if (*ptr == '.') ptr++;			/* decimal point */
  while (isdigit((int)*ptr)) ptr++, strscale++;	/* digits */
  if ((*ptr != '\0') || (digits+strscale == 0))
    {
      *num = bc_copy_num (_zero_);
      return;
    }

  /* Adjust numbers. */
  strscale = MIN(strscale, scale);

  /* Allocate storage. */
  *num = bc_new_num (1, strscale);

  nptr = (char *) bc_num_malloc (digits + strscale + 2);

  /* Copy relevant portion of string. */
  ptr = str;
  iptr = nptr;
  if (*ptr == '+') ptr++;		       /* Skip leading '+' */
  if (*ptr == '-') *iptr++ = *ptr++;	       /* Sign */
  while (*ptr == '0') ptr++;		       /* Skip leading zeros. */
  while (isdigit((int)*ptr)) *iptr++ = *ptr++; /* Digits */
  if (*ptr == '.') ptr++;		       /* Skip decimal point */
  while (isdigit((int)*ptr) && (strscale > 0))
    *(iptr++) = *(ptr++), strscale--;	       /* Digits */
  *iptr = '\0';				       /* NUL terminator */

  /* Read in. */
  gmp_sscanf(nptr, "%Zd", (*num)->n_value);

  free (nptr);
}

/* Give the number of significant digits in a bc_num. */

int
bc_num_length (bc_num num)
{
  char *nptr;
  int n_len;

  n_len = gmp_asprintf(&nptr, "%Zd", num->n_value);

  if (*nptr == '-') n_len--;

  free (nptr);

  return n_len;
}

/* Give the scale of a bc_num.
   Yes, we can just use num->n_scale, but that violates
   the bc_num abstraction and doesn't hide data. */

int
bc_num_scale (bc_num num)
{
  return num->n_scale;
}

/* Negate a bc_num. */

void
bc_neg (bc_num *num)
{
  bc_num result = NULL;

  /* A valid optimization iff there is one reference to a number. */
  if ((*num)->n_refs == 1)
    {
      mpz_neg ((*num)->n_value, (*num)->n_value);
    }
  else
    {
      result = bc_new_num (1, (*num)->n_scale);

      mpz_neg (result->n_value, (*num)->n_value);

      bc_free_num (num);
      *num = result;
    }
}


/* Debugging routines, are probably all broken now. */

#ifdef DEBUG

/* pn prints the number NUM in base 10. */

static void
out_char (int c)
{
  putchar(c);
}


void
pn (bc_num num)
{
  bc_out_num (num, 10, out_char, 0);
  out_char ('\n');
}


/* pv prints a character array as if it was a string of bcd digits. */
void
pv (char *name, unsigned char *num, int len)
{
  int i;
  printf ("%s=", name);
  for (i=0; i<len; i++) printf ("%c",BCD_CHAR(num[i]));
  printf ("\n");
}

#endif
