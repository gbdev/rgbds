// SPDX-License-Identifier: MIT

#ifndef RGBDS_DIAGNOSTICS_HPP
#define RGBDS_DIAGNOSTICS_HPP

#include <inttypes.h>
#include <optional>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
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

std::pair<WarningState, std::optional<uint8_t>> getInitialWarningState(std::string &flag);

template<typename L>
struct WarningFlag {
	char const *name;
	L level;
};

enum WarningBehavior { DISABLED, ENABLED, ERROR };

template<typename W>
struct ParamWarning {
	W firstID;
	W lastID;
	uint8_t defaultLevel;
};

template<typename W>
struct DiagnosticsState {
	WarningState flagStates[W::NB_WARNINGS];
	WarningState metaStates[W::NB_WARNINGS];
	bool warningsEnabled = true;
	bool warningsAreErrors = false;
};

template<typename L, typename W>
struct Diagnostics {
	std::vector<WarningFlag<L>> metaWarnings;
	std::vector<WarningFlag<L>> warningFlags;
	std::vector<ParamWarning<W>> paramWarnings;
	DiagnosticsState<W> state;

	WarningBehavior getWarningBehavior(W id) const;
	std::string processWarningFlag(char const *flag);
};

template<typename L, typename W>
WarningBehavior Diagnostics<L, W>::getWarningBehavior(W id) const {
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
	if (warningFlags[id].level == L::LEVEL_DEFAULT) { // enabled by default
		return enabledBehavior;
	}

	// No flag enables this warning, explicitly or implicitly
	return WarningBehavior::DISABLED;
}

template<typename L, typename W>
std::string Diagnostics<L, W>::processWarningFlag(char const *flag) {
	std::string rootFlag = flag;

	// Check for `-Werror` or `-Wno-error` to return early
	if (rootFlag == "error") {
		// `-Werror` promotes warnings to errors
		state.warningsAreErrors = true;
		return rootFlag;
	} else if (rootFlag == "no-error") {
		// `-Wno-error` disables promotion of warnings to errors
		state.warningsAreErrors = false;
		return rootFlag;
	}

	auto [flagState, param] = getInitialWarningState(rootFlag);

	// Try to match the flag against a parametric warning
	// If there was an equals sign, it will have set `param`; if not, `param` will be 0, which
	// applies to all levels
	for (auto const &paramWarning : paramWarnings) {
		W baseID = paramWarning.firstID;
		uint8_t maxParam = paramWarning.lastID - baseID + 1;
		assume(paramWarning.defaultLevel <= maxParam);

		if (rootFlag == warningFlags[baseID].name) { // Match!
			// If making the warning an error but param is 0, set to the maximum
			// This accommodates `-Werror=<flag>`, but also `-Werror=<flag>=0`, which is
			// thus filtered out by the caller.
			// A param of 0 makes sense for disabling everything, but neither for
			// enabling nor "erroring". Use the default for those.
			if (!param.has_value() || *param == 0) {
				param = paramWarning.defaultLevel;
			} else if (*param > maxParam) {
				if (*param != 255) { // Don't warn if already capped
					warnx(
					    "Invalid parameter %" PRIu8
					    " for warning flag \"%s\"; capping at maximum %" PRIu8,
					    *param,
					    rootFlag.c_str(),
					    maxParam
					);
				}
				*param = maxParam;
			}

			// Set the first <param> to enabled/error, and disable the rest
			for (uint8_t ofs = 0; ofs < maxParam; ofs++) {
				WarningState &warning = state.flagStates[baseID + ofs];
				if (ofs < *param) {
					warning.update(flagState);
				} else {
					warning.state = WARNING_DISABLED;
				}
			}
			return rootFlag;
		}
	}

	// Try to match against a non-parametric warning, unless there was an equals sign
	if (!param.has_value()) {
		// Try to match against a "meta" warning
		for (WarningFlag<L> const &metaWarning : metaWarnings) {
			if (rootFlag == metaWarning.name) {
				// Set each of the warning flags that meets this level
				for (W id : EnumSeq(W::NB_WARNINGS)) {
					if (metaWarning.level >= warningFlags[id].level) {
						state.metaStates[id].update(flagState);
					}
				}
				return rootFlag;
			}
		}

		// Try to match the flag against a "normal" flag
		for (W id : EnumSeq(W::NB_PLAIN_WARNINGS)) {
			if (rootFlag == warningFlags[id].name) {
				state.flagStates[id].update(flagState);
				return rootFlag;
			}
		}
	}

	warnx("Unknown warning flag \"%s\"", flag);
	return rootFlag;
}

#endif // RGBDS_DIAGNOSTICS_HPP
