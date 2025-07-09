#include "diagnostics.hpp"

void warnx(char const *fmt, ...) {
	va_list ap;
	fputs("warning: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);
}

void WarningState::update(WarningState other) {
	if (other.state != WARNING_DEFAULT) {
		state = other.state;
	}
	if (other.error != WARNING_DEFAULT) {
		error = other.error;
	}
}

std::pair<WarningState, std::optional<uint8_t>> getInitialWarningState(std::string &flag) {
	// Check for prefixes that affect what the flag does
	WarningState state;
	if (flag.starts_with("error=")) {
		// `-Werror=<flag>` enables the flag as an error
		state = {.state = WARNING_ENABLED, .error = WARNING_ENABLED};
		flag.erase(0, literal_strlen("error="));
	} else if (flag.starts_with("no-error=")) {
		// `-Wno-error=<flag>` prevents the flag from being an error,
		// without affecting whether it is enabled
		state = {.state = WARNING_DEFAULT, .error = WARNING_DISABLED};
		flag.erase(0, literal_strlen("no-error="));
	} else if (flag.starts_with("no-")) {
		// `-Wno-<flag>` disables the flag
		state = {.state = WARNING_DISABLED, .error = WARNING_DEFAULT};
		flag.erase(0, literal_strlen("no-"));
	} else {
		// `-W<flag>` enables the flag
		state = {.state = WARNING_ENABLED, .error = WARNING_DEFAULT};
	}

	// Check if there is an "equals" sign followed by a decimal number
	// Ignore an equals sign at the very end of the string
	auto equals = flag.find('=');
	// `-Wno-<flag>` and `-Wno-error=<flag>` negation cannot have an `=` parameter, but without
	// one, the 0 value will apply to all levels of a parametric warning
	if (state.state != WARNING_ENABLED || equals == flag.npos || equals == flag.size() - 1) {
		return {state, std::nullopt};
	}

	// Is the rest of the string a decimal number?
	// We want to avoid `strtoul`'s whitespace and sign, so we parse manually
	char const *ptr = flag.c_str() + equals + 1;
	uint8_t param = 0;
	bool warned = false;

	// The `if`'s condition above ensures that this will run at least once
	do {
		// If we don't have a digit, bail
		if (*ptr < '0' || *ptr > '9') {
			break;
		}
		// Avoid overflowing!
		if (param > UINT8_MAX - (*ptr - '0')) {
			if (!warned) {
				warnx("Invalid warning flag \"%s\": capping parameter at 255", flag.c_str());
			}
			warned = true; // Only warn once, cap always
			param = 255;
			continue;
		}
		param = param * 10 + (*ptr - '0');

		ptr++;
	} while (*ptr);

	// If we reached the end of the string, truncate it at the '='
	if (*ptr == '\0') {
		flag.resize(equals);
		// `-W<flag>=0` is equivalent to `-Wno-<flag>`
		if (param == 0) {
			state.state = WARNING_DISABLED;
		}
	}

	return {state, param};
}
