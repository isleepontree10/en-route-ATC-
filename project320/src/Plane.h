#ifndef SRC_PLANE_H_
#define SRC_PLANE_H_

#include <stdlib.h>
#include <sys/neutrino.h>
#include "commandCodes.h"
#include <string>
#include <iostream>
#include <cmath>

using namespace std;

using std::string;
// How often the plane should update its position.
#define POSITION_UPDATE_INTERVAL_SECONDS 1
// Used internally to identify when the position update timer has fired.
#define CODE_TIMER 1

typedef struct Vec3 {
	Vec3 sum(Vec3 b) {
		return {x+b.x, y+b.y, z+b.z};
	}
	Vec3 absoluteDiff(Vec3 b) {
		return {abs(x-b.x),abs(y-b.y),abs(z-b.z)};
	}
	string print() {
		return std::to_string(x) + "," + std::to_string(y) + ","
				+ std::to_string(z);
	}
	Vec3 scalarMultiplication(int scalarMultiplier) {
		return {x*scalarMultiplier, y*scalarMultiplier, z*scalarMultiplier};
	}
	bool operator==(const Vec3 &rhs) {
		return x == rhs.x && y == rhs.y && z == rhs.z;
	}
	bool operator!=(const Vec3 &rhs) {
		return x != rhs.x && y != rhs.y && z != rhs.z;
	}
	int x;
	int y;
	int z;
} Vec3;

// Operator overload allowing Vec3 to be printed to cout using << operator
ostream& operator<<(ostream &os, const Vec3 &vec);

typedef struct {
	int id;
	int arrivalTime;
	Vec3 initialPosition;
	Vec3 initialVelocity;
} PlaneStartParams;

typedef struct {
	struct _pulse header;
	int command;
	Vec3 newVelocity;
} PlaneCommandMessage;

typedef struct {
	Vec3 currentPosition;
	Vec3 currentVelocity;
} PlanePositionResponse;

class Plane {
public:
	Plane(PlaneStartParams &params);
	int getChid() const;
	int getPlaneId() const;

private:
	void run();
	void listen();
	void updatePosition();

	PlaneStartParams startParams;
	Vec3 currentPosition;
	Vec3 currentVelocity;
	bool arrived;
	bool left;
	int chid;

public:
	// Thread host function to initialize the plane. Use as target for pthread_create.
	static void* start(void *context);
};

#endif /* SRC_PLANE_H_ */
