#include "CommunicationSystem.h"
#include "commandCodes.h"


CommunicationSystem::CommunicationSystem() {}
CommunicationSystem::CommunicationSystem(std::vector<Plane> &planes) :
		planes(planes) {}

// sets the velocity of the plane with a given ID
bool CommunicationSystem::send(int planeNumber, Vec3 &newVelocity)
{
	for (size_t i = 0; i < planes.size(); i++)
	{
		if (planes[i].getPlaneId() == planeNumber)
		{
			return send(planes[i], newVelocity);
		}
	}
	return false;
}

// sets the velocity of a specific plane object
bool CommunicationSystem::send(Plane &R, Vec3 &newVelocity)
{

	int planeChid = R.getChid();
	//send id
	int sndid;
	//client id
	int coid;

	if ((coid = ConnectAttach(0, 0, planeChid, 0, 0)) == -1)
	{
		std::cout << "client connection failed " << std::endl;
		return false;
	}

	PlaneCommandMessage msg;
	msg.command = COMMAND_SET_VELOCITY; //determining the type
	msg.newVelocity = newVelocity; //setting the new velocity

	sndid = MsgSend(coid, &msg, sizeof(msg), NULL, 0);
	ConnectDetach(coid);
	if (sndid == -1)
	{
		std::cout << "failed to send message" << std::endl;
		return false;
	}

	return true;

}
