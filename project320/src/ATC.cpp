#include <sys/dispatch.h>
#include <pthread.h>
#include <chrono>
#include <string>
#include <thread>
#include <fstream>
#include <sstream>
#include <iostream>
#include <time.h>

#include "ComputerSystem.h"
#include "Radar.h"
#include "Plane.h"
#include "OperatorConsole.h"
#include "DataDisplay.h"
#include "CommunicationSystem.h"
#include "InputStrings.h"
#include "constants.h"

using namespace std;

// create files on for each scenario given test inputs.
void writeFiles() {
	int fdlow = creat("/data/home/qnxuser/lowload.txt",	S_IRUSR | S_IWUSR | S_IXUSR);
	if (fdlow != -1) {
		write(fdlow, LOW_LOAD, strlen(LOW_LOAD));
		close(fdlow);
	}

	int fdmed = creat("/data/home/qnxuser/medload.txt",	S_IRUSR | S_IWUSR | S_IXUSR);
	if (fdmed != -1) {
		write(fdmed, MED_LOAD, strlen(MED_LOAD));
		close(fdmed);
	}

	int fdhigh = creat("/data/home/qnxuser/highload.txt",	S_IRUSR | S_IWUSR | S_IXUSR);
	if (fdhigh != -1) {
		write(fdhigh, HIGH_LOAD, strlen(HIGH_LOAD));
		close(fdhigh);
	}

	int fdoverload = creat("/data/home/qnxuser/overloadload.txt",	S_IRUSR | S_IWUSR | S_IXUSR);
	if (fdoverload != -1) {
		write(fdoverload, OVERLOAD_LOAD, strlen(OVERLOAD_LOAD));
		close(fdoverload);
	}
	int fdnull = creat("/data/home/qnxuser/nullload.txt",	S_IRUSR | S_IWUSR | S_IXUSR);
		if (fdnull != -1)
		 {/*
			write(fdnull, OVERLOAD_LOAD, strlen(OVERLOAD_LOAD));
			close(fdnull);*/
		}
}

void writeManualFiles() {
	int fdmanual = creat("/data/home/qnxuser/manualload.txt",	S_IRUSR | S_IWUSR | S_IXUSR);
	if (fdmanual != -1) {

		int time1,time2,id1,id2,px1,px2,py1,py2,pz1,pz2,vx1,vx2,vy1,vy2,vz1,vz2;

		cout<<"set the time,id,position in x,y,z, velocity x,y,z of the first plane";
		cin>> time1>>id1>>px1>>py1>>pz1>>vx1>>vy1>>vz1;

		cout<<"set the time,id,position in x,y,z, velocity x,y,z of the second plane";
		cin>> time2>>id2>>px2>>py2>>pz2>>vx2>>vy2>>vz2;

		ofstream MyFile("manualload.txt");
		MyFile<<time1<<id1<<px1<<py1<<pz1<<vx1<<vy1<<vz1;
		MyFile<<time2<<id2<<px2<<py2<<pz2<<vx2<<vy2<<vz2;
		MyFile.close();
	}

}



// Read start parameters for each plane from a given file.

vector<PlaneStartParams> readFile(string filePath) {
	vector<PlaneStartParams> planes;
	ifstream input(filePath);
	if (input)
	{
		string line;
		while (getline(input, line))
		{
			stringstream ss(line);
			int time, id, px, py, pz, vx, vy, vz;
			ss >> time >> id >> px >> py >> pz >> vx >> vy >> vz;
			PlaneStartParams p;
			p.id = id;
			p.arrivalTime = time;
			p.initialPosition = {px, py, pz};
			p.initialVelocity = {vx, vy, vz};
			planes.push_back(p);
		}
	}
	else
	{
		cout << "Could not open input file" << endl;
	}
	return planes;
}

void computerSystemDemo()
{
	string choice = "";
	while (choice != "low" && choice != "medium" && choice != "high" && choice != "overload" && choice != "manual" && choice != "null")
	{
		cout << "Enter the congestion level: [low,medium,high,overload,manual]: ";
		cin >> choice;
	}

	string filePath;
	if (choice == "low")
	{
		filePath = "/data/home/qnxuser/lowload.txt";
	}
	else if (choice == "medium")
	{
		filePath = "/data/home/qnxuser/medload.txt";
	}
	else if (choice == "high")
	{
		filePath = "/data/home/qnxuser/highload.txt";
	}
	else if (choice == "overload")
	{
			filePath = "/data/home/qnxuser/overloadload.txt";
	}
	else if (choice == "manual")
	{
			filePath = "/data/home/qnxuser/manualload.txt";
	}
	else if (choice == "null")
	{
			filePath = "/data/home/qnxuser/nulload.txt";
	}

	// initialize the planes
	vector<PlaneStartParams> params = readFile(filePath);
	vector<Plane> planes;
	for (size_t i = 0; i < params.size(); i++)
	{
		planes.push_back(Plane(params[i]));
	}

	pthread_t compSystemTid, opConsoleTid, displayTid;

	int numOfPlanes = planes.size();
	pthread_t planeThreads[numOfPlanes];

	// create threads for planes
	for (size_t i = 0; i < planes.size(); i++) {
		pthread_create(&planeThreads[i], NULL, &Plane::start, &planes[i]);
	}

	// Initialize components of the system.
	ComputerSystem compSystem;
	OperatorConsole opConsole;
	DataDisplay display;
	CommunicationSystem commSystem = CommunicationSystem(planes);
	Radar radar = Radar(planes);
	compSystem.setRadar(radar);
	compSystem.setCommSystem(commSystem);
	compSystem.setCongestionDegreeSeconds(15);


	// start the operator console and display threads, and wait for them to open up their message passing channels.
	pthread_create(&opConsoleTid, NULL, &OperatorConsole::start, &opConsole);
	pthread_create(&displayTid, NULL, &DataDisplay::start, &display);
	while (opConsole.getChid() == -1 || display.getChid() == -1)
		;

	// give the computer system the necessary channel IDs for IPC then start its thread.
	compSystem.setDisplayChid(display.getChid());
	compSystem.setOperatorChid(opConsole.getChid());
	pthread_create(&compSystemTid, NULL, &ComputerSystem::start, &compSystem);


	this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));

	int compSystemCoid = 0;
	if ((compSystemCoid = ConnectAttach(0, 0, compSystem.getChid(), 0, 0))
			== -1) {
		cout << "ComputerSystem: failed to attach to. Exiting thread.";
		return;
	}

	// run the simulation for 180 seconds.
	this_thread::sleep_for(std::chrono::seconds(TIME_RANGE_SECONDS));

	// terminate the computer system.
	ComputerSystemMessage msg;
	msg.command = COMMAND_EXIT_THREAD;
	if (MsgSend(compSystemCoid, &msg, sizeof(msg), NULL, 0) == 0)
	{
		cout << "Shut down compSystem" << endl;
	}
	else
	{
		cout << "Unable to shut down compSystem." << endl;
	}
	ConnectDetach(compSystemCoid);
	pthread_join(compSystemTid, NULL);

	// Terminate the data display.
	dataDisplayCommandMessage ddMsg;
	ddMsg.commandType = COMMAND_EXIT_THREAD;
	int ddCoid = ConnectAttach(0, 0, display.getChid(), _NTO_SIDE_CHANNEL, 0);
	MsgSend(ddCoid, &ddMsg, sizeof(ddMsg), NULL, 0);
	ConnectDetach(ddCoid);
	pthread_join(displayTid, NULL);

	// Terminate the operator console.
	OperatorConsoleCommandMessage ocMsg;
	ocMsg.systemCommandType = COMMAND_EXIT_THREAD;
	int ocCoid = ConnectAttach(0, 0, opConsole.getChid(), _NTO_SIDE_CHANNEL, 0);
	MsgSend(ocCoid, &ocMsg, sizeof(ocMsg), NULL, 0);
	ConnectDetach(ocCoid);
	pthread_join(opConsoleTid, NULL);

	// Terminate each plane.
	for (size_t i = 0; i < planes.size(); i++) {
		PlaneCommandMessage exitMsg;
		exitMsg.command = COMMAND_EXIT_THREAD;
		int planeCoid = ConnectAttach(0, 0, planes[i].getChid(),_NTO_SIDE_CHANNEL, 0);
		MsgSend(planeCoid, &exitMsg, sizeof(exitMsg), NULL, 0);
		ConnectDetach(planeCoid);
		pthread_join(planeThreads[i], NULL);
	}
}

int main() {
	std::string choice = "";
	while (choice != "write" && choice != "run" && choice != "manual" && choice != "scenario") {
		cout<< "Enter 'write' to create the input files in the QNX VM. Enter 'run' to run the ATC simulation. " << endl;
		cin >> choice;
	}
	if (choice == "write") {
		cout<< "Enter 'manual' to setup specific inputs in the QNX VM. Enter 'scenario' write low/med/high options to the QNX VM. " << endl;
		cin >> choice;
		if (choice == "scenario"){
			writeFiles();
		}
		else if (choice == "manual")
		{
			writeManualFiles();
		}
	} else if (choice == "run") {
		computerSystemDemo();
	}
	return 0;
}
