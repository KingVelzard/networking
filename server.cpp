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

	printf("server: waiting for connections...\n");



	while (1) { // main accept loop
		sin_size = sizeof their_addr;
		recv_sockfd = accept(listen_sockfd, (struct sockaddr*)&their_addr,
				&sin_size);
		if (recv_sockfd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family, 
				get_in_addr((struct sockaddr*)&their_addr),
				s, INET6_ADDRSTRLEN);
		printf("server: got connection from %s\n", s);

		my_pool.push({ // this delegates send to threads
			thread_pool::TaskType::Execute, // Execute
			[recv_sockfd](const std::vector<int>&)
			{
				if (send(recv_sockfd, "Hello, world!", 13, 0) == -1)
					perror("send");
				close(recv_sockfd);
			},
			{recv_sockfd}
		});
	}

	return 0;
}
