#include "Plane.h"

#include <iostream>
#include <time.h>
#include <sys/siginfo.h>

#include "constants.h"

ostream& operator<<(ostream &os, const Vec3 &vec) {
	std::cout << '<' << vec.x << ", " << vec.y << ", " << vec.z << '>';
	return os;
}

Plane::Plane(PlaneStartParams &params) :
		startParams(params), currentPosition { -1, -1, -1 }, currentVelocity {
				-1, -1, -1 }, arrived(false), left(false), chid(-1) {
}

int Plane::getChid() const {
	return chid;
}

int Plane::getPlaneId() const {
	return startParams.id;
}

void Plane::run() {
	// Create a new communication channel belonging to the plane and store the handle in chid.
	if ((chid = ChannelCreate(0)) == -1) {
		std::cout << "Plane " << startParams.id
				<< ": channel creation failed. Exiting thread." << std::endl;
		return;
	}

	// Open a client to our own connection to be used for timer pulses and store the handle in coid.
	int coid;
	if ((coid = ConnectAttach(0, 0, chid, 0, 0)) == -1) {
		std::cout << "Plane " << startParams.id
				<< ": failed to attach to self. Exiting thread.";
		return;
	}

	// Initialize a sigevent to send a pulse.
	struct sigevent sigev;
	SIGEV_PULSE_INIT(&sigev, coid, SIGEV_PULSE_PRIO_INHERIT, CODE_TIMER, 0);

	timer_t updateTimer;
	if (timer_create(CLOCK_MONOTONIC, &sigev, &updateTimer) == -1) {
		std::cout << "Plane " << startParams.id
				<< ": failed to initialize update timer. Exiting thread.";
		return;
	}

	// Set the timer to fire once at the arrival time, and then every second thereafter.
	struct itimerspec timerValue;
	timerValue.it_value.tv_sec = startParams.arrivalTime;
	timerValue.it_value.tv_nsec = 0;
	timerValue.it_interval.tv_sec = POSITION_UPDATE_INTERVAL_SECONDS;
	timerValue.it_interval.tv_nsec = 0;

	// Start the timer.
	timer_settime(updateTimer, 0, &timerValue, NULL);

	// Start listening for messages
	listen();
}

void Plane::listen() {
	int rcvid;
	PlaneCommandMessage msg;
	while (1) {
		// Wait for any type of message.
		rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

		if (rcvid == 0) {
			// We've received a pulse.
			switch (msg.header.code) {
			case CODE_TIMER:
				// The timer fires for the first time when the plane initially enters the airspace.
				if (!arrived) {
					arrived = true;
					currentPosition = startParams.initialPosition;
					currentVelocity = startParams.initialVelocity;
					break;
				} else if (left) {
					// If we've left the airspace, stop updating our position.
					break;
				}
				// Once the plane is in the airspace, update its position whenever the timer fires.
				updatePosition();
				break;
			default:
				std::cout << "Plane " << startParams.id
						<< ": received pulse with unknown code "
						<< msg.header.code << std::endl;
				break;
			}
		} else {
			// We've received a user message.
			switch (msg.command) {
			case COMMAND_RADAR_PING: {
				// The plane responds with its current position and velocity.
				PlanePositionResponse res { currentPosition, currentVelocity };
				MsgReply(rcvid, EOK, &res, sizeof(res));
				break;
			}
			case COMMAND_SET_VELOCITY:
				// The plane accepts a new velocity.
				currentVelocity = msg.newVelocity;
				MsgReply(rcvid, EOK, NULL, 0);
				break;
			case COMMAND_EXIT_THREAD:
				// Required to allow all threads to gracefully terminate when the program is terminating
				MsgReply(rcvid, EOK, NULL, 0);
				return;
			default:
				std::cout << "Plane " << startParams.id
						<< ": received unknown command " << msg.command
						<< std::endl;
				MsgError(rcvid, ENOSYS);
				break;
			}
		}
	}
}

// Updates the plane's position based on its velocity.
void Plane::updatePosition() {
	currentPosition.x += currentVelocity.x * POSITION_UPDATE_INTERVAL_SECONDS;
	currentPosition.y += currentVelocity.y * POSITION_UPDATE_INTERVAL_SECONDS;
	currentPosition.z += currentVelocity.z * POSITION_UPDATE_INTERVAL_SECONDS;
	if (currentPosition.x < MIN_AIRSPACE_X_BOUND
			|| currentPosition.x > MAX_AIRSPACE_X_BOUND
			|| currentPosition.y < MIN_AIRSPACE_Y_BOUND
			|| currentPosition.y > MAX_AIRSPACE_Y_BOUND
			|| currentPosition.z < MIN_AIRSPACE_Z_BOUND
			|| currentPosition.z > MAX_AIRSPACE_Z_BOUND) {
		left = true;
		currentPosition = { -1, -1, -1 };
	}
}

void* Plane::start(void *context) {
	auto p = (Plane*) context;
	p->run();
	return NULL;
}
