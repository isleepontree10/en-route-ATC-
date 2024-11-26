#ifndef SRC_COMPUTER_SYSTEM_H
#define SRC_COMPUTER_SYSTEM_H

#include <sys/neutrino.h>
#include <iostream>
#include <map>
#include "Plane.h"
#include "Radar.h"
#include "CommunicationSystem.h"

using namespace std;

typedef struct {
	struct _pulse header;
	int command;
} ComputerSystemMessage;

typedef struct {
	int timerCode;
	int taskIntervalSeconds;
} periodicTask;

class ComputerSystem {
public:
	ComputerSystem();
	int getChid() const;
	void setOperatorChid(int id);
	void setRadar(Radar &radar);
	void setCommSystem(CommunicationSystem &commSystem);
	void setDisplayChid(int id);
	void setCongestionDegreeSeconds(int congestionDegreeSeconds);
private:
	void run();
	void listen();
	void createPeriodicTasks();
	void logSystem(bool toFile);
	void violationCheck();
	void opConCheck();
	void sendDisplayCommand(int planeNumber);
	void sendVelocityUpdateToComm(int planeNumber, Vec3 newVelocity);
	void checkForFutureViolation(std::pair<int, PlanePositionResponse> plane1,
			std::pair<int, PlanePositionResponse> plane2);
	int chid;
	int operatorChid;
	int displayChid;
	Radar radar;
	CommunicationSystem commSystem;
	Vec3 airspaceBounds;
	int congestionDegreeSeconds;
	std::vector<pair<int, PlanePositionResponse>> airspace;
public:
	static void* start(void *context);
};

#endif /* SRC_COMPUTER_PLANE_H */
