#ifndef SRC_COMMUNICATIONSYSTEM_H_
#define SRC_COMMUNICATIONSYSTEM_H_

#include "Plane.h"
#include <vector>

class CommunicationSystem {
public:
	CommunicationSystem(std::vector<Plane> &planes);
	CommunicationSystem();
	bool send(Plane &R, Vec3 &newVelocity); //returns true if sending message is successful and false if not
	bool send(int planeNumber, Vec3 &newVelocity);

private:
	std::vector<Plane> planes;
};

#endif /* SRC_COMMUNICATIONSYSTEM_H_ */
