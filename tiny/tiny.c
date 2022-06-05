/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 
 Fom csapp
 Modified by Paul

 */

/* Afonso Oliveira (44404), Margarida Silva (43367), Ruben Abadesso (43664) @ UBI */

#include "csapp.h"

void *doit(void *p_fd);

void read_requesthdrs(rio_t *rp);

int parse_uri(char *uri, char *filename, char *cgiargs);

void serve_static(int fd, char *filename, int filesize);

void get_filetype(char *filename, char *filetype);

void serve_dynamic(int fd, char *filename, char *cgiargs);

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

struct timeval timediff(struct timeval stop);

queue_element_t *queue_element;
queue_element_t NULL_ELEMENT;

int numeroRequestStat = 0;
int THREADMAX = 1;
int queue_activesize = 0;
int queue_maxsize = 0;
int algorithm = 0; //0 - ANY/FIFO 1 - HPSC  2 - HPDC
int dispatchcount = 0, completecount = 0;
sem_t qsizemutex;
sem_t threadmutex;
sem_t threadstats;

struct timeval starttime;

int main(int argc, char **argv) {
    gettimeofday(&starttime, NULL);
    int arrival_count = 0;
    struct timeval arrival_time;
    rio_t nullrio;
    NULL_ELEMENT.fd = 0;
    NULL_ELEMENT.isstatic = 0;
    strcpy(NULL_ELEMENT.filename, "");
    strcpy(NULL_ELEMENT.buf, "");
    strcpy(NULL_ELEMENT.cgiargs, "");
    strcpy(NULL_ELEMENT.method, "");
    strcpy(NULL_ELEMENT.version, "");
    strcpy(NULL_ELEMENT.uri, "");
    NULL_ELEMENT.rio = nullrio;
    sem_init(&qsizemutex, 0, 1);
    sem_init(&threadmutex, 0, 0);
    sem_init(&threadstats, 0, 1);
    int listenfd, connfd, port;
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
    if (THREADMAX <= 0 || queue_maxsize <= 0) {
        fprintf(stderr, "Error - size of thread-pool and size of buffer must both be > 0\n");
        exit(1);
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
        // !! STATS - NEW ARRIVAL
        gettimeofday(&arrival_time, NULL);
        arrival_count++;
        arrival_time = timediff(arrival_time);
        printf("Stat-req-arrival-count - %d\n", arrival_count);
        printf("Stat-req-arrival-time - %ld seconds %ld microseconds\n", arrival_time.tv_sec, arrival_time.tv_usec);

        int *pclient = malloc(sizeof(int));//client socket

        *pclient = connfd;
        if (queue_activesize >= queue_maxsize) {
            clienterror(*pclient, "BUSY", "400", "Server is busy", "Try later"); // nao funciona
            Close(*pclient);
            continue;
        }
        insertq(queue_element, *pclient);
        sem_post(&threadmutex);
    }
    return 0;
}

/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void *doit(void *args) {
    struct timeval dispatch_time;
    struct timeval complete_time;
    int staticcount = 0, dynamiccount = 0;
    while (1) {
        sem_wait(&threadmutex);
        printf("Stat-req-dispatch-count - %d\n", dispatchcount);
        queue_element_t element;
        int id = *(int *) args;
        element = selector(queue_element);
        int fd = element.fd;
        int is_static = element.isstatic;
        printf("FD %d ID %d", fd, id);
        fflush(stdout);
        struct stat sbuf;
        char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
        char filename[MAXLINE], cgiargs[MAXLINE];
        rio_t rio = element.rio;
        strcpy(buf, element.buf);
        strcpy(method, element.method);
        strcpy(uri, element.uri);
        strcpy(version, element.version);
        strcpy(filename, element.filename);
        strcpy(cgiargs, element.cgiargs);


        if (strcasecmp(method, "GET")) {                //line:netp:doit:beginrequesterr
            clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
            sem_wait(&threadstats);
            dispatchcount++;
            sem_post(&threadstats);
            gettimeofday(&dispatch_time, NULL);
            dispatch_time = timediff(dispatch_time);
            printf("Thread ID: %d\nStat-req-dispatch-time - %ld seconds %ld microseconds\n", id, dispatch_time.tv_sec,
                   dispatch_time.tv_usec);
            Close(fd);
            continue;
        }


        if (stat(filename, &sbuf) < 0) {                //line:netp:doit:beginnotfound
            clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
            sem_wait(&threadstats);
            dispatchcount++;
            sem_post(&threadstats);
            gettimeofday(&dispatch_time, NULL);
            dispatch_time = timediff(dispatch_time);
            printf("Thread ID: %d\nStat-req-dispatch-time - %ld seconds %ld microseconds\n", id, dispatch_time.tv_sec,
                   dispatch_time.tv_usec);
            Close(fd);

            continue;
        }                //line:netp:doit:endnotfound



        if (is_static) {/* Serve static content */

            if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
                //line:netp:doit:readable
                clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
                sem_wait(&threadstats);
                dispatchcount++;
                sem_post(&threadstats);
                gettimeofday(&dispatch_time, NULL);
                dispatch_time = timediff(dispatch_time);
                printf("Thread ID: %d\nStat-req-dispatch-time - %ld seconds %ld microseconds\n", id,
                       dispatch_time.tv_sec, dispatch_time.tv_usec);
                Close(fd);

                continue;
            }
            printf("Stat-req-complete-count (STATIC ONLY) - %d\n", completecount);
            sem_wait(&threadstats);
            completecount++;
            sem_post(&threadstats);
            serve_static(fd, filename, sbuf.st_size);    //line:netp:doit:servestatic
            gettimeofday(&complete_time, NULL);
            complete_time = timediff(complete_time);
            staticcount++;
            printf("Thread ID: %d\n"
                   "Stat-req-complete-time - %ld seconds %ld microseconds\n"
                   "Stat-thread-count - %d\n"
                   "Stat-thread-static - %d\n"
                   "Stat-thread-dynamic - %d\n", id, complete_time.tv_sec, complete_time.tv_usec,
                   staticcount + dynamiccount, staticcount, dynamiccount);


        } else {                /* Serve dynamic content */
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {            //line:netp:doit:executable
                clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
                sem_wait(&threadstats);
                dispatchcount++;
                sem_post(&threadstats);
                gettimeofday(&dispatch_time, NULL);
                dispatch_time = timediff(dispatch_time);
                printf("Thread ID: %d\nStat-req-dispatch-time - %ld seconds %ld microseconds\n", id,
                       dispatch_time.tv_sec, dispatch_time.tv_usec);
                Close(fd);

                continue;
            }

            serve_dynamic(fd, filename, cgiargs);    //line:netp:doit:servedynamic
            dynamiccount++;
            printf("Thread ID: %d\n"
                   "Stat-thread-count - %d\n"
                   "Stat-thread-static - %d\n"
                   "Stat-thread-dynamic - %d\n", id, staticcount + dynamiccount, staticcount, dynamiccount);

        }

        Close(fd);
        sem_wait(&threadstats);
        dispatchcount++;
        sem_post(&threadstats);
        gettimeofday(&dispatch_time, NULL);
        dispatch_time = timediff(dispatch_time);
        printf("Thread ID: %d\nStat-req-dispatch-time - %ld seconds %ld microseconds\n", id, dispatch_time.tv_sec,
               dispatch_time.tv_usec);
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
    queue_element_t out;
    if (algorithm == 1) {
        for (int i = 0; i + 1 <= queue_activesize; i++) {
            if (queue[i].isstatic == 1) {
                strcpy(out.buf, queue[i].buf);
                strcpy(out.method, queue[i].method);
                strcpy(out.uri, queue[i].uri);
                strcpy(out.version, queue[i].version);
                strcpy(out.filename, queue[i].filename);
                strcpy(out.cgiargs, queue[i].cgiargs);
                out.rio = queue[i].rio;
                out.fd = queue[i].fd;
                out.isstatic = queue[i].isstatic;
                removeq(queue, i);
                return out;
            }
        }
        strcpy(out.buf, queue[0].buf);
        strcpy(out.method, queue[0].method);
        strcpy(out.uri, queue[0].uri);
        strcpy(out.version, queue[0].version);
        strcpy(out.filename, queue[0].filename);
        strcpy(out.cgiargs, queue[0].cgiargs);
        out.rio = queue[0].rio;
        out.fd = queue[0].fd;
        out.isstatic = queue[0].isstatic;
        removeq(queue, 0);
        return out;
    } else if (algorithm == 2) {
        for (int i = 0; i + 1 <= queue_activesize; i++) {
            if (queue[i].isstatic == 0) {
                strcpy(out.buf, queue[i].buf);
                strcpy(out.method, queue[i].method);
                strcpy(out.uri, queue[i].uri);
                strcpy(out.version, queue[i].version);
                strcpy(out.filename, queue[i].filename);
                strcpy(out.cgiargs, queue[i].cgiargs);
                out.rio = queue[i].rio;
                out.fd = queue[i].fd;
                out.isstatic = queue[i].isstatic;
                removeq(queue, i);
                return out;
            }
        }

        strcpy(out.buf, queue[0].buf);
        strcpy(out.method, queue[0].method);
        strcpy(out.uri, queue[0].uri);
        strcpy(out.version, queue[0].version);
        strcpy(out.filename, queue[0].filename);
        strcpy(out.cgiargs, queue[0].cgiargs);
        out.rio = queue[0].rio;
        out.fd = queue[0].fd;
        out.isstatic = queue[0].isstatic;
        removeq(queue, 0);
        return out;
    } else {
        strcpy(out.buf, queue[0].buf);
        strcpy(out.method, queue[0].method);
        strcpy(out.uri, queue[0].uri);
        strcpy(out.version, queue[0].version);
        strcpy(out.filename, queue[0].filename);
        strcpy(out.cgiargs, queue[0].cgiargs);
        out.rio = queue[0].rio;
        out.fd = queue[0].fd;
        out.isstatic = queue[0].isstatic;
        removeq(queue, 0);
        return out;

    }

}

void removeq(queue_element_t *queue, int index) {
    sem_wait(&qsizemutex);

    for (int i = index; i + 1 <= queue_activesize; i++) {
        if (i + 1 == queue_activesize) {
            queue[i] = NULL_ELEMENT;
        } else {
            queue[i] = queue[i + 1];
        }
    }
    queue_activesize--;
    sem_post(&qsizemutex);
    return NULL;
}

void insertq(queue_element_t *queue, int fd) {

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);    //line:netp:doit:readrequest
    sscanf(buf, "%s %s %s", method, uri, version);    //line:netp:doit:parserequest
    int isstatic = parse_uri(uri, filename, cgiargs);
    read_requesthdrs(&rio);

    queue_element_t element;
    strcpy(element.buf, buf);
    element.fd = fd;
    element.isstatic = isstatic;
    strcpy(element.method, method);
    strcpy(element.uri, uri);
    strcpy(element.version, version);
    strcpy(element.filename, filename);
    strcpy(element.cgiargs, cgiargs);
    element.rio = rio;
    sem_wait(&qsizemutex);
    queue[queue_activesize] = element;
    queue_activesize++;
    sem_post(&qsizemutex);


    return NULL;
}

struct timeval timediff(struct timeval stop) {
    struct timeval out;
    long secs = stop.tv_sec - starttime.tv_sec;
    long ms = stop.tv_usec - starttime.tv_usec;
    if (ms < 0) {
        secs--;
        ms = 1e+6 + ms;
    }
    out.tv_sec = secs;
    out.tv_usec = ms;
    return out;
}