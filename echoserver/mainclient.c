#include "csapp.h" 

/* usage: ./echoclient host port */
int main(int argc, char **argv)
{ 
    int clientfd, port; 
    char *host, buf[MAXLINE]; 
    rio_t rio; 
    
    host = argv[1]; 
    port = atoi(argv[2]); 
 
    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd); 
 
    while (Fgets(buf, MAXLINE, stdin) != NULL) { 
        Rio_writen(clientfd, buf, strlen(buf)); 
        Rio_readlineb(&rio, buf, MAXLINE); 
        Fputs(buf, stdout); 
    } 
    Close(clientfd); 
    exit(0); 
} 

