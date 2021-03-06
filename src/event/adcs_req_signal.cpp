#include "adcs_req_signal.h"

#include "../state/state.h"


namespace horst {

ADCSreqSignal::ADCSreqSignal(ADCS::adcs_state adcs_status) : adcs_status(adcs_status) {}

bool ADCSreqSignal::is_fact() const {
	return true;
}

void ADCSreqSignal::update(State &state) {
	state.adcs.requested = this->adcs_status;

	// Reset pointing, since we do not know anymore, what we are
	// currently pointing at
	state.adcs.pointing = ADCS::adcs_state::NONE;
}

}  // horst
