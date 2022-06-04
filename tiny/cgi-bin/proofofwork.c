
/* $begin pow */
#include "csapp.h"
#include "tomcrypt_hash.h"

int firstnarezeros(int, unsigned char *);

int main(void) {

    hash_state md;
    unsigned char tmp[20];
    unsigned long k = 0;
    char *buf, *p;
    char string[MAXLINE], arg2[MAXLINE]; // content[MAXLINE];
    int difficulty = 0, diffcheck = 0;

    /* Extract the two arguments */
    if ((buf = getenv("QUERY_STRING")) != NULL) {
        p = strchr(buf, '&');
        *p = '\0';              //anular o & dentro do string p -  para aproveitar no strcpy a seguir
        strcpy(string, buf);
        strcpy(arg2, p + 1);
        difficulty = atoi(arg2);

    }
    char *powStr = (char *) malloc(strlen(string) + 10 + 1);
    while (1) {

        //TENHO a certeza que alocei bem memoria para sprintf
        sprintf(powStr, "%s%lu", string, k);

        sha1_init(&md);
        sha1_process(&md, (unsigned char *) powStr, (unsigned long) strlen(powStr));
        sha1_done(&md, tmp);

        if ((diffcheck = firstnarezeros(difficulty, tmp)) >= 1) {
            if(diffcheck == 2){
                break;
            }
            printf("Proof of work done: nonce = %lu\n HASH = ", k);

            for (int i = 0; i < 20; i++)
                printf("%x ", tmp[i]);
            printf("\n");
            fflush(stdout);
            break;
        }
        /*if (k % 5000000 == 0)
            printf("searching %lu\n", k);*/
        k++;
    }


    /* Make the response body *
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);
    */

    /* Generate the HTTP response */
    /* Paul Crocker - changed so that headers always produced on the parent process */
    //printf("Content-length: %d\r\n", (int)strlen(content));
    //printf("Content-type: text/html\r\n\r\n");

    //printf("%s", content);
    //fflush(stdout);
    exit(0);
}

int firstnarezeros(int difficulty, unsigned char* tmp){
    int flag = 1;
    if(difficulty > 20){
        fprintf(stderr, "DIFFICULTY CANNOT BE >20\n");
        return 2;
    }
    for(int i = 0; i<=difficulty; i++){
        if(tmp[i] != 0){
            flag = 0;
        }
    }
    return flag;
}
/* $end adder */
