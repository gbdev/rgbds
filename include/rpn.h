/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2021, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/* Generic hashmap implementation (C++ templates are calling...) */
#ifndef RGBDS_RPN_H
#define RGBDS_RPN_H

#include <stdbool.h>
#include <stdint.h>

#include "asm/warning.h"

#include "linkdefs.h"

struct Result { // In the Rust sense...
	union {
		uint32_t value;        // For NO_ERR
		struct Symbol *symbol; // For ERR_SYM
		size_t exprSize;       // For ERR_UNK, size of the RPN expression (for discarding)
	};
	// Some notes about how these are handled...
	// Constant values are not written immediately to the RPN buffer, as they might be reduced
	// Symbols follow the same logic, since a subtraction may reduce them
	// Errors are not written directly to the buffer, unless short-circuiting might discard them
	//   at link time (example: `(Label & 1) && 1 / 0`)
	enum ValueError {
		NO_ERR,             // No error
		ERR_SYM,            // Same, but it's a symbol
		ERR_UNK,            // Result has non-constant value
#define ERR(name) ERR_##name = RPN_ERR_##name
		ERR(NO_SELF_BANK),  // BANK(@) outside of a SECTION
		ERR(DIV_BY_0),      // Division by 0
		ERR(MOD_BY_0),      // Modulo by 0
		ERR(BANK_NOT_SYM),  // BANK(Sym), but `Sym` is not a label
		ERR(EXP_NEG_POW),   // Exponentiation by negative power
#undef ERR
#define WARN(name) WARN_##name = WARNING_##name
		WARN(SHIFT_AMOUNT), // Shifting by a negative or large amount
		WARN(SHIFT),        // Shifting a negative value right
		WARN(DIV),          // Division of INT32_MIN by -1
#undef WARN
	} error;
};
#define Ok(val) ((struct Result){ .value = (val), .error = NO_ERR })
#define Sym(sym) ((struct Result){ .symbol = (sym), .error = ERR_SYM })
#define Unk(size) ((struct Result){ .exprSize = (size), .error = ERR_UNK })
#define Err(err) ((struct Result){ .error = (enum ValueError)(err) })

static inline bool is_ok(struct Result const *res)
{
	return res->error == NO_ERR;
}

static inline bool is_unk(struct Result const *res)
{
	return res->error == ERR_UNK || res->error == ERR_SYM;
}

static inline bool is_err(struct Result const *res)
{
	return !is_ok(res) && !is_unk(res);
}

enum UnaryError {
	UN_ERR_HRAM,
	UN_ERR_RST,
};
typedef void (*UnaryCallback)(enum UnaryError type, int32_t value);
// Pass NULL as a callback to have error-generating expressions
uint32_t rpn_ConstUnaryOp(enum RPNCommand op, uint32_t value, UnaryCallback error);

enum BinaryWarning {
	BIN_WARN_SHL_NEG,   // Shifting left by a negative amount
	BIN_WARN_SHL_LARGE, // Shifting left by more than 32
	BIN_WARN_NEG_SHR,   // Shifting a negative value right
	BIN_WARN_SHR_NEG,   // Shifting right by a negative amount
	BIN_WARN_SHR_LARGE, // Shifting right by more than 32
	BIN_WARN_DIV,       // Dividing INT32_MIN by -1
};
typedef void (*BinaryCallback)(enum BinaryWarning type, int32_t lhs, int32_t rhs);
// Pass NULL as a callback to have warning-generating expressions be returned as "unknown"
struct Result rpn_ConstBinaryOp(int32_t lhs, enum RPNCommand op, int32_t rhs,
				BinaryCallback warningCallback);

#endif /* RGBDS_RPN_H */
