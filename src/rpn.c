
#include "helpers.h"
#include "opmath.h"
#include "rpn.h"

uint32_t rpn_ConstUnaryOp(enum RPNCommand op, uint32_t value, UnaryCallback errorCallback)
{
#define ERR(type) do { \
	if (errorCallback) \
		errorCallback((type), value); \
} while (0)

	switch (op) {
	case RPN_ISCONST:
		return 1; // If this is known to be constant at parse time, it will always be

	case RPN_UNSUB:
		return -value;

	case RPN_UNNOT:
		return ~value;

	case RPN_LOGUNNOT:
		return !value;

	// Normally, we'd return an error RPN byte so that errors can be discarded
	// However, HRAM and RST checks are always called last, so their errors cannot be discarded
	case RPN_HRAM:
		if (value >= 0xFF00 && value <= 0xFFFF) {
			// That range is valid, but only keep the lower byte
			value &= 0xFF;
		} else if (value < 0 || value > 0xFF) {
			ERR(UN_ERR_HRAM);
		}
		return value;

	case RPN_RST:
		// A valid RST address must be masked with 0x38
		if (value & ~0x38)
			ERR(UN_ERR_RST);
		// The target addr is in the "0x38" bits, all other bits are set
		return value | 0xC7;

	case RPN_ADD:
	case RPN_SUB:
	case RPN_MUL:
	case RPN_DIV:
	case RPN_MOD:
	case RPN_EXP:
	case RPN_OR:
	case RPN_AND:
	case RPN_XOR:
	case RPN_LOGAND:
	case RPN_LOGOR:
	case RPN_LOGEQ:
	case RPN_LOGNE:
	case RPN_LOGGT:
	case RPN_LOGLT:
	case RPN_LOGGE:
	case RPN_LOGLE:
	case RPN_SHL:
	case RPN_SHR:
	case RPN_BANK_SYM:
	case RPN_BANK_SECT:
	case RPN_BANK_SELF:
	case RPN_SIZEOF_SECT:
	case RPN_STARTOF_SECT:
	case RPN_CONST:
	case RPN_SYM:
	case RPN_ERR_NO_SELF_BANK:
	case RPN_ERR_DIV_BY_0:
	case RPN_ERR_MOD_BY_0:
	case RPN_ERR_BANK_NOT_SYM:
	case RPN_ERR_EXP_NEG_POW:
		break;
	}
	unreachable_();

#undef ERR
}


struct Result rpn_ConstBinaryOp(int32_t lhs, enum RPNCommand op, int32_t rhs,
				BinaryCallback warningCallback)
{
#define WARN(type) do { \
	if (warningCallback) \
		warningCallback((type), lhs, rhs); \
	else \
		return Err(ERR_UNK); \
} while (0)

	switch (op) {
	case RPN_LOGOR:
		return Ok(lhs || rhs);

	case RPN_LOGAND:
		return Ok(lhs && rhs);

	case RPN_LOGEQ:
		return Ok(lhs == rhs);

	case RPN_LOGGT:
		return Ok(lhs > rhs);

	case RPN_LOGLT:
		return Ok(lhs < rhs);

	case RPN_LOGGE:
		return Ok(lhs >= rhs);

	case RPN_LOGLE:
		return Ok(lhs <= rhs);

	case RPN_LOGNE:
		return Ok(lhs != rhs);

	case RPN_ADD:
		return Ok(lhs + rhs);

	case RPN_SUB:
		return Ok(lhs - rhs);

	case RPN_XOR:
		return Ok(lhs ^ rhs);

	case RPN_OR:
		return Ok(lhs | rhs);

	case RPN_AND:
		return Ok(lhs & rhs);

		// TODO: defer all of these warnings/errors
	case RPN_SHL:
		if (rhs < 0)
			WARN(BIN_WARN_SHL_NEG);

		if (rhs >= 32)
			WARN(BIN_WARN_SHL_LARGE);

		return Ok(op_shift_left(lhs, rhs));

	case RPN_SHR:
		if (lhs < 0)
			WARN(BIN_WARN_NEG_SHR);

		if (rhs < 0)
			WARN(BIN_WARN_SHR_NEG);

		if (rhs >= 32)
			WARN(BIN_WARN_SHR_LARGE);

		return Ok(op_shift_right(lhs, rhs));

	case RPN_MUL:
		return Ok(lhs * rhs);

	case RPN_DIV:
		if (rhs == 0)
			return Err(ERR_DIV_BY_0);

		if (lhs == INT32_MIN && rhs == -1) {
			WARN(BIN_WARN_DIV);
			return Ok(INT32_MIN);
		} else {
			return Ok(op_divide(lhs, rhs));
		}

	case RPN_MOD:
		if (rhs == 0)
			return Err(ERR_MOD_BY_0);

		if (lhs == INT32_MIN && rhs == -1)
			return Ok(0);
		else
			return Ok(op_modulo(lhs, rhs));

	case RPN_EXP:
		if (rhs < 0)
			return Err(ERR_EXP_NEG_POW);

		return Ok(op_exponent(lhs, rhs));

	case RPN_ISCONST:
	case RPN_UNSUB:
	case RPN_UNNOT:
	case RPN_LOGUNNOT:
	case RPN_BANK_SYM:
	case RPN_BANK_SECT:
	case RPN_BANK_SELF:
	case RPN_SIZEOF_SECT:
	case RPN_STARTOF_SECT:
	case RPN_HRAM:
	case RPN_RST:
	case RPN_CONST:
	case RPN_SYM:
	case RPN_ERR_NO_SELF_BANK:
	case RPN_ERR_DIV_BY_0:
	case RPN_ERR_MOD_BY_0:
	case RPN_ERR_BANK_NOT_SYM:
	case RPN_ERR_EXP_NEG_POW:
		break;
	}
	unreachable_();

#undef WARN
}
