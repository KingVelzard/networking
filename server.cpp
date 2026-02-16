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
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "thread_pool.h"
#include <poll.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unordered_map>
#include <string>

#define PORT "3490" // the port the users will be connecting to
#define BACKLOG 8192 // how many pending connections

static const char RESPONSE[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 13\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "Hello, world!";
static const size_t RESPONSE_LEN = sizeof(RESPONSE) - 1;

struct Connection {
    int fd;
    size_t write_offset;

    Connection(int f, size_t wo)
        : fd(f), write_offset(wo) {}
};

std::unordered_map<int, Connection> fd_map;

/*
 * Convert socket to IP address string
 * addrL struct sockaddr_in or struct sockaddr_in6
 */
const char * inet_ntop2(void *addr, char *buf, size_t size)
{
    struct sockaddr_storage *sas = static_cast<sockaddr_storage*>(addr);
    struct sockaddr_in *sa4;
    struct sockaddr_in6 *sa6;
    void* src;

    switch(sas->ss_family) {
        case AF_INET:
            sa4 = (struct sockaddr_in*)addr;
            src = &(sa4->sin_addr);
            break;
        case AF_INET6:
            sa6 = (struct sockaddr_in6*)addr;
            src = &(sa6->sin6_addr);
            break;
        default:
            return NULL;
    }

    return inet_ntop(sas->ss_family, src, buf, size);
}


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




void handle_new_connection(int listen_sockfd,
		int epollfd, struct epoll_event* ev)
{
	struct sockaddr_storage remoteaddr; // Client address
	socklen_t addrlen;
	int newfd; // newly accepted sockfd
	char remoteIP[INET6_ADDRSTRLEN];
	addrlen = sizeof remoteaddr;

    while (true) {
        newfd = accept(listen_sockfd, (struct sockaddr*)&remoteaddr,
                &addrlen);


        if (newfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { 
                break; 
            } else { 
                perror("accept");
                break;
            }
        } else {
            // Add to interest list of epoll
            int flags = fcntl(newfd, F_GETFL, 0);
            fcntl(newfd, F_SETFL, flags | O_NONBLOCK);
            
            int enable = 1;
            setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));

            fd_map.emplace(newfd, Connection(newfd, 0));

            ev->events = EPOLLIN;
            ev->data.fd = newfd;

            if (epoll_ctl(epollfd, EPOLL_CTL_ADD, newfd, ev) == -1) {
                perror("epoll_ct1: newfd");
                exit(EXIT_FAILURE);
            }

            printf("pollserver: new connection from %s on socket %d\n",
                    inet_ntop2(&remoteaddr, remoteIP, sizeof remoteIP),
                    newfd);
        }
    }
}

void handle_client_data(int listen_sockfd, int epollfd,
		struct epoll_event* events, int nfd_i)
{
	char buf[4096];	// buffer for client data
	
  	int sender_fd = events[nfd_i].data.fd;
    
    auto it = fd_map.find(sender_fd);
    if (it == fd_map.end()) {
        return;
    }
    Connection* conn = &(it->second);


    while (true) {
    	int nbytes = recv(sender_fd, buf, sizeof buf, 0);
        
        if (nbytes > 0) { // got data
            
            size_t total_sent = 0;

            while (total_sent < RESPONSE_LEN) {
                int send_bytes = send(
                    sender_fd, 
                    RESPONSE + total_sent, 
                    RESPONSE_LEN - total_sent, 
                    0);
			
		        if (send_bytes == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        conn->write_offset = total_sent;
                        break;
                    } else {
                        perror("send");
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, sender_fd, NULL);
                        fd_map.erase(sender_fd);
                        close(sender_fd);
                        return;
                    }
                }

                if (send_bytes > 0) {
                    total_sent += send_bytes;
                }

            }

            conn->write_offset = 0; // reset after complete send
                
        } else if (nbytes == 0) { // Connection closed
			printf("pollserver: socket %d hung up \n", sender_fd);
		    epoll_ctl(epollfd, EPOLL_CTL_DEL, sender_fd, NULL);
            fd_map.erase(sender_fd);      
            close(sender_fd); 
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // no more data right now
                break;
		    } else {
                perror("recv");
                epoll_ctl(epollfd, EPOLL_CTL_DEL, sender_fd, NULL);
                fd_map.erase(sender_fd);
                close(sender_fd); 
                break;
            }
		
        }
    }
}


void process_connections(int listen_sockfd, int nfds, int epollfd,
		struct epoll_event* events, struct epoll_event* ev)
{
	for (int i{0}; i < nfds; ++i)
	{
			if (events[i].data.fd == listen_sockfd)
			{
				// if its the listener, new connection
				handle_new_connection(listen_sockfd, epollfd, ev);
			} else {
				// otherwise it's a regular client request
				handle_client_data(listen_sockfd, epollfd, events, i);
			}
	}
}

int main(void)
{
	int listen_sockfd;
    struct addrinfo hints, *servinfo, *p;
	struct sigaction sa;
	int yes=1;
	int rv;
	thread_pool::ThreadPool my_pool{
		static_cast<size_t>(std::thread::hardware_concurrency()
	)};

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my ip
	
	// polling inits
    struct epoll_event ev, events[BACKLOG];
    int epollfd;
    int nfds;
    
    fd_map.reserve(10000);

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	}
	
	// loop through all the results and bind to the first we can
	
	for (p = servinfo; p != NULL; p = p->ai_next) {
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
    
    int flags = fcntl(listen_sockfd, F_GETFL, 0);
    fcntl(listen_sockfd, F_SETFL, flags | O_NONBLOCK);

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sockfd, &ev) == -1) {
        perror("epoll_ctl: listen_sockfd");
        exit(EXIT_FAILURE);
    }
	printf("server: waiting for connections...\n");



	while (1) { // main accept loop
		nfds = epoll_wait(epollfd, events, BACKLOG, -1);

        if (nfds == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

		process_connections(listen_sockfd, nfds, epollfd, events, &ev);
	}
    
	return 0;
}

