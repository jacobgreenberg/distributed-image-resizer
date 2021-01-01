#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BACKLOG 10     // how many pending connections queue will hold


int prepare_server_socket(char* PORT){
	char* server_port = PORT;
    int sockfd, rc;
    int yes=1;
    struct addrinfo hints, *server;
    char message[256];

 	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; /* or SOCK_DGRAM */
    hints.ai_flags = AI_PASSIVE;

    /* getaddrinfo() gives us back a server address we can connect to.
       The first parameter is NULL since we want an address on this host.
       It actually gives us a linked list of addresses, but we'll just use the first.
     */
    if ((rc = getaddrinfo(NULL, server_port, &hints, &server)) != 0) {
		perror(gai_strerror(rc));
		exit(-1);
    }

    /* Now we can create the socket and bind it to the local IP and port */
    sockfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sockfd == -1) {
		perror("ERROR opening socket");
		exit(-1);
    }

    /* Get rid of "Address already in use" error messages */
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(-1);
	}
	rc = bind(sockfd, server->ai_addr, server->ai_addrlen);
	if (rc == -1) {
		perror("ERROR on connect");
		close(sockfd);
		exit(-1);
	}

	/* Begin listening for clients.*/
    listen(sockfd, BACKLOG);


	return sockfd;
}

/* Wait for a new client to connect. Returns the client socket. */
int accept_client(int server_socket) {
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    int client_socket;
    int client_response = -1;
    int rc;

    addr_size = sizeof client_addr;
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
    return client_socket;
}

/* Create socket and connect to a server. Return socket file descriptor on success */
int connect_to_server(char* ip, char* port) {
    int socketfd, rc;
    struct addrinfo hints, *server;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /* getaddrinfo() gives us back a server address we can connect to.
        It actually gives us a linked list of addresses, but we'll just use the first.
    */
    if ((rc = getaddrinfo(ip, port, &hints, &server)) != 0) {
        perror(gai_strerror(rc));
        exit(-1);
    }
    /* Now we can create the socket and connect */
    socketfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (socketfd == -1) {
        perror("ERROR opening socket");
        exit(-1);
    }
    rc = connect(socketfd, server->ai_addr, server->ai_addrlen);
    if (rc == -1) {
        perror("ERROR on connect");
        close(socketfd);
        exit(-1);
    }
    return socketfd;
}


/* Helpers based on 
https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c */

/* Send an integer */
int send_int(int fd, int num)
{
    int32_t conv = htonl(num);
    char *data = (char*)&conv;
    int left = sizeof(conv);
    int rc;
    do {
        rc = write(fd, data, left);
        if (rc < 0) {
            return -1;
        }
        else {
            data += rc;
            left -= rc;
        }
    }
    while (left > 0);
    return 0;
}

/* Receive an integer. This thread will block until the number is received! */
int receive_int(int fd, int *num)
{
    int32_t ret;
    char *data = (char*)&ret;
    int left = sizeof(ret);
    int rc;
    do {
        rc = read(fd, data, left);
        if (rc <= 0) { 
            return -1;
        }
        else {
            data += rc;
            left -= rc;
        }
    }
    while (left > 0);
    *num = ntohl(ret);
    return 0;
}


/* Sends the size of a string, then the string.
   Returns 0 on success or -1 on failure. */
int send_string(int fd, char* str) {
	int rc;
	rc = send_int(fd, strlen(str)+1);
	assert(!rc);
	char *data = str;
    int left = strlen(data)+1;
    do {
        rc = write(fd, data, left);
        if (rc < 0) {
            return -1;
        }
        else {
            data += rc;
            left -= rc;
        }
    }
    while (left > 0);
    return 0;
}

/* Receives an incoming string sent with send_string. This thread will block 
   until the string is received! Assumes that str is preallocated with max_size space.
   Returns an error if the size of incoming string is bigger than max_size. */
int receive_string(int fd, char* str, int max_size){
	int rc;
	int size;
	rc = receive_int(fd, &size);
	assert(!rc);
	if(size > max_size) {
		return -1;
	}
    char *data = str;
    int left = size;
    do {
        rc = read(fd, data, left);
        if (rc <= 0) { 
            return -1;
        }
        else {
            data += rc;
            left -= rc;
        }
    }
    while (left > 0);
    return 0;
}
