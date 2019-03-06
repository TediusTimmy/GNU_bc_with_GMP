/* number.h: Arbitrary precision numbers header file. */
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
       
*************************************************************************/

#ifndef _NUMBER_H_
#define _NUMBER_H_

#include <gmp.h>

typedef struct bc_struct *bc_num;

typedef struct bc_struct
    {
      int    n_scale;	/* The number of digits after the decimal point. */
      int    n_refs;    /* The number of pointers to this number. */
      bc_num n_next;	/* Linked list for available list. */

      mpz_t  n_value;	/* The number. */
    } bc_struct;


#ifdef MIN
#undef MIN
#undef MAX
#endif
#define MAX(a,b)      ((a)>(b)?(a):(b))
#define MIN(a,b)      ((a)>(b)?(b):(a))

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef LONG_MAX
#define LONG_MAX 0x7fffffff
#endif


/* Global numbers. */
extern bc_num _zero_;
extern bc_num _one_;
extern bc_num _two_;


/* Function Prototypes */

void bc_init_numbers (void);

bc_num bc_new_num (int length, int scale);

void bc_free_num (bc_num *num);

bc_num bc_copy_num (bc_num num);

void bc_init_num (bc_num *num);

void bc_str2num (bc_num *num, char *str, int scale);

char *bc_num2str (bc_num num);

void bc_int2num (bc_num *num, int val);

long bc_num2long (bc_num num);

int bc_num_length (bc_num num);
int bc_num_scale (bc_num num);

void bc_neg (bc_num *num);

int bc_compare (bc_num n1, bc_num n2);

char bc_is_zero (bc_num num);

char bc_is_neg (bc_num num);

void bc_add (bc_num n1, bc_num n2, bc_num *result, int scale_min);

void bc_sub (bc_num n1, bc_num n2, bc_num *result, int scale_min);

void bc_multiply (bc_num n1, bc_num n2, bc_num *prod, int scale);

int bc_divide (bc_num n1, bc_num n2, bc_num *quot, int scale);

int bc_modulo (bc_num num1, bc_num num2, bc_num *result, int scale);

int bc_divmod (bc_num num1, bc_num num2, bc_num *quot,
			   bc_num *rem, int scale);

int bc_raisemod (bc_num base, bc_num expo, bc_num mod,
			     bc_num *result, int scale);

void bc_raise (bc_num num1, bc_num num2, bc_num *result,
			   int scale);

int bc_sqrt (bc_num *num, int scale);

void bc_out_num (bc_num num, int o_base, void (* out_char)(int),
			     int leading_zero);

void bc_out_long (long val, int size, int space, void (*out_char)(int));
#endif
