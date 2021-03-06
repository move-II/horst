#include "safemode_req.h"

#include "../state/state.h"


namespace horst {

SafeModeReq::SafeModeReq(bool wanted) : wanted(wanted) {}

bool SafeModeReq::is_fact() const {
	return false;
}

void SafeModeReq::update(State &state) {
	state.safemode = this->wanted;
	state.enforced_safemode = true;
}

}  // horst
