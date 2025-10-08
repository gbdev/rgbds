// SPDX-License-Identifier: MIT

#ifndef RGBDS_DIAGNOSTICS_HPP
#define RGBDS_DIAGNOSTICS_HPP

#include <inttypes.h>
#include <optional>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

#include "helpers.hpp"
#include "itertools.hpp"

[[gnu::format(printf, 1, 2)]]
void warnx(char const *fmt, ...);

enum WarningAbled { WARNING_DEFAULT, WARNING_ENABLED, WARNING_DISABLED };

struct WarningState {
	WarningAbled state;
	WarningAbled error;

	void update(WarningState other);
};

std::pair<WarningState, std::optional<uint32_t>> getInitialWarningState(std::string &flag);

template<typename LevelEnumT>
struct WarningFlag {
	char const *name;
	LevelEnumT level;
};

enum WarningBehavior { DISABLED, ENABLED, ERROR };

template<typename WarningEnumT>
struct ParamWarning {
	WarningEnumT firstID;
	WarningEnumT lastID;
	uint8_t defaultLevel;
};

template<typename WarningEnumT>
struct DiagnosticsState {
	WarningState flagStates[WarningEnumT::NB_WARNINGS];
	WarningState metaStates[WarningEnumT::NB_WARNINGS];
	bool warningsEnabled = true;
	bool warningsAreErrors = false;
};

template<typename LevelEnumT, typename WarningEnumT>
struct Diagnostics {
	std::vector<WarningFlag<LevelEnumT>> metaWarnings;
	std::vector<WarningFlag<LevelEnumT>> warningFlags;
	std::vector<ParamWarning<WarningEnumT>> paramWarnings;
	DiagnosticsState<WarningEnumT> state;
	uint64_t nbErrors;

	void incrementErrors() {
		if (nbErrors != UINT64_MAX) {
			++nbErrors;
		}
	}

	WarningBehavior getWarningBehavior(WarningEnumT id) const;
	void processWarningFlag(char const *flag);
};

template<typename LevelEnumT, typename WarningEnumT>
WarningBehavior Diagnostics<LevelEnumT, WarningEnumT>::getWarningBehavior(WarningEnumT id) const {
	// Check if warnings are globally disabled
	if (!state.warningsEnabled) {
		return WarningBehavior::DISABLED;
	}

	// Get the state of this warning flag
	WarningState const &flagState = state.flagStates[id];
	WarningState const &metaState = state.metaStates[id];

	// If subsequent checks determine that the warning flag is enabled, this checks whether it has
	// -Werror without -Wno-error=<flag> or -Wno-error=<meta>, which makes it into an error
	bool warningIsError = state.warningsAreErrors && flagState.error != WARNING_DISABLED
	                      && metaState.error != WARNING_DISABLED;
	WarningBehavior enabledBehavior =
	    warningIsError ? WarningBehavior::ERROR : WarningBehavior::ENABLED;

	// First, check the state of the specific warning flag
	if (flagState.state == WARNING_DISABLED) { // -Wno-<flag>
		return WarningBehavior::DISABLED;
	}
	if (flagState.error == WARNING_ENABLED) { // -Werror=<flag>
		return WarningBehavior::ERROR;
	}
	if (flagState.state == WARNING_ENABLED) { // -W<flag>
		return enabledBehavior;
	}

	// If no flag is specified, check the state of the "meta" flags that affect this warning flag
	if (metaState.state == WARNING_DISABLED) { // -Wno-<meta>
		return WarningBehavior::DISABLED;
	}
	if (metaState.error == WARNING_ENABLED) { // -Werror=<meta>
		return WarningBehavior::ERROR;
	}
	if (metaState.state == WARNING_ENABLED) { // -W<meta>
		return enabledBehavior;
	}

	// If no meta flag is specified, check the default state of this warning flag
	if (warningFlags[id].level == LevelEnumT::LEVEL_DEFAULT) { // enabled by default
		return enabledBehavior;
	}

	// No flag enables this warning, explicitly or implicitly
	return WarningBehavior::DISABLED;
}

template<typename LevelEnumT, typename WarningEnumT>
void Diagnostics<LevelEnumT, WarningEnumT>::processWarningFlag(char const *flag) {
	std::string rootFlag = flag;

	// Check for `-Werror` or `-Wno-error` to return early
	if (rootFlag == "error") {
		// `-Werror` promotes warnings to errors
		state.warningsAreErrors = true;
		return;
	} else if (rootFlag == "no-error") {
		// `-Wno-error` disables promotion of warnings to errors
		state.warningsAreErrors = false;
		return;
	}

	auto [flagState, param] = getInitialWarningState(rootFlag);

	// Try to match the flag against a parametric warning
	// If there was an equals sign, it will have set `param`; if not, `param` will be 0,
	// which applies to all levels
	for (ParamWarning<WarningEnumT> const &paramWarning : paramWarnings) {
		WarningEnumT baseID = paramWarning.firstID;
		uint8_t maxParam = paramWarning.lastID - baseID + 1;
		assume(paramWarning.defaultLevel <= maxParam);

		if (rootFlag != warningFlags[baseID].name) {
			continue;
		}

		// If making the warning an error but param is 0, set to the maximum
		// This accommodates `-Werror=<flag>`, but also `-Werror=<flag>=0`, which is
		// thus filtered out by the caller.
		// A param of 0 makes sense for disabling everything, but neither for
		// enabling nor "erroring". Use the default for those.
		if (!param.has_value() || *param == 0) {
			param = paramWarning.defaultLevel;
		} else if (*param > maxParam) {
			warnx(
			    "Invalid warning flag parameter \"%s=%" PRIu32 "\"; capping at maximum %" PRIu8,
			    rootFlag.c_str(),
			    *param,
			    maxParam
			);
			*param = maxParam;
		}

		// Set the first <param> to enabled/error, and disable the rest
		for (uint32_t ofs = 0; ofs < maxParam; ++ofs) {
			if (WarningState &warning = state.flagStates[baseID + ofs]; ofs < *param) {
				warning.update(flagState);
			} else {
				warning.state = WARNING_DISABLED;
			}
		}
		return;
	}

	if (param.has_value()) {
		warnx("Unknown warning flag parameter \"%s=%" PRIu32 "\"", rootFlag.c_str(), *param);
		return;
	}

	// Try to match against a "meta" warning
	for (WarningFlag<LevelEnumT> const &metaWarning : metaWarnings) {
		if (rootFlag != metaWarning.name) {
			continue;
		}

		// Set each of the warning flags that meets this level
		for (WarningEnumT id : EnumSeq(WarningEnumT::NB_WARNINGS)) {
			if (metaWarning.level >= warningFlags[id].level) {
				state.metaStates[id].update(flagState);
			}
		}
		return;
	}

	// Try to match against a "normal" flag
	for (WarningEnumT id : EnumSeq(WarningEnumT::NB_PLAIN_WARNINGS)) {
		if (rootFlag == warningFlags[id].name) {
			state.flagStates[id].update(flagState);
			return;
		}
	}

	warnx("Unknown warning flag \"%s\"", rootFlag.c_str());
}

#endif // RGBDS_DIAGNOSTICS_HPP
