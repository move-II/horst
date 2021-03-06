#include "satellite.h"

#include <iostream>
#include <memory>
#include <systemd/sd-daemon.h>
#include <uv.h>

#include "action/action.h"
#include "util.h"

namespace horst {

Satellite::Satellite(const arguments &args)
	:
	args{args},
	loop{},
	s3tp_link{args.port, args.socketpath},
	dbus{this},
	next_id{0}
	{

	this->current_state.manualmode = args.startmanual;
	this->current_state.battery_treshold = args.battery_treshold;
	this->current_state.eps.battery_level = args.battery_treshold;
	this->current_state.leop = args.leop;
	uv_loop_init(&this->loop);

	// Init watchdog timer
	uint64_t watchdog_period = 0;
	sd_watchdog_enabled(false, &watchdog_period);
	if (watchdog_period > 0) {
		uv_timer_init(&this->loop, &this->timer);
		uv_timer_start(
			&this->timer,
			[] (uv_timer_t *) {
				sd_notify(false, "WATCHDOG=1");
			},
			0, // time in milliseconds, do first notify immediatelly
			watchdog_period/2/1000 // notify after half of watchdog timeout
		);
	}
}


Satellite::~Satellite() {
	LOG_DEBUG("[satellite] destroying...");
	uv_loop_close(&this->loop);
}


int Satellite::run() {
	LOG_INFO("[satellite] starting up connections...");
	int ret;

	if (this->dbus.connect()) {
		LOG_ERROR(9, "[satellite] Failed to listen on DBus.");
		return 1;
	}

	if (!this->s3tp_link.start(&this->loop)) {
		LOG_ERROR(10, "[satellite] Failed to listen on S3TP.");
	}

	// let the event loop run forever.
	LOG_INFO("[satellite] Starting event loop");
	ret = uv_run(&this->loop, UV_RUN_DEFAULT);
	LOG_INFO("[satellite] Stopping event loop");
	return ret;
}


std::string Satellite::get_scripts_path() {
	return args.scripts;
}


uv_loop_t *Satellite::get_loop() {
	return &this->loop;
}


id_t Satellite::add_action(std::unique_ptr<Action> &&action) {
	this->actions.emplace(this->next_id, std::move(action));
	return this->next_id++;
}


Action *Satellite::get_action(id_t id) {
	auto loc = this->actions.find(id);
	if (loc == std::end(this->actions)) {
		return nullptr;
	} else {
		return loc->second.get();
	}
}

State *Satellite::get_state() {
	return &this->current_state;
}


S3TPServer *Satellite::get_s3tp() {
	return &this->s3tp_link;
}


void Satellite::remove_action(id_t id) {
	auto pos = this->actions.find(id);
	if (pos != std::end(this->actions)) {
		this->actions.erase(pos);
	} else {
		LOG_WARN("[satellite] Attempt to remove an unknown action");
	}
}


void Satellite::on_event(std::shared_ptr<Event> &&event) {
	// called for each event the satellite receives
	// it may come from earth or any other subsystem

	// if the event is a fact, update the current state
	if (event->is_fact()) {
		event->update(this->current_state);
	}

	// create the target state as a copy of the current state
	State target_state = this->current_state.copy();

	// it the event is a request (i.e. not a fact),
	// update it in the target state
	if (not event->is_fact()) {
		event->update(target_state);
	}

	// determine the actions needed to reach the target state
	auto actions = this->current_state.transform_to(target_state);

	for (auto &action_m : actions) {
		// store the action
		id_t id = this->add_action(std::move(action_m));

		// and fetch its new location
		Action *action = this->get_action(id);
		LOG_INFO("[action] run #" + std::to_string(id) + ": " + action->describe());

		// perform the action, this may just enqueue it in the event loop.
		// the callback is executed when the action is done.
		action->perform(
			this,
			[this, id] (bool success, Action *) {
				if (not success) {
					LOG_WARN("[action] #" + std::to_string(id) + " failed!");
				}
				else {
					LOG_INFO("[action] #" + std::to_string(id) + " succeeded");
				}

				// the last action in here must be the free of the
				// action from memory:
				this->remove_action(id);
			}
		);
	}
}


} // horst
