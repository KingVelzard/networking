/*
 * server.cpp -- a stream socket server demo
*/


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
#include <sys/wait.h>
#include <signal.h>
#include "thread_pool.h"


#define PORT "3490" // the port the users will be connecting to
#define BACKLOG 10 // how many pending connections

void sigchld_handler(int s)
{
	(void)s;

	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6
void* get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void add_to_pfds(struct pollfd **pfds, int newfd, int *fd_count,
		int *fd_size)
{
	// If we don't have room, add more space in the pfds array
	if (*fd_count == *fd_size) {
		*fd_size *= 2; // Double it
		*pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
	}

	(*pfds)[*fd_count].fd = newfd;
	(*pfds)[*fd_count].events = POLLIN; // Check ready-to-read
	(*pfds)[*fd_count].revents = 0;

	(*fd_count)++;
}


void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
	// copy the one from the end over this one
	pfds[i] = pfds[*fd_count - 1]
	
		(*fd_count)--;
}

void handle_new_connection(int listener, int *fd_count,
		int *fd)size, struct pollfd **pfds)
{
	struct sockaddr_storage remoteaddr; // Client address
	socklen_t addrlen;
	int newfd; // newly accepted sockfd
	char remoteIP[INET6_ADDRSTRLEN];
	addrlen = sizeof remoteaddr;
	newfd = accept(listen_sockfd, (struct sockaddr*)&remoteaddr,
			&addrlen);

	if (newfd == -1) {
		perror("accept");
	} else {
		add_to_pfds(pfds, newfd, fd_count, fd_size);

		printf("pollserver: new connection from %s on socket %d\n".
				inetntop2($remoteaddr, remoteIP, sizeof remoteIP),
				newfd);
		}
}

void handle_client_data(int listen_sockfd, int *fd_count,
		struct pollfd *pfds, int *pfd_i)
{
	char buf[256];	// buffer for client data
	
	int nbytes = recv(pfd_i[*pfd_i].fd, buf, sizeof buf, 0);

	int sender_fd = pfds[*pfd_i].fd;

	const char* response =
        	"HTTP/1.1 200 OK\r\n"
                "Content-Length: 13\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Hello, world!";

	if (nbytes <= 0) { // Got error or connection closed
		if (nbytes == 0) {
			printf("pollserver: socket %d hung up \n", sender_fd);
			
		} else {
			perror("recv");
		}
		
		close(pfds[*pfd_i].fd); 

		del_from_pfds(pfds, *pfd_i, fd_count);

		// reexamine slot we just deleted
		//
		(*pfd_i)--;
	} else { // We got good data from client
		printf("pollserver: recv from fd %d: %.*s", sender_fd, nbytes, buf);

		for (int j{0}; j < *fd_count; ++j) {
			int dest_fd = pfds[j].fd;

			// except the listener and sender
			if (dest_fd != listen_sockfd && dest_fd != sender_fd) {
				if (send(dest_fd, response, sizeof response, 0) == -1) {
					perror("send");
				}
			}
		}
	}
}


void process_connections(int listen_sockfd, int *fd_count, int *fd_size,
		struct pollfd *pfds)
{
	for (int i{0}; i < *fd_count; ++i)
	{
		// Check if someone's ready to send
		if ((*pfds)[i].revents & (POLLIN | POLLHUP)) {
			// We have an event!!

			if ((*pfds)[i].fd == listen_sockfd)
			{
				// if its the listener, new connection
				handle_new_connection(listen_sockfd, fd_count, fd_size,
							pfds);
			} else {
				// otherwise it's a regular client request
				handle_client_data(listener, fd_count, *pfds, &i);
			}
		}
	}
}

int main(void)
{
	int listen_sockfd, recv_sockfd;
       	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector addr info
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	thread_pool::ThreadPool my_pool{
		static_cast<size_t>(std::thread::hardware_concurrency()
	)};

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my ip
	
	// polling inits
	int fd_size = 100;
	int fd_count = 0;
	struct pollfd *pfds = malloc(sizeof *pfds * fd_size);	

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	}
	
	// loop through all the results and bind to the first we can
	
	for (p = servinfo; p != NULL; p->ai_next) {
		if ((listen_sockfd = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) == -1) {
			perror("server : socket");
			continue;
		}

		if (setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
					sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(listen_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(listen_sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with serv info
	
	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}


	if (listen(listen_sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	
	pfds[0].fd = listen_sockfd;
	pfds[0].events = POLLIN;
	printf("server: waiting for connections...\n");



	while (1) { // main accept loop
		int poll_count = poll(pfds, fd_count, -1);

		if (poll_count == -1) {
			perror("poll");
			exit(1);
		}

		process_connections(listen_sockfd, &fd_count, &fd_size, &pfds);
	}
	
	free(pfds);
	return 0;
}
