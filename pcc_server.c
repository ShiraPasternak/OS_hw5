//
// Created by shira on 07/01/2023.
//
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdint.h>
#include <math.h>

#define MB (1024 * 1024)
#define PRINTABLE 95

void writeBufferToClient(char *buff, int connfd, size_t messageLen, int shifting); //todo
void writeIntToClient(long int file, int fd); //todo
void clearBuffer(char *buff);
void readToBuffFromClient(int connfd, char *buff, size_t messageLen); //todo
uint32_t readIntFromClient(int connfd); //todo
void countPrintableInChunk(char *buff); //todo


int main(int argc, char **argv) { //general build taken from recitations code
    char dataBuff[MB];
    unsigned short port;
    int listenfd  = -1, connfd = -1;
    int pcc_total[PRINTABLE];
    struct sockaddr_in serv_addr;
    struct sockaddr_in my_addr;   // where we actually connected through todo delete
    struct sockaddr_in peer_addr; // where we actually connected to todo delete
    socklen_t addrsize = sizeof(struct sockaddr_in); // todo delete

    if (argc != 2) {
        perror("incorrect number of inputs");
        exit(1);
    } else {
        sscanf(argv[1], "%hu", &port);
    }

    memset(&pcc_total, 0, PRINTABLE);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, 0, addrsize);

    serv_addr.sin_family = AF_INET;
    // INADDR_ANY = any local machine address
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if(0 != bind(listenfd, (struct sockaddr*) &serv_addr, addrsize))
    {
        printf("\n Error : Bind Failed. %s \n", strerror(errno));
        return 1;
    }

    if(0 != listen(listenfd, 10))
    {
        printf("\n Error : Listen Failed. %s \n", strerror(errno));
        return 1;
    }

    while(1)
    {
        // Accept a connection.
        // Can use NULL in 2nd and 3rd arguments
        // but we want to print the client socket details
        connfd = accept(listenfd, (struct sockaddr*) &peer_addr, &addrsize);
        if(connfd < 0)
        {
            perror("Error : Accept Failed. %s\n");
            return 1;
        }

        // todo delete at the end
        getsockname(connfd, (struct sockaddr*) &my_addr, &addrsize);
        getpeername(connfd, (struct sockaddr*) &peer_addr, &addrsize);
        printf( "Server: Client connected.\n"
                "\t\tClient IP: %s Client Port: %d\n"
                "\t\tServer IP: %s Server Port: %d\n",
                inet_ntoa( peer_addr.sin_addr ),
                ntohs(     peer_addr.sin_port ),
                inet_ntoa( my_addr.sin_addr   ),
                ntohs(     my_addr.sin_port   ) );
        //todo until here

        // reading phase - len of text
        uint32_t expectedLen = readIntFromClient(connfd);
        int chunks = 0;
        // reading phase - text
        memset(dataBuff, 0,sizeof(MB));
        if (expectedLen % MB > 0)
            chunks = expectedLen / MB + 1;
        else
            chunks = expectedLen / MB;
        for (int i = 0; i < chunks; ++i) {
            readToBuffFromClient(connfd, dataBuff, MB);
            countPrintableInChunk(dataBuff);
        }
        // writing phase
        
        // close socket
        close(connfd);
    }
    exit(0);
}

void clearBuffer(char *buff) {
    for (int i = 0; i < MB; ++i) {
        buff[i] = 0;
    }
}