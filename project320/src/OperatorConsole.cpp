#include "OperatorConsole.h"
#include <iostream>
#include <atomic>
#include <sstream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>

#include "commandCodes.h"

pthread_mutex_t OperatorConsole::mutex = PTHREAD_MUTEX_INITIALIZER;
std::queue<OperatorConsoleResponseMessage> OperatorConsole::responseQueue;

OperatorConsole::OperatorConsole() :
		chid(-1) {
}

int OperatorConsole::getChid() const {
	return chid;
}

void OperatorConsole::run() {
	// Create a communication channel
	if ((chid = ChannelCreate(0)) == -1) {
		std::cout
				<< "Operator console: channel creation failed. Exiting thread."
				<< std::endl;
		return;
	}

	// Start the console reader thread
	pthread_t cinReaderThread;
	std::atomic_bool cinReaderStopFlag;
	cinReaderStopFlag = false;
	pthread_create(&cinReaderThread, NULL, &OperatorConsole::cinRead,
			&cinReaderStopFlag);

	// Start listening for messages
	listen();

	// Stop the console reader thread
	cinReaderStopFlag = true;
	pthread_join(cinReaderThread, NULL);
}

void OperatorConsole::listen() {
	int rcvid;
	OperatorConsoleCommandMessage msg;
	while (1) {
		rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
		switch (msg.systemCommandType) {
		case OPCON_CONSOLE_COMMAND_GET_USER_COMMAND: {
			// When the computer system asks us, check if any commands are waiting and return the applicable response.
			pthread_mutex_lock(&mutex);
			if (responseQueue.empty()) {
				OperatorConsoleResponseMessage msg;
				msg.userCommandType = OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE;
				MsgReply(rcvid, EOK, &msg, sizeof(msg));
			} else {
				OperatorConsoleResponseMessage msg = responseQueue.front();
				responseQueue.pop();
				MsgReply(rcvid, EOK, &msg, sizeof(msg));
			}
			pthread_mutex_unlock(&mutex);
			break;
		}
		case OPCON_CONSOLE_COMMAND_ALERT: {
			// Print an alert about an imminent constraint violation to the operator console.
			cout << "OpConsole: " << "Planes " << msg.plane1 << " and "
					<< msg.plane2 << " will collide in "
					<< msg.collisionTimeSeconds << " seconds" << endl;
			OperatorConsoleResponseMessage msg;
			msg.userCommandType = OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE;
			MsgReply(rcvid, EOK, &msg, sizeof(msg));
			break;
		}
		case COMMAND_EXIT_THREAD:
			// Required to allow all threads to gracefully terminate when the program is terminating
			MsgReply(rcvid, EOK, NULL, 0);
			return;
		default:
			std::cout << "OpConsole: " << "received unknown command "
					<< msg.systemCommandType << std::endl;
			MsgError(rcvid, ENOSYS);
			break;
		}
	}
}

void* OperatorConsole::start(void *context) {
	auto c = (OperatorConsole*) context;
	c->run();
	return NULL;
}

void* OperatorConsole::cinRead(void *param) {
	// Get the flag we monitor to know when to stop reading
	std::atomic_bool *stop = (std::atomic_bool*) param;

	int fd = creat("/data/home/qnxuser/commandlog.txt",
	S_IRUSR | S_IWUSR | S_IXUSR);
	if (fd == -1) {
		std::cout << "OpConsole: " << "Failed to open logfile. Errno is "
				<< errno << std::endl;
	}

	std::string msg;
	while (!(*stop)) {
		// Get a command from cin and break it up by spaces
		std::getline(std::cin, msg);
		std::vector<std::string> tokens;
		tokenize(tokens, msg);

		if (tokens.size() == 0)
			continue;

		if (fd != -1) {
			// Create a char buffer with length equal to the message + 1 for the null terminator.
			char *buffer = new char[msg.length() + 1];
			// Copy the C++ string into the char buffer.
			strncpy(buffer, msg.c_str(), msg.length() + 1);
			// Write the command to the file with a newline.
			write(fd, buffer, msg.length() + 1);
			write(fd, "\n", 1);
			// Delete the buffer.
			delete[] buffer;
		}

		if (tokens[0] == OPCON_COMMAND_STRING_SHOW_PLANE) {
			if (tokens.size() < 2) {
				std::cout << "OpConsole: " << "Error: must provide plane number"
						<< std::endl;
				continue;
			}
			try {
				// Parse the plane number and prepare a response in the queue.
				int planeNum = std::stoi(tokens[1]);

				OperatorConsoleResponseMessage res;
				res.userCommandType = OPCON_USER_COMMAND_DISPLAY_PLANE_INFO;
				res.planeNumber = planeNum;

				pthread_mutex_lock(&mutex);
				responseQueue.push(res);
				pthread_mutex_unlock(&mutex);
			} catch (std::invalid_argument &e) {
				std::cout << "OpConsole: " << "Error: not a valid integer"
						<< std::endl;
				continue;
			}
		} else if (tokens[0] == OPCON_COMMAND_STRING_SET_VELOCITY) {
			if (tokens.size() < 5) {
				std::cout
						<< "Error: must provide plane number and 3 velocity components (x,y,z)"
						<< std::endl;
				continue;
			}
			try {
				// Parse the plane number and velocity components and prepare a response.
				int planeNum = std::stoi(tokens[1]);
				int components[3];
				for (size_t i = 0; i < 3; i++) {
					components[i] = std::stoi(tokens[2 + i]);
				}
				Vec3 velocity { components[0], components[1], components[2] };

				OperatorConsoleResponseMessage res;
				res.userCommandType = OPCON_USER_COMMAND_SET_PLANE_VELOCITY;
				res.planeNumber = planeNum;
				res.newVelocity = velocity;

				pthread_mutex_lock(&mutex);
				responseQueue.push(res);
				pthread_mutex_unlock(&mutex);
			} catch (std::invalid_argument &e) {
				std::cout << "OpConsole: " << "Error: not a valid integer"
						<< std::endl;
				continue;
			}
		} else if (tokens[0] == OPCON_COMMAND_STRING_UPDATE_CONGESTION_VALUE) {
			if (tokens.size() < 2) {
				std::cout << "OpConsole: "
						<< "Error: must provide a valid integer" << std::endl;
				continue;
			}
			try {
				// Parse the new congestion degree and prepare a response.
				int congestionDegree = std::stoi(tokens[1]);
				if (congestionDegree < 1) {
					std::cout << "OpConsole: "
							<< "Congestion value must be larger than 1" << endl;
					continue;
				}

				OperatorConsoleResponseMessage res;
				res.userCommandType =
				OPCON_USER_COMMAND_UPDATE_CONGESTION_VALUE;
				res.newCongestionValue = congestionDegree;

				pthread_mutex_lock(&mutex);
				responseQueue.push(res);
				pthread_mutex_unlock(&mutex);
			} catch (std::invalid_argument &e) {
				std::cout << "OpConsole: " << "Error: not a valid integer"
						<< std::endl;
				continue;
			}
		} else {
			std::cout << "OpConsole: " << "Unknown command" << std::endl;
			continue;
		}
	}

	if (fd != -1) {
		close(fd);
	}
	return NULL;
}

// Breaks up a string by spaces
void OperatorConsole::tokenize(std::vector<std::string> &dest,
		std::string &str) {
	std::stringstream ss(str);
	std::string token;
	while (std::getline(ss, token, ' ')) {
		dest.push_back(token);
	}
}
