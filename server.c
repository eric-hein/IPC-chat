/*
 * Eric Hein
 * Operating Systems - Lab 3
 * server.c - a chat server (and monitor) that uses pipes and sockets
 *      Must be called with -h and -p optargs to specify host server and port number
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <netdb.h>

#define MAX_CLIENTS 10

// constants for pipe FDs
#define WFD 1
#define RFD 0


/**
 * nonblock - a function that makes a file descriptor non-blocking
 * @param fd file descriptor
 */
void nonblock(int fd) {
  int flags;

  if ((flags = fcntl(fd, F_GETFL, 0)) == -1) {
    perror("fcntl (get):");
    exit(1);
  }
  if (fcntl(fd, F_SETFL, flags | FNDELAY) == -1) {
    perror("fcntl (set):");
    exit(1);
  }
}


/*
 * monitor - provides a local chat window
 * @param srfd - server read file descriptor
 * @param swfd - server write file descriptor
 */
void monitor(int srfd, int swfd) {
  char buf[1024];
  int rbytes;
  struct pollfd fds[2];				//Pipe between server and monitor
  int timeout_msecs = 700;
  int ret_poll;
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;
  fds[1].fd = srfd;
  fds[1].events = POLLIN;
  int numfds = 2;

  while(1){
    if ((ret_poll = poll(fds, numfds, timeout_msecs)) == -1){
      perror("Error in monitor poll");
      exit(1);
    }
  
    if (fds[0].revents != 0){
      //Read from STDIN. If EOF received, close connection
      if ((rbytes = read(fds[0].fd, buf, sizeof(buf))) <= 0){
        if (rbytes == -1){
          perror("Read from STDIN");
          exit(1);
        }
        if (rbytes == 0)
          goto end;
      }
      //Write message to server
      write(swfd, buf, rbytes);
    }

    if (fds[1].revents != 0){
      //Read from Server
      if ((rbytes = read(fds[1].fd, buf, sizeof(buf))) <= 0){
        if (rbytes == -1){
          perror("Read from server");
          exit(1);
        }
       if (rbytes == 0)
         goto end;        
      }
	  //Write to STDOUT.
      write(STDOUT_FILENO, buf, rbytes);
    }
  }

  end:
  close(srfd);
  close(swfd);
}


/*
 * server - relays chat messages
 * @param mrfd - monitor read file descriptor
 * @param mwfd - monitor write file descriptor
 * @param portno - TCP port number to use for client connections
 */

//Server needs to take input from client (socket) and send it to monitor (pipe)
//and take in input from monitor (pipe) and send it to client (socket)
void server(int mrfd, int mwfd, int portno) {
  char buf[1024];
  int rbytes;

  //Poll implementation here: Poll function multiplexes over the set of file descriptors
  struct pollfd fds[10];		//File descriptors for each client
  int timeout_msecs = 100;
  int ret_poll;
  fds[0].fd = mrfd;
  fds[0].events = POLLIN;
  int numfds = 1;

  //Socket implementation here:
  int sockfd, clientsock;
  int status;
  int yes = 1;
  struct addrinfo *servinfo;
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  struct sockaddr_storage clientaddr;
  socklen_t addr_size;
  char portnum[5];

  sprintf(portnum, "%d", portno);
  //Getaddrinfo for socket
  if ((status = getaddrinfo(NULL, portnum, &hints, &servinfo)) != 0){
    //perror("Error in getaddrinfo");
    printf("getaddinfo error: %s\n", gai_strerror(status));
    exit(1);
  }

  sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

  //Bind
  if ((status = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)) == -1){
    perror("Error in binding socket");
    exit(1);
  }

  //Listen
  if ((status = listen(sockfd, 10)) != 0){
    perror("Error in listening on socket");
    exit(1);
  }

  nonblock(sockfd);  
  fds[1].fd = sockfd;
  fds[1].events = POLLIN;
  numfds++;
 
   //Main server chat loop. Handles all messages to/from clients and the server's monitor
  do {
    if ((ret_poll = poll(fds, numfds, timeout_msecs)) == -1){
      perror("Error in poll of sockets");
      exit(1);
    }
       
    //Accept
    addr_size = sizeof clientaddr;                                             
    if (numfds < MAX_CLIENTS && fds[1].revents != 0){
      if ((clientsock = accept(sockfd, (struct sockaddr *)&clientaddr, &addr_size)) == -1){
        perror("Error in accepting socket");
        exit(1);
      }
      //Adding client to poll fds[]
      if (clientsock > 0){
        nonblock(clientsock);
        fds[numfds].fd = clientsock;
        fds[numfds].events = POLLIN;
        numfds++;
      }
    } 

    //if monitor has message:
    if (fds[0].revents != 0){
     //Read from monitor. If EOF received, close connection
      if ((rbytes = read(fds[0].fd, buf, sizeof(buf))) == 0){
        goto end;
      }
     for (int i=2; i <= numfds; i++){
       write(fds[i].fd, buf, rbytes);
      }
    }

    //for each client with a message
    for (int i=2; i <= numfds; i++){
      if (fds[i].revents != 0){
        //Read from client
        if ((rbytes = recv(fds[i].fd, buf, sizeof(buf), 0)) <= 0){
          if (rbytes == 0){
            write(STDOUT_FILENO, "A client disconncected\n", 24);
         
            //Close this one connection, adjust contents of pollfd fds[]
            close(fds[i].fd);
            for (int j=i; j <= numfds; j++)
              fds[j] = fds[j+1];
            numfds--;
          }
          if (rbytes == -1){
            perror("Error in receiving message from a client");
            exit(1);
          }
        }
        //Write to monitor pipe
        write(STDOUT_FILENO, buf, rbytes);        
        write(fds[0].fd, buf, rbytes);
        //Write to each client in fdlist
        for (int j=2; j <= numfds-1; j++){
          if (i != j){
            if ((status = send(fds[j].fd, buf, rbytes, 0)) == -1){
              perror("Error in sending message to other clients");
              exit(1);
            }
          }
        }
      }
    }
  } while(1);

  end:
  printf("Closing\n");
  //Close all client connections
  for (int i=1; i < numfds; i++)
    close(fds[i].fd);
  close(clientsock);
  close(sockfd);
  freeaddrinfo(servinfo);
}


int main(int argc, char **argv) {
  //Pipes that simulate communication between the server and a monitor for it
  int toserver[2], tomonitor[2];        
  pid_t pid;
  int opt;
  char servname[200];
  int portno = 3724;

  //Handling -h and -p arguments
  while ((opt = getopt(argc, argv, "hp:")) != -1) {
    switch(opt) {
    case 'h':
      strcat(servname, optarg);
      break; 
    case 'p':
      portno = atoi(optarg);
      break;
    default: // '?'
      printf("usage: ./server [-h] [-p port #]\n-h - specify a server name\n"
        "-p # - the port to use when connecting to the server\n");
      exit(EXIT_FAILURE);  
    }
  }

  //Creating the pipes
  if (pipe(toserver) == -1) {
    perror("toserver pipe");
    exit(1);
  }
  if (pipe(tomonitor) == -1) {
    perror("tomonitor pipe");
    exit(1);
  }

  //Making all pipe communications non-blocking
  nonblock(toserver[RFD]);
  nonblock(toserver[WFD]);
  nonblock(tomonitor[RFD]);
  nonblock(tomonitor[WFD]);
 
  //Forking into 2 processes, with the parent acting as server and child acting as monitor
  pid = fork();

  if (pid == -1) {
    perror("fork");
    exit(1);
  } else if (pid == 0) {
    // child/monitor process. Closing pipe fd's which this process won't use
    close(toserver[RFD]);
    close(tomonitor[WFD]);
    monitor(tomonitor[RFD], toserver[WFD]);
    exit(0);
    close(tomonitor[RFD]);
    close(toserver[WFD]);
  } else {
    // parent/server process. Closing pipe fd's which this process won't use
    close(toserver[WFD]);
    close(tomonitor[RFD]);
    server(toserver[RFD], tomonitor[WFD], portno);
    wait(NULL);
    close(toserver[RFD]);
    close(tomonitor[WFD]);
  }
  exit(0);
}
