/*
 * Bytecode utility functions
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#define YASM_LIB_INTERNAL
#include "util.h"
/*@unused@*/ RCSID("$Id$");

#include "coretype.h"

#include "errwarn.h"
#include "intnum.h"
#include "expr.h"
#include "value.h"
#include "symrec.h"

#include "bytecode.h"

#include "bc-int.h"
#include "expr-int.h"


void
yasm_bc_set_multiple(yasm_bytecode *bc, yasm_expr *e)
{
    if (bc->multiple)
	bc->multiple = yasm_expr_create_tree(bc->multiple, YASM_EXPR_MUL, e,
					     e->line);
    else
	bc->multiple = e;
}

void
yasm_bc_finalize_common(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
}

void
yasm_bc_transform(yasm_bytecode *bc, const yasm_bytecode_callback *callback,
		  void *contents)
{
    if (bc->callback)
	bc->callback->destroy(bc->contents);
    bc->callback = callback;
    bc->contents = contents;
}

yasm_bytecode *
yasm_bc_create_common(const yasm_bytecode_callback *callback, void *contents,
		      unsigned long line)
{
    yasm_bytecode *bc = yasm_xmalloc(sizeof(yasm_bytecode));

    bc->callback = callback;

    bc->section = NULL;

    bc->multiple = (yasm_expr *)NULL;
    bc->len = 0;

    bc->line = line;

    bc->offset = 0;

    bc->opt_flags = 0;

    bc->symrecs = NULL;

    bc->contents = contents;

    return bc;
}

yasm_section *
yasm_bc_get_section(yasm_bytecode *bc)
{
    return bc->section;
}

void
yasm_bc__add_symrec(yasm_bytecode *bc, yasm_symrec *sym)
{
    if (!bc->symrecs) {
	bc->symrecs = yasm_xmalloc(2*sizeof(yasm_symrec *));
	bc->symrecs[0] = sym;
	bc->symrecs[1] = NULL;
    } else {
	/* Very inefficient implementation for large numbers of symbols.  But
	 * that would be very unusual, so use the simple algorithm instead.
	 */
	size_t count = 1;
	while (bc->symrecs[count])
	    count++;
	bc->symrecs = yasm_xrealloc(bc->symrecs,
				    (count+2)*sizeof(yasm_symrec *));
	bc->symrecs[count] = sym;
	bc->symrecs[count+1] = NULL;
    }
}

void
yasm_bc_destroy(yasm_bytecode *bc)
{
    if (!bc)
	return;

    if (bc->callback)
	bc->callback->destroy(bc->contents);
    yasm_expr_destroy(bc->multiple);
    if (bc->symrecs)
	yasm_xfree(bc->symrecs);
    yasm_xfree(bc);
}

void
yasm_bc_print(const yasm_bytecode *bc, FILE *f, int indent_level)
{
    if (!bc->callback)
	fprintf(f, "%*s_Empty_\n", indent_level, "");
    else
	bc->callback->print(bc->contents, f, indent_level);
    fprintf(f, "%*sMultiple=", indent_level, "");
    if (!bc->multiple)
	fprintf(f, "nil (1)");
    else
	yasm_expr_print(bc->multiple, f);
    fprintf(f, "\n%*sLength=%lu\n", indent_level, "", bc->len);
    fprintf(f, "%*sLine Index=%lu\n", indent_level, "", bc->line);
    fprintf(f, "%*sOffset=%lx\n", indent_level, "", bc->offset);
}

void
yasm_bc_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
    if (bc->callback)
	bc->callback->finalize(bc, prev_bc);
    if (bc->multiple) {
	yasm_value val;

	if (yasm_value_finalize_expr(&val, bc->multiple, 0))
	    yasm_error_set(YASM_ERROR_TOO_COMPLEX,
			   N_("multiple expression too complex"));
	else if (val.rel)
	    yasm_error_set(YASM_ERROR_NOT_ABSOLUTE,
			   N_("multiple expression not absolute"));
	bc->multiple = val.abs;
    }
}

/*@null@*/ yasm_intnum *
yasm_common_calc_bc_dist(yasm_bytecode *precbc1, yasm_bytecode *precbc2)
{
    unsigned long dist;
    yasm_intnum *intn;

    if (precbc1->section != precbc2->section)
	return NULL;

    dist = precbc2->offset + precbc2->len;
    if (dist < precbc1->offset + precbc1->len) {
	intn = yasm_intnum_create_uint(precbc1->offset + precbc1->len - dist);
	yasm_intnum_calc(intn, YASM_EXPR_NEG, NULL);
	return intn;
    }
    dist -= precbc1->offset + precbc1->len;
    return yasm_intnum_create_uint(dist);
}

yasm_bc_resolve_flags
yasm_bc_resolve(yasm_bytecode *bc, int save,
		yasm_calc_bc_dist_func calc_bc_dist)
{
    yasm_bc_resolve_flags retval = YASM_BC_RESOLVE_MIN_LEN;
    /*@null@*/ yasm_expr *temp;
    yasm_expr **tempp;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;

    bc->len = 0;	/* start at 0 */

    if (!bc->callback)
	yasm_internal_error(N_("got empty bytecode in bc_resolve"));
    else
	retval = bc->callback->resolve(bc, save, calc_bc_dist);

    /* Multiply len by number of multiples */
    if (bc->multiple) {
	if (save) {
	    temp = NULL;
	    tempp = &bc->multiple;
	} else {
	    temp = yasm_expr_copy(bc->multiple);
	    assert(temp != NULL);
	    tempp = &temp;
	}
	num = yasm_expr_get_intnum(tempp, calc_bc_dist);
	if (!num) {
	    retval = YASM_BC_RESOLVE_UNKNOWN_LEN;
	    if (temp && yasm_expr__contains(temp, YASM_EXPR_FLOAT)) {
		yasm_error_set(YASM_ERROR_VALUE,
		    N_("expression must not contain floating point value"));
		retval |= YASM_BC_RESOLVE_ERROR;
	    }
	} else {
	    if (yasm_intnum_sign(num) >= 0)
		bc->len *= yasm_intnum_get_uint(num);
	    else
		retval |= YASM_BC_RESOLVE_ERROR;
	}
	yasm_expr_destroy(temp);
    }

    /* If we got an error somewhere along the line, clear out any calc len */
    if (retval & YASM_BC_RESOLVE_UNKNOWN_LEN)
	bc->len = 0;

    return retval;
}

/*@null@*/ /*@only@*/ unsigned char *
yasm_bc_tobytes(yasm_bytecode *bc, unsigned char *buf, unsigned long *bufsize,
		/*@out@*/ unsigned long *multiple, /*@out@*/ int *gap,
		void *d, yasm_output_value_func output_value,
		/*@null@*/ yasm_output_reloc_func output_reloc)
    /*@sets *buf@*/
{
    /*@only@*/ /*@null@*/ unsigned char *mybuf = NULL;
    unsigned char *origbuf, *destbuf;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;
    unsigned long datasize;
    int error = 0;

    if (bc->multiple) {
	num = yasm_expr_get_intnum(&bc->multiple, NULL);
	if (!num)
	    yasm_internal_error(
		N_("could not determine multiple in bc_tobytes"));
	if (yasm_intnum_sign(num) < 0) {
	    yasm_error_set(YASM_ERROR_VALUE, N_("multiple is negative"));
	    *bufsize = 0;
	    return NULL;
	}
	*multiple = yasm_intnum_get_uint(num);
	if (*multiple == 0) {
	    *bufsize = 0;
	    return NULL;
	}
    } else
	*multiple = 1;

    datasize = bc->len / (*multiple);

    /* special case for reserve bytecodes */
    if (bc->callback->reserve) {
    	*bufsize = datasize;
	*gap = 1;
	return NULL;	/* we didn't allocate a buffer */
    }

    *gap = 0;

    if (*bufsize < datasize) {
	mybuf = yasm_xmalloc(bc->len);
	origbuf = mybuf;
	destbuf = mybuf;
    } else {
	origbuf = buf;
	destbuf = buf;
    }
    *bufsize = datasize;

    if (!bc->callback)
	yasm_internal_error(N_("got empty bytecode in bc_tobytes"));
    else
	error = bc->callback->tobytes(bc, &destbuf, d, output_value,
				      output_reloc);

    if (!error && ((unsigned long)(destbuf - origbuf) != datasize))
	yasm_internal_error(
	    N_("written length does not match optimized length"));
    return mybuf;
}
