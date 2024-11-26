#include "ComputerSystem.h"
#include <sys/siginfo.h>
#include <time.h>
#include <fstream>
#include "constants.h"
#include "inlineStaticHelpers.h"
#include "OperatorConsole.h"
#include "DataDisplay.h"
#include "commandCodes.h"

ComputerSystem::ComputerSystem() :
		chid(-1), operatorChid(-1), displayChid(-1), congestionDegreeSeconds(-1) {
}

int ComputerSystem::getChid() const {
	return chid;
}

void ComputerSystem::setOperatorChid(int id) {
	operatorChid = id;
}

void ComputerSystem::setRadar(Radar &radar) {
	this->radar = radar;
}

void ComputerSystem::setCommSystem(CommunicationSystem &commSystem) {
	this->commSystem = commSystem;
}

void ComputerSystem::setDisplayChid(int id) {
	displayChid = id;
}

void ComputerSystem::setCongestionDegreeSeconds(int congestionDegreeSeconds) {
	this->congestionDegreeSeconds = congestionDegreeSeconds;
}

void ComputerSystem::run() {
	createPeriodicTasks();
	listen();
}

void ComputerSystem::createPeriodicTasks() {
	/*
	  calculate airspace violation constraints every second
	  save the airspace state and operator requests to the logfile
	  codes corresponding to the index of the timer in the array are:

      AIRSPACE_VIOLATION_CONSTRAINT_TIMER 0
	  LOG_AIRSPACE_TO_CONSOLE_TIMER 1
	  OPCON_USER_ACTION_TIMER 2
	  LOG_AIRSPACE_TO_FILE_TIMER 3
	 */

	periodicTask periodicTasks[COMPUTER_SYSTEM_NUM_PERIODIC_TASKS] = { {
	AIRSPACE_VIOLATION_CONSTRAINT_TIMER, 1 },
			{ LOG_AIRSPACE_TO_CONSOLE_TIMER, 5 }, {
			OPERATOR_COMMAND_CHECK_TIMER, 1 },
			{ LOG_AIRSPACE_TO_FILE_TIMER, 30 } };

	// Create a new communication channel belonging to the plane and store the handle in chid.
	if ((chid = ChannelCreate(0)) == -1) {
		std::cout << "channel creation failed. Exiting thread." << std::endl;
		return;
	}

	// Open a client to our own connection to be used for timer pulses and store the handle in coid.
	int coid;
	if ((coid = ConnectAttach(0, 0, chid, 0, 0)) == -1) {
		std::cout
				<< "ComputerSystem: failed to attach to self. Exiting thread.";
		return;
	}

	// For each periodic task, initialize a timer with the associated code and interval.
	for (int i = 0; i < COMPUTER_SYSTEM_NUM_PERIODIC_TASKS; i++) {
		periodicTask pt = periodicTasks[i];
		struct sigevent sigev;
		timer_t timer;
		SIGEV_PULSE_INIT(&sigev, coid, SIGEV_PULSE_PRIO_INHERIT, pt.timerCode,
				0);
		if (timer_create(CLOCK_MONOTONIC, &sigev, &timer) == -1) {
			std::cout
					<< "ComputerSystem: failed to initialize timer. Exiting thread.";
			return;
		}

		struct itimerspec timerValue;
		timerValue.it_value.tv_sec = pt.taskIntervalSeconds;
		timerValue.it_value.tv_nsec = 0;
		timerValue.it_interval.tv_sec = pt.taskIntervalSeconds;
		timerValue.it_interval.tv_nsec = 0;

		// Start the timer.
		timer_settime(timer, 0, &timerValue, NULL);
	}

}

void ComputerSystem::listen() {
	int rcvid;
	ComputerSystemMessage msg;
	while (1) {
		// Wait for any type of message.
		rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
		if (rcvid == 0) {
			// Handle internal switches from the pulses of the various timers.
			switch (msg.header.code) {
			case LOG_AIRSPACE_TO_CONSOLE_TIMER:
				logSystem(false);
				break;
			case AIRSPACE_VIOLATION_CONSTRAINT_TIMER:
				violationCheck();
				break;
			case OPERATOR_COMMAND_CHECK_TIMER:
				opConCheck();
				break;
			case LOG_AIRSPACE_TO_FILE_TIMER:
				logSystem(true);
				break;
			default:
				std::cout
						<< "ComputerSystem: received pulse with unknown code: "
						<< msg.header.code << " and unknown command: "
						<< msg.command << std::endl;
				break;
			}
		} else {
			// handle messages from external processes
			switch (msg.command) {
			case COMMAND_OPERATOR_REQUEST:
				MsgSend(operatorChid, &msg, sizeof(msg), NULL, 0);
				break;
			case COMMAND_EXIT_THREAD:

				cout << "ComputerSystem: " << "Received EXIT command" << endl;
				MsgReply(rcvid, EOK, NULL, 0);
				return;
			default:
				std::cout << "ComputerSystem: " << "received unknown command "
						<< msg.command << std::endl;
				MsgError(rcvid, ENOSYS);
				break;
			}
		}
	}
}

// Creates a plan view of the system using the data display, instructing it to log either to the console or to a file.
void ComputerSystem::logSystem(bool toFile) {
	this->airspace = radar.pingAirspace();
	size_t aircraftCount = airspace.size();

	// Create three separate arrays as per the data display message format.
	int *idArray = new int[aircraftCount];
	Vec3 *positionArray = new Vec3[aircraftCount];
	Vec3 *velocityArray = new Vec3[aircraftCount];
	for (size_t i = 0; i < aircraftCount; i++) {
		auto &current = airspace[i];
		idArray[i] = current.first;
		positionArray[i] = current.second.currentPosition;
		velocityArray[i] = current.second.currentVelocity;
	}

	// Construct the message.
	dataDisplayCommandMessage msg;
	if (toFile) {
		msg.commandType = COMMAND_LOG;
	} else {
		msg.commandType = COMMAND_GRID;
	}
	msg.commandBody.multiple.numberOfAircrafts = aircraftCount;
	msg.commandBody.multiple.planeIDArray = idArray;
	msg.commandBody.multiple.positionArray = positionArray;
	msg.commandBody.multiple.velocityArray = velocityArray;

	// Send the message and then clean up the allocated memory.
	int coid = ConnectAttach(0, 0, displayChid, _NTO_SIDE_CHANNEL, 0);
	if (MsgSend(coid, &msg, sizeof(msg), NULL, 0) == -1) {
		cout << "ComputerSystem: " << "Couldn't send command to the display.";
		exit(-1);
	}
	ConnectDetach(coid);
	delete[] idArray;
	delete[] positionArray;
	delete[] velocityArray;
}

// Poll the operator console for a new command from the human operator.
void ComputerSystem::opConCheck() {
	int coid = ConnectAttach(0, 0, operatorChid, _NTO_SIDE_CHANNEL, 0);
	OperatorConsoleCommandMessage sendMsg;
	OperatorConsoleResponseMessage rcvMsg;
	sendMsg.systemCommandType = OPCON_CONSOLE_COMMAND_GET_USER_COMMAND;
	if (MsgSend(coid, &sendMsg, sizeof(sendMsg), &rcvMsg, sizeof(rcvMsg))
			== -1) {
		cout << "ComputerSystem: "
				<< "Couldn't get user request queue from operator console";
		exit(-1);
	}
	ConnectDetach(coid);
	switch (rcvMsg.userCommandType) {
	case OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE:
		break;
	case OPCON_USER_COMMAND_DISPLAY_PLANE_INFO:
		sendDisplayCommand(rcvMsg.planeNumber); // open disp channel and send msg
		break;
	case OPCON_USER_COMMAND_UPDATE_CONGESTION_VALUE:
		this->congestionDegreeSeconds = rcvMsg.newCongestionValue;
		cout << "ComputerSystem: " << "New congestion value set to "
				<< congestionDegreeSeconds << endl;
		break;
	case OPCON_USER_COMMAND_SET_PLANE_VELOCITY:
		sendVelocityUpdateToComm(rcvMsg.planeNumber, rcvMsg.newVelocity); // open comm channel and send msg
		break;
	}
}

// Instructs the data display to show a single plane's current parameters.
void ComputerSystem::sendDisplayCommand(int planeNumber) {
	// Request radar for an update position on the plane
	// Open connection to display and send display message
	PlanePositionResponse out;
	if (radar.pingPlane(planeNumber, &out)) {
		int coid = ConnectAttach(0, 0, displayChid, _NTO_SIDE_CHANNEL, 0);
		dataDisplayCommandMessage sendMsg;
		sendMsg.commandType = COMMAND_ONE_PLANE;
		sendMsg.commandBody.one.aircraftID = planeNumber;
		sendMsg.commandBody.one.position = out.currentPosition;
		sendMsg.commandBody.one.velocity = out.currentVelocity;
		if (MsgSend(coid, &sendMsg, sizeof(sendMsg), NULL, 0) == -1) {
			cout << "ComputerSystem: "
					<< "Couldn't send command to the display.";
			exit(-1);
		}
		ConnectDetach(coid);
	} else {
		cout
				<< "The plane requested to update the position is not found in the airspace"
				<< endl;
	}
}

// Updates a plane's velocity via the communication system.
void ComputerSystem::sendVelocityUpdateToComm(int planeNumber,
		Vec3 newVelocity) {
	// Request radar for the plane for sanity purposes
	// Open connection to comm and send update message
	PlanePositionResponse out;
	if (radar.pingPlane(planeNumber, &out)) {
		if (commSystem.send(planeNumber, newVelocity)) {
			cout << "ComputerSystem: " << "Plane " << planeNumber
					<< " velocity updated." << endl;
		} else {
			cout << "ComputerSystem: couldn't update velocity for plane "
					<< planeNumber << endl;
		}
	} else {
		cout
				<< "The plane requested to update the velocity is not found in the airspace"
				<< endl;
	}

}

// Checks each pair of planes in the airspace for constraint violations.
void ComputerSystem::violationCheck() {
	this->airspace = radar.pingAirspace();
	//Perform sequential validation, in case of a collision send out an alert to the operator and an update to the display
	for (size_t i = 0; i < airspace.size(); i++) {
		for (size_t j = i + 1; j < airspace.size(); j++) {
			checkForFutureViolation(airspace[i], airspace[j]);
		}
	}
}

// Checks whether a pair of planes will violate separation constraints at some interval in the future.
void ComputerSystem::checkForFutureViolation(
		std::pair<int, PlanePositionResponse> plane1,
		std::pair<int, PlanePositionResponse> plane2) {
	// Based on the doc:
	// for all aircrafts in space, aircrafts must have a distance no less than
	// 1000 units in height, 3000 units in width/length
	// verify at time T + congestionDegreeSeconds for potential collision range
	int VERTICAL_LIMIT = 1000;
	int HORIZONTAL_LIMIT = 3000;
	Vec3 plane1projection = plane1.second.currentPosition.sum(
			plane1.second.currentVelocity.scalarMultiplication(
					congestionDegreeSeconds));
	Vec3 plane2projection = plane2.second.currentPosition.sum(
			plane2.second.currentVelocity.scalarMultiplication(
					congestionDegreeSeconds));
	// 3D overlap algorithm based on https://stackoverflow.com/a/20925869
	int x1min = plane1projection.x - HORIZONTAL_LIMIT;
	int x1max = plane1projection.x + HORIZONTAL_LIMIT;
	int x2min = plane2projection.x - HORIZONTAL_LIMIT;
	int x2max = plane2projection.x + HORIZONTAL_LIMIT;
	int y1min = plane1projection.y - HORIZONTAL_LIMIT;
	int y1max = plane1projection.y + HORIZONTAL_LIMIT;
	int y2min = plane2projection.y - HORIZONTAL_LIMIT;
	int y2max = plane2projection.y + HORIZONTAL_LIMIT;
	int z1min = plane1projection.z - VERTICAL_LIMIT;
	int z1max = plane1projection.z + VERTICAL_LIMIT;
	int z2min = plane2projection.z - VERTICAL_LIMIT;
	int z2max = plane2projection.z + VERTICAL_LIMIT;
	if ((x1max >= x2min && x2max >= x1min) && (y1max >= y2min && y2max >= y1min)
			&& (z1max >= z2min && z2max >= z1min)) {
		int coid = ConnectAttach(0, 0, operatorChid, _NTO_SIDE_CHANNEL, 0);
		OperatorConsoleCommandMessage sendMsg;
		sendMsg.plane1 = plane1.first;
		sendMsg.plane2 = plane2.first;
		sendMsg.collisionTimeSeconds = congestionDegreeSeconds;
		OperatorConsoleResponseMessage rcvMsg;
		sendMsg.systemCommandType = OPCON_CONSOLE_COMMAND_ALERT;
		if (MsgSend(coid, &sendMsg, sizeof(sendMsg), &rcvMsg, sizeof(rcvMsg))
				== -1) {
			cout << "ComputerSystem: "
					<< "Couldn't get user request queue from operator console";
			exit(-1);
		}
		ConnectDetach(coid);
		switch (rcvMsg.userCommandType) {
		case OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE:
			break;
		}
		return;
	}
}

void* ComputerSystem::start(void *context) {
	auto cs = (ComputerSystem*) context;
	cs->run();
	return NULL;
}
