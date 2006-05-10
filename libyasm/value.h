/**
 * \file libyasm/value.h
 * \brief YASM value interface.
 *
 * \rcs
 * $Id$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2006  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
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
 * \endlicense
 */
#ifndef YASM_VALUE_H
#define YASM_VALUE_H

/** Initialize a #yasm_value with just an expression.  No processing is
 * performed, the expression is simply stuck into value.abs and the other
 * fields are initialized.  Use yasm_expr_extract_value() to perform "smart"
 * processing into a #yasm_value.  This function is intended for use during
 * parsing simply to ensure all fields of the value are initialized; after
 * the parse is complete, yasm_value_extract() should be called to finalize
 * the value.
 * \param value	    value to be initialized
 * \param e	    expression (kept)
 * \param size	    value size (in bits)
 */
void yasm_value_initialize(/*@out@*/ yasm_value *value,
			   /*@null@*/ /*@kept@*/ yasm_expr *e,
			   unsigned int size);

/** Initialize a #yasm_value with just a symrec.  No processing is performed,
 * the symrec is simply stuck into value.rel and the other fields are
 * initialized.
 * \param value	    value to be initialized
 * \param sym	    symrec
 */
void yasm_value_init_sym(/*@out@*/ yasm_value *value,
			 /*@null@*/ yasm_symrec *sym);

/** Frees any memory inside value; does not free value itself.
 * \param value	    value
 */
void yasm_value_delete(yasm_value *value);

/** Perform yasm_value_finalize_expr() on a value that already exists from
 * being initialized with yasm_value_initialize().
 * \param value		value
 * \return Nonzero if value could not be split.
 */
int yasm_value_finalize(yasm_value *value);

/** Break a #yasm_expr into a #yasm_value constituent parts.  Extracts
 * the relative portion of the value, SEG and WRT portions, and top-level
 * right shift, if any.  Places the remaining expr into the absolute
 * portion of the value.  Essentially a combination of yasm_value_initialize()
 * and yasm_value_finalize().  First expands references to symrecs in
 * absolute sections by expanding with the absolute section start plus the
 * symrec offset within the absolute section.
 * \param value		value to store split portions into
 * \param e		expression input
 * \param size		value size (in bits)
 * \return Nonzero if the expr could not be split into a value for some
 *         reason (e.g. the relative portion was not added, but multiplied,
 *         etc).
 * \caution Do not use e after this call.  Even if an error is returned, e
 *          is stored into value.
 * \note This should only be called after the parse is complete.  Calling
 *       before the parse is complete will usually result in an error return.
 */
int yasm_value_finalize_expr(/*@out@*/ yasm_value *value,
			     /*@null@*/ /*@kept@*/ yasm_expr *e,
			     unsigned int size);

/** Output value if constant or PC-relative section-local.  This should be
 * used from objfmt yasm_output_value_func() functions.
 * functions.
 * \param value		value
 * \param buf		buffer for byte representation
 * \param destsize	destination size (in bytes)
 * \param bc		current bytecode (usually passed into higher-level
 *			calling function)
 * \param warn		enables standard warnings: zero for none;
 *			nonzero for overflow/underflow floating point warnings;
 *			negative for signed integer warnings,
 *			positive for unsigned integer warnings
 * \param arch		architecture
 * \param calc_bc_dist	function used to determine bytecode distance
 * \note Adds in value.rel (correctly) if PC-relative and in the same section
 *       as bc (and there is no WRT or SEG); if this is not the desired
 *       behavior, e.g. a reloc is needed in this case, don't use this
 *       function!
 * \return 0 if no value output due to value needing relocation;
 *         1 if value output; -1 if error.
 */
int yasm_value_output_basic
    (yasm_value *value, /*@out@*/ unsigned char *buf, size_t destsize,
     yasm_bytecode *bc, int warn, yasm_arch *arch,
     yasm_calc_bc_dist_func calc_bc_dist);

/** Print a value.  For debugging purposes.
 * \param value		value
 * \param indent_level	indentation level
 * \param f		file
 */
void yasm_value_print(const yasm_value *value, FILE *f, int indent_level);


#ifndef YASM_DOXYGEN
#define yasm_value_initialize(value, e, sz) \
    do { \
	(value)->abs = e; \
	(value)->rel = NULL; \
	(value)->wrt = NULL; \
	(value)->seg_of = 0; \
	(value)->rshift = 0; \
	(value)->curpos_rel = 0; \
	(value)->ip_rel = 0; \
	(value)->section_rel = 0; \
	(value)->size = sz; \
    } while(0)

#define yasm_value_init_sym(value, sym, sz) \
    do { \
	(value)->abs = NULL; \
	(value)->rel = sym; \
	(value)->wrt = NULL; \
	(value)->seg_of = 0; \
	(value)->rshift = 0; \
	(value)->curpos_rel = 0; \
	(value)->ip_rel = 0; \
	(value)->section_rel = 0; \
	(value)->size = sz; \
    } while(0)

#define yasm_value_delete(value) \
    do { \
	yasm_expr_destroy((value)->abs); \
	(value)->abs = NULL; \
	(value)->rel = NULL; \
    } while(0)
#endif

#endif
