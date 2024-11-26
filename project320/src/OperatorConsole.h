#ifndef SRC_OPERATORCONSOLE_H_
#define SRC_OPERATORCONSOLE_H_

#include <stdlib.h>
#include <sys/neutrino.h>
#include <queue>
#include <pthread.h>
#include <vector>
#include <string>
#include "Plane.h"

#define OPCON_COMMAND_STRING_SHOW_PLANE "showplane"
#define OPCON_COMMAND_STRING_SET_VELOCITY "setvelocity"
#define OPCON_COMMAND_STRING_UPDATE_CONGESTION_VALUE "setcongestion"

typedef struct {
	int systemCommandType;
	int plane1;
	int plane2;
	int collisionTimeSeconds;
} OperatorConsoleCommandMessage;

typedef struct {
	int userCommandType;
	int planeNumber;
	int newCongestionValue;
	Vec3 newVelocity;
} OperatorConsoleResponseMessage;

class OperatorConsole {
public:
	OperatorConsole();
	int getChid() const;

private:
	void run();
	void listen();

	int chid;

public:
	static void* start(void *context);
private:
	static pthread_mutex_t mutex;
	static std::queue<OperatorConsoleResponseMessage> responseQueue;
	static void* cinRead(void *param);
	static void tokenize(std::vector<std::string> &dest, std::string &str);
};

#endif /* SRC_OPERATORCONSOLE_H_ */
