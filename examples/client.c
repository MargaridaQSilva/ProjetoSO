/*
** client.c - a stream socket client demo
*/
#include "example.h"

#define PORT 3490		// the port client will be connecting to
#define MAXDATASIZE 100		// max number of bytes we can get at once

int main (int argc, char *argv[])
{
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  struct hostent *he;
  struct sockaddr_in their_addr;	// connector’s address information
  if (argc != 2)
    {
      fprintf (stderr, "usage: client hostname\n");
      exit (1);
    }
  he = gethostbyname (argv[1]);	// get the host info
  if (NULL == he)
    {
      perror ("Host Info Failed");
      exit (1);
    }

  sockfd = socket (AF_INET, SOCK_STREAM, 0);

  their_addr.sin_family = AF_INET;	// host byte order
  their_addr.sin_port = htons (PORT);	// short, network byte order
  their_addr.sin_addr = *((struct in_addr *) he->h_addr);
  memset (&(their_addr.sin_zero), '\0', 8);	// zero the rest of the struct
  connect (sockfd, (struct sockaddr *) &their_addr, sizeof (struct sockaddr));

  numbytes = recv (sockfd, buf, MAXDATASIZE - 1, 0);

  buf[numbytes] = '\0';
  printf ("Received from server: %s", buf);
  close (sockfd);

  return 0;
}
