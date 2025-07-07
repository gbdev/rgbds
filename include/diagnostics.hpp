// SPDX-License-Identifier: MIT

#ifndef RGBDS_DIAGNOSTICS_HPP
#define RGBDS_DIAGNOSTICS_HPP

enum WarningAbled { WARNING_DEFAULT, WARNING_ENABLED, WARNING_DISABLED };

struct WarningState {
	WarningAbled state;
	WarningAbled error;

	void update(WarningState other);
};

#endif // RGBDS_DIAGNOSTICS_HPP
