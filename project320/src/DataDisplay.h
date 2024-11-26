#ifndef SRC_DATADISPLAY_H_
#define SRC_DATADISPLAY_H_

#include <string>
#include "Plane.h"

//for one aircraft
typedef struct {
	int aircraftID;
	Vec3 position; //can read flight level / altitude from z component of position vector
	Vec3 velocity;
} oneAircraftDisplay;

//for displaying multiple aircrafts
typedef struct {
	size_t numberOfAircrafts;
	Vec3 *positionArray;
	Vec3 *velocityArray;
	int *planeIDArray;
} multipleAircraftDisplay;

union commandBodyUnion {
	oneAircraftDisplay one;
	multipleAircraftDisplay multiple;
};

typedef struct {
	int commandType; //defining which of union element it is
	commandBodyUnion commandBody;

} dataDisplayCommandMessage;

class DataDisplay {
private:
	void run();
	void receiveMessage();
	std::string generateGrid(multipleAircraftDisplay &airspaceInfo);

	int chid;
	int fd;
public:
	DataDisplay();

	int getChid() const;
	static void* start(void *context);

};

#endif /* SRC_DATADISPLAY_H_ */
