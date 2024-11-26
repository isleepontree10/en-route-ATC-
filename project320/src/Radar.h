#ifndef SRC_RADAR_H_
#define SRC_RADAR_H_

#include <vector>
#include <map>

#include "Plane.h"

class Radar {
public:
	Radar();
	Radar(std::vector<Plane> &planes);

	bool pingPlane(int planeNumber, PlanePositionResponse *out);
	std::vector<pair<int, PlanePositionResponse>> pingAirspace();

private:
	PlanePositionResponse pingPlane(Plane &p);
	std::vector<PlanePositionResponse> pingMultiplePlanes(
			std::vector<Plane> &planes);

	std::vector<Plane> planes;
};

#endif /* SRC_RADAR_H_ */

