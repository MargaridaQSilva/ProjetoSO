/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 
 Fom csapp
 Modified by Paul
 
 */
#include "csapp.h"

void *doit(void *p_fd);

void read_requesthdrs(rio_t *rp);

int parse_uri(char *uri, char *filename, char *cgiargs);

void serve_static(int fd, char *filename, int filesize);

void get_filetype(char *filename, char *filetype);

void serve_dynamic(int fd, char *filename, char *cgiargs);

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

queue_element_t *queue_element;
queue_element_t NULL_ELEMENT;

int numeroRequestStat = 0;
int THREADMAX = 1;
int queue_activesize = 0;
int queue_maxsize = 0;
int algorithm = 0; //0 - ANY/FIFO 1 - HPSC  2 - HPDC
sem_t qsizemutex;
sem_t threadmutex;
sem_t threadend;

int main(int argc, char **argv) {
    NULL_ELEMENT.fd = 0;
    NULL_ELEMENT.isstatic = 0;
    sem_init(&qsizemutex, 0, 1);
    sem_init(&threadmutex, 0, 0);
    sem_init(&threadend, 0, 1);
    int listenfd, connfd, port;
    //change to unsigned as sizeof returns unsigned
    struct sockaddr_in clientaddr;
    unsigned int clientlen = sizeof(clientaddr);


    /* Check command line args */
    if (argc != 5) {
        fprintf(stderr, "usage: %s <port> <size of thread-pool> <buffer size> <algorithm (ANY/FIFO, HPSC, HPDC)>\n",
                argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);
    THREADMAX = atoi(argv[2]);
    queue_maxsize = atoi(argv[3]);
    queue_element_t arraytemp[queue_maxsize];
    queue_element = arraytemp;
    if (strcmp(argv[4], "HPSC") == 0) {
        algorithm = 1;
    } else if (strcmp(argv[4], "HPDC") == 0) {
        algorithm = 2;
    } else {
        algorithm = 0;
    }

    //CREATE THREAD POOL

    pthread_t threadpool[THREADMAX];
    int ids[THREADMAX];

    for (int i = 0; i < THREADMAX; i++) {
        ids[i] = i;
    }

    for (int i = 0; i < THREADMAX; i++) {
        pthread_create(&threadpool[i], NULL, doit, &ids[i]);
    }


    fprintf(stderr, "Server : %s Running on  <%d>\n", argv[0], port);

    listenfd = Open_listenfd(port);
    while (connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen)) {

        //line:netp:tiny:accept  oldline - connfd = Accept (listenfd, (SA *) & clientaddr, &clientlen);
        //pthread_t t;
        int *pclient = malloc(sizeof(int));//client socket

        *pclient = connfd;
        insertq(queue_element, *pclient, 1);
        fflush(stdout);

        //is static doesnt matter for FIFO, the only method currently implemented
        //sem_wait(&threadend);
        sem_post(&threadmutex);
        //pthread_create(&t, NULL, doit, pclient);
        //Close(connfd);        //line:netp:tiny:close ->retirei este close devido ao erro Rio_readlineb error: Bad file descriptor
    }
    return 0;
}

/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void *doit(void *args) {
    while (1) {
        sem_wait(&threadmutex);
        queue_element_t element;

        int id = *(int *) args;
        element = selector(queue_element);
        int fd = element.fd;
        printf("FD %d id %d", fd, id);
        //free(p_fd);
        int is_static;
        struct stat sbuf;
        char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
        char filename[MAXLINE], cgiargs[MAXLINE];
        rio_t rio;


        if (queue_activesize >= queue_maxsize) {
            clienterror(fd, method, "400", "Bad Request", "Server is busy.");
            Close(fd);
            pthread_exit(NULL);
            return NULL;
        }

        /* Read request line and headers */
        Rio_readinitb(&rio, fd);
        Rio_readlineb(&rio, buf, MAXLINE);    //line:netp:doit:readrequest
        sscanf(buf, "%s %s %s", method, uri, version);    //line:netp:doit:parserequest

        if (strcasecmp(method, "GET")) {                //line:netp:doit:beginrequesterr
            clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");


            Close(fd);
            return NULL;
        }

        //line:netp:doit:endrequesterr
        read_requesthdrs(&rio);    //line:netp:doit:readrequesthdrs

        /* Parse URI from GET request */
        is_static = parse_uri(uri, filename, cgiargs);    //line:netp:doit:staticcheck

        if (stat(filename, &sbuf) < 0) {                //line:netp:doit:beginnotfound
            clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");

            Close(fd);
            return NULL;
        }                //line:netp:doit:endnotfound
        if (is_static) {/* Serve static content */
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {            //line:netp:doit:readable
                clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
                Close(fd);
                return NULL;
            }
            serve_static(fd, filename, sbuf.st_size);    //line:netp:doit:servestatic
        } else {                /* Serve dynamic content */
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {            //line:netp:doit:executable
                clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");

                Close(fd);
                return NULL;
            }

            serve_dynamic(fd, filename, cgiargs);    //line:netp:doit:servedynamic
        }
        Close(fd);
        //sem_post(&threadend);
    }
}

/* $end doit */

/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {                //line:netp:readhdrs:checkterm
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {                /* Static content *///line:netp:parseuri:isstatic
        strcpy(cgiargs, "");    //line:netp:parseuri:clearcgi
        strcpy(filename, ".");    //line:netp:parseuri:beginconvert1
        strcat(filename, uri);    //line:netp:parseuri:endconvert1
        if (uri[strlen(uri) - 1] == '/')    //line:netp:parseuri:slashcheck
            strcat(filename, "home.html");    //line:netp:parseuri:appenddefault
        return 1;
    } else {                /* Dynamic content *///line:netp:parseuri:isdynamic
        ptr = index(uri, '?');    //line:netp:parseuri:beginextract
        if (ptr) {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        } else
            strcpy(cgiargs, "");    //line:netp:parseuri:endextract
        strcpy(filename, ".");    //line:netp:parseuri:beginconvert2
        strcat(filename, uri);    //line:netp:parseuri:endconvert2
        return 0;
    }
}

/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);    //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //line:netp:servestatic:beginserve
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sRequestStat: %d\r\n", buf, numeroRequestStat++);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);    //line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);    //line:netp:servestatic:mmap
    Close(srcfd);        //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);    //line:netp:servestatic:write
    Munmap(srcp, filesize);    //line:netp:servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 * Deverá adicionar mais tipos
 */
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "application/mp4");
    else
        strcpy(filetype, "text/plain");
}

/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXLINE], *emptylist[] = {NULL};

    int pipefd[2];

    /*Paul Crocker
      Changed so that client content is piped back to parent
    */
    Pipe(pipefd);

    if (Fork() == 0) {                /* child *///line:netp:servedynamic:fork
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1);    //line:netp:servedynamic:setenv
        //Dup2 (fd, STDOUT_FILENO);	/* Redirect stdout to client *///line:netp:servedynamic:dup2
        Dup2(pipefd[1], STDOUT_FILENO);

        Execve(filename, emptylist, environ);    /* Run CGI program *///line:netp:servedynamic:execve
    }
    close(pipefd[1]);
    char content[1024]; //max size that cgi program will return

    int contentLength = read(pipefd[0], content, 1024);
    Wait(NULL);            /* Parent waits for and reaps child *///line:netp:servedynamic:wait


    /* Generate the HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //line:netp:servestatic:beginserve
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sRequestStat: %d\r\n", buf, numeroRequestStat++);
    sprintf(buf, "%sContent-length: %d\r\n", buf, contentLength);
    sprintf(buf, "%sContent-type: text/html\r\n\r\n", buf);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve

    Rio_writen(fd, content, contentLength);

}

/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    /* Fazer primeiro visto que precisamos de saber o tamanho do body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=" "ffffff" ">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sRequestStat: %d\r\n", buf, numeroRequestStat++);
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Content-length: %d\r\n\r\n", (int) strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));


    Rio_writen(fd, body, strlen(body));
}

/* $end clienterror */

/* queue manipulation */

queue_element_t selector(queue_element_t *queue) {
    int index;
    queue_element_t out;
    if (algorithm == 1) {
        // STATIC IS HIGH PRIO
    } else if (algorithm == 2) {
        //DYNAMIC IS HIGH PRIO
    } else {
        queue_element_t out = queue[0];
        removeq(queue);
        return out;

    }

}

void removeq(queue_element_t *queue) { // ONLY WORKS FOR FIFO
    sem_wait(&qsizemutex);
    for (int i = 0; i + 1 <= queue_activesize; i++) {
        if (i - 1 == queue_activesize) { // OUT OF BOUNDS
            printf("\nremoving if, i %d as %d\n", i, queue_activesize);
            fflush(stdout);
            queue[i] = queue[i + 1];
        } else {
            printf("\nelse i %d\n", i);
            fflush(stdout);
            queue[i] = NULL_ELEMENT;
        }
    }
    queue_activesize--;
    sem_post(&qsizemutex);
    return NULL;
}

void insertq(queue_element_t *queue, int fd, int isstatic) { //ONLY WORKS FOR FIFO

    queue_element_t element;
    element.fd = fd;
    element.isstatic = isstatic;
    sem_wait(&qsizemutex);
    queue[queue_activesize] = element;
    queue_activesize++;
    sem_post(&qsizemutex);

    return NULL;
}