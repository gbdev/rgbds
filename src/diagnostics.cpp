#include "diagnostics.hpp"

void WarningState::update(WarningState other) {
	if (other.state != WARNING_DEFAULT) {
		state = other.state;
	}
	if (other.error != WARNING_DEFAULT) {
		error = other.error;
	}
}
