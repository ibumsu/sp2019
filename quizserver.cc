//==============================================================================
// quizserver.cc: includes 'quiz' feature
//==============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>

#define MAXDATASIZE 1000
#define BACKLOG		10
#define MAXUSERS	20
#define MAXIDSIZE	40

#define NUMQUESTIONS 5
char questions[20][200] = {
	"What is the capital of Korea?",
	"What is the capital of Mexico?",
	"What is the capital of Argentina?",
	"What is the capital of Australia?",
	"What is the capital of Canada?",
};
char answers[20][200] = {
	"Seoul",
	"Mexico City",
	"Buenos Aires",
	"Canberra",
	"Ottawa",
};

// global variables
char userid[MAXUSERS][MAXIDSIZE];
int temp_usernum;

// the main function
int main(int argc, char *argv[]) {

	int sockfd, newfd;
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET_ADDRSTRLEN];
    int yes = 1;
    int rv;

    // for select()
    fd_set master;
    fd_set read_fds;
    int fdmax;

    // for message handling
    char buf[MAXDATASIZE], tmpbuf[MAXDATASIZE], msg[MAXDATASIZE];
    int numbytes;

    // for user information
    temp_usernum = 1;

	// quiz mode
	int quizmode = 0;
	int qnum = 0;

	// if number of command-line arguments is not 2, show usage and abort.
	if(argc != 2) {
		printf("usage: server portnum\n");
		exit(1);
	}

	// set random number generator seed randomly (using current time)
	srand(time(NULL));

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    // use my IP

	// Prepare "servinfo" data structure.
    if((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

	// Create the "receptionist" socket.
    if((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
        perror("server: socket");
        exit(1);
    }

    // Allow immediate reuse of ports.
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	// Bind the socket with the addresse in "servinfo".
    if(bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(sockfd);
        perror("server: bind");
        exit(1);
    }

	// "servinfo" is no longer needed.
    freeaddrinfo(servinfo);

	// Begin listening on the port.
	if(listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

	// Prepare data structure for using select(). Initially, only the receptionist socket are active.
	FD_ZERO(&master);
    FD_SET(sockfd, &master);
    fdmax = sockfd;

	// the main loop
	while(1) {
		read_fds = master;
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(1);
        }

		for(int i=0; i<=fdmax; i++) {
			if(FD_ISSET(i, &read_fds)) {
				if(i == sockfd) {
					// A new user is trying to connect to the server.
					sin_size = sizeof their_addr;
					newfd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);

                    if(newfd == -1) {
                        perror("accept");
                    } else {

						// Make the new socket file descriptor active in the "master" set.
                        FD_SET(newfd, &master);
                        if(newfd > fdmax) {
                            fdmax = newfd;	// Update "fdmax" if necessary.
                        }
                        printf("chatserver: new connection from %s on socket %d\n",
                            inet_ntop(their_addr.ss_family, &((struct sockaddr_in*)&their_addr)->sin_addr, s, sizeof s), newfd);
                    }

					// Assign the new user a temporary ID.
					sprintf(userid[newfd], "user%02d", temp_usernum);
					temp_usernum++;

					// Notify the user his/her ID.
					sprintf(buf, "Welcome! your ID is %s", userid[newfd]);
					if(send(newfd, buf, strlen(buf), 0) == -1) {
						perror("send");
					}

					// Tell other clients that the new user has joined.
					sprintf(buf, "New user %s has joined.", userid[newfd]);
					for(int j=3; j<=fdmax; j++) {
						if(j == sockfd) continue;				// skip the receptionist socket
						if(j == newfd) continue;				// skip the client who just joined
						if(!FD_ISSET(j, &master)) continue;		// skip the inactive sockets
						if(send(j, buf, strlen(buf), 0) == -1) perror("send");
					}
				}
				else {
					// A message has arrived from a connected user.
					if((numbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
						// If numbytes is less than or equal to 0, it means the client is disconnected.
						if(numbytes == 0) {
							printf("chatserver: socket %d hung up\n", i);
						} else {
							perror("recv");
						}
						close(i);
						FD_CLR(i, &master);

						// Tell other clients that a user has left.
						sprintf(buf, "%s has left.", userid[i]);
						for(int j=3; j<=fdmax; j++) {
                        	if(j == sockfd) continue;				// skip the receptionist socket
                        	if(j == i) continue;					// skip the client who sent the message
                        	if(!FD_ISSET(j, &master)) continue;		// skip the inactive sockets
                        	if(send(j, buf, strlen(buf), 0) == -1) perror("send");
                    	}
					}
					else {
						// A message has arrived.
						buf[numbytes] = '\0';
						strcpy(msg, buf);
						printf("server received \"%s\" from %s.\n", msg, userid[i]);

						if(strncasecmp(buf, "/id ", 4) == 0) {
							strcpy(tmpbuf, userid[i]);		// save the old ID in "tmpbuf"
							strcpy(userid[i], buf+4);		// update the user ID.
							// Send a message to client i.
							sprintf(buf, "Changed ID to %s", userid[i]);	
							if(send(i, buf, strlen(buf), 0) == -1) perror("send");
							// Send a message to other clients.
							sprintf(buf, "%s changed ID to %s", tmpbuf, userid[i]);	
							for(int j=3; j<=fdmax; j++) {
                            	if(j == sockfd) continue;
                            	if(j == i) continue;
                            	if(!FD_ISSET(j, &master)) continue;
                            	if(send(j, buf, strlen(buf), 0) == -1) perror("send");
                        	}
						}
						else if(strncasecmp(buf, "/user", 5) == 0) {
							// Prepare a message to send to client i.
							strcpy(buf, "");
							for(int j=3; j<=fdmax; j++) {
								if(j == sockfd) continue;
								if(!FD_ISSET(j, &master)) continue;
								strcat(buf, userid[j]);
								strcat(buf, "\n");
							}
							// Send the message to client i.
							if(send(i, buf, strlen(buf), 0) == -1) perror("send");
						}
						else if(strncasecmp(buf, "/dice", 5) == 0) {
							int rn;
							char tmpstr[80];
							sprintf(buf, "%s has rolled the dice.\n", userid[i]);
							for(int j=3; j<=fdmax; j++) {
								if(j == sockfd) continue;
								if(!FD_ISSET(j, &master)) continue;
								rn = rand() % 6 + 1;	// get a random number from 1 to 6.
								sprintf(tmpstr, "%s -- %d\n", userid[j], rn);
								strcat(buf, tmpstr);
							}
							// Send the message to all clients.
							for(int j=3; j<=fdmax; j++) {
								if(j == sockfd) continue;
								if(!FD_ISSET(j, &master)) continue;
								if(send(j, buf, strlen(buf), 0) == -1) perror("send");		
							}
						}
						else if(strncasecmp(buf, "/quiz", 5) == 0) {
							strcpy(tmpbuf, buf);
							sprintf(buf, "%s: %s", userid[i], tmpbuf);

							// Send the message to all other clients.
							for(int j=3; j<=fdmax; j++) {
                                if(j == sockfd) continue;
                                if(j == i) continue;
                                if(!FD_ISSET(j, &master)) continue;
                                if(send(j, buf, strlen(buf), 0) == -1) perror("send");
                            }

							// If we are not already in the quiz mode, enter quiz mode and post a question.
							if(quizmode == 0) {
								quizmode = 1;

								// Select a question
								qnum = rand() % NUMQUESTIONS;
								strcpy(buf, questions[qnum]);
	
								// Send this question to all clients.
								for(int j=3; j<=fdmax; j++) {
	                                if(j == sockfd) continue;
	                                if(!FD_ISSET(j, &master)) continue;
									if(j != i) {
										if(send(j, "\n", 1, 0) == -1) perror("send");
									}
	                                if(send(j, buf, strlen(buf), 0) == -1) perror("send");
	                            }
							}
						}
						else {
							strcpy(tmpbuf, buf);
							sprintf(buf, "%s: %s", userid[i], tmpbuf);
							// Send the message to all other clients.
							for(int j=3; j<=fdmax; j++) {
                                if(j == sockfd) continue;
                                if(j == i) continue;
                                if(!FD_ISSET(j, &master)) continue;
                                if(send(j, buf, strlen(buf), 0) == -1) perror("send");
                            }

							if(quizmode == 1) {
								if(strcasecmp(msg, answers[qnum]) == 0) {
									sprintf(buf, "Correct! %s is the winner!", userid[i]);
									quizmode = 0;
								}	
								else {
									sprintf(buf, "Try again!");
								}

								// Send the message to all clients.
                                for(int j=3; j<=fdmax; j++) {
                                    if(j == sockfd) continue;
                                    if(!FD_ISSET(j, &master)) continue;
									if(j != i) {
										if(send(j, "\n", 1, 0) == -1) perror("send");
									}
                                    if(send(j, buf, strlen(buf), 0) == -1) perror("send");
                                }
							}
						}
					}
				}
			}
		}
	}
	
	return 0;
}

