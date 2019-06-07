/*
 * Eric Hein
 * Operating Systems - Lab 3
 * client.c - A chat client that uses sockets to connect to a server
 *	   Must be called with -h and -p optargs to specify host server and port number
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

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


int main(int argc, char **argv){
  //Variable declaration/initialization
  int portno = 3724;
  char servname[200];			//Name of the server, received from -h opt
  char buf[1024];
  int opt;
  int rbytes;
  struct pollfd fds[2];			//Bidirectional file descriptor for communication to server
  int timeout_msecs = 100;
  int ret_poll;
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;
  int numfds = 1;
 
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

  //Socket implementation here:
  int status;
  struct addrinfo *servinfo;
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  int sockfd;
  char portnum[5];

  sprintf(portnum, "%d", portno);
  //Get addr info for socket
  if ((status = getaddrinfo(servname, portnum, &hints, &servinfo)) != 0){
    perror("Error in getaddrinfo");
    exit(1);
  }

  sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
 
  //Connect
  if ((status = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)) != 0){
    perror("Error in connecting socket");
    exit(1);
  }

  //Updating pollfd with the socket
  nonblock(sockfd);
  fds[1].fd = sockfd;
  fds[1].events = POLLIN;
  numfds++;

  //Main chat loop. Sends/Receives messages to/from server
  while(1){
    if ((ret_poll = poll(fds, numfds, timeout_msecs)) == -1){
      perror("Error in poll");
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
      if ((status = send(fds[1].fd, buf, rbytes, 0)) == -1){
        perror("Error in sending message to other clients");
        exit(1);
      }
    }

    if (fds[1].revents != 0){
      //Read from Server
      if ((rbytes = recv(fds[1].fd, buf, sizeof(buf), 0)) <= 0){
        if (rbytes == -1){
          perror("Read from server");
          exit(1);
        }
        if (rbytes == 0)
          goto end;
      }
	  //Write to STDOUT.
      write(fds[0].fd, buf, rbytes);
    }
  }

  end:
  close(sockfd);
  freeaddrinfo(servinfo);
}
