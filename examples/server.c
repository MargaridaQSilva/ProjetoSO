/* server.c - a stream socket server demo */
#include "example.h"

#define MYPORT 3490		// the port users will be connecting to
#define BACKLOG 10		// how many pending connections queue will hold

int main (void)
{
  int sockfd;				// listen on sock_fd
  int new_fd;				// new connection on new_fd
  struct sockaddr_in my_addr;		// my address information
  struct sockaddr_in their_addr;	// connector’s address information
  socklen_t sin_size;			// tamanho dos sockets
  int yes = 1;

  /* Criar um Socket com propriedades */
  sockfd = socket (AF_INET, SOCK_STREAM, 0);
  setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int));

  /* Construção do endereço do servidor */
  my_addr.sin_family = AF_INET;	// host byte order
  my_addr.sin_port = htons (MYPORT);	// short, network byte order
  my_addr.sin_addr.s_addr = INADDR_ANY;	// automatically fill with my IP
  memset (&(my_addr.sin_zero), '\0', 8);	// zero the rest of the struct


/* LIGAR Socket com Endereço */
  bind (sockfd, (struct sockaddr *) &my_addr, sizeof (struct sockaddr));

/* Começar a escutar .. */
  listen (sockfd, BACKLOG);
/*Nova ligação será aceita */

  sin_size = sizeof (struct sockaddr_in);

  new_fd = accept (sockfd, (struct sockaddr *) &their_addr, &sin_size);

  printf ("server: got connection from %s\n",inet_ntoa (their_addr.sin_addr));

  /*Escreve qualquer coisa – 14 bytes -para o cliente */
  send (new_fd, "Hello, world!\n", 14, 0);

  return 0;
}
