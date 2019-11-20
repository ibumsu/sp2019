//==============================================================================
// chatclient.c
// a client that uses select() to enable asynchronous user input            
//==============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXDATASIZE 1000

int main(int argc, char *argv[]) {

	int sockfd;
	int numbytes;
	char buf[MAXDATASIZE];
	char tmpbuf[MAXDATASIZE];
	struct addrinfo hints, *servinfo;
	int rv;
	char s[INET_ADDRSTRLEN];
	int quit;

	fd_set master, read_fds;

	// If number of command-line arguments is not 2, print usage and exit.
	if(argc != 3) {
		fprintf(stderr, "usage: %s hostname portnum\n", argv[0]);
		exit(1);
	}

	// Prepare the data structure "servinfo".
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

	// Create a socket using "servinfo".
    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
            servinfo->ai_protocol)) == -1) {
        perror("client: socket");
        return 2;
    }

	// Connect to the server.
    if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(sockfd);
        perror("client: connect");
        return 2;
    }

	// Print debug message.
    inet_ntop(servinfo->ai_family, &((struct sockaddr_in*)servinfo->ai_addr)->sin_addr, s, sizeof s);
    printf("client: connecting to %s\n", s);

	// The "servinfo" data structure is no longer needed. Free the data.
    freeaddrinfo(servinfo); 

	// Initialize fd_set structure. Only stdin (0) and the socket connected to the server is active.
	FD_ZERO(&master);
	FD_SET(0, &master);			
	FD_SET(sockfd, &master);
	
	// If "quit" is 1, then we break out of the while loop. Initially "quit" is 0.
	quit = 0;
	
	// The main loop
	while(1) {
		
		// make a copy of "master" and use that, because the function select() will modify the data.
		read_fds = master;
		if(select(sockfd+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(1);
		}

		for(int i=0; i<=sockfd+1; i++) {
			if(FD_ISSET(i, &read_fds)) {
				if(i == 0) {
					// message entered by user (stdin)
					fgets(buf, sizeof(buf), stdin);		// Read the message from stdin.
					buf[strlen(buf)-1] = '\0';			// Need to attach a null character at the end.

					// If the user enters "/quit", set the "quit" flag.
					if(strcmp(buf, "/quit") == 0) {	
						quit = 1;
						break;
					}

					// Otherwise, send the message to the server.
					if(send(sockfd, buf, strlen(buf), 0) == -1) {
						perror("send");
						close(sockfd);
						exit(1);
					}
				}
				else if(i == sockfd) {
					// message received from server
					// Receive message from the socket.
					if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
						perror("recv");
						close(sockfd);
						exit(1);
					}

					// If numbytes is 0, it means the server is disconnected.
					if(numbytes == 0) {
						printf("server disconnected.\n");
						quit = 1;
						break;
					}

					// Attach the null character at the end and print message on the display.
					buf[numbytes] = '\0';
					printf("%s\n", buf);
				}
			}
		}
		
		if(quit) break;
	}

	printf("Connection closed.\n");
	close(sockfd);
						
	return 0;
}

