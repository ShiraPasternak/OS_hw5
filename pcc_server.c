//
// Created by shira on 07/01/2023.
//
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>

#define MB (1024 * 1024)
#define PRINTABLE 95

typedef struct PCC {
    int count_arr[PRINTABLE];
} pcc;

void initPcc();
void clearClientPcc();
void addClientCountToTotal();
bool clientCountIsEmpty();
void printStatic()

void clearBuffer(char *buff);
int writeBufferToClient(int connfd, char *buff, size_t messageLen, int shifting);
int writeIntToClient(int fd, long int num);
int readToBuffFromClient(int connfd, char *buff, size_t messageLen);
uint32_t readIntFromClient(int connfd);
int countPrintableInChunk(char *buff, int buffLen);


pcc pcc_total[PRINTABLE], pcc_curr_client[PRINTABLE];
bool firstClientFlag = true;
bool stopServerFlag = false;

void initPcc() {
    if (firstClientFlag) {
        memset(&pcc_total->count_arr, 0, PRINTABLE * sizeof(int));
        memset(&pcc_curr_client->count_arr, 0, PRINTABLE * sizeof(int));
        firstClientFlag = false;
    }
    clearClientPcc();
}

void clearClientPcc() {
    for (int i = 0; i < PRINTABLE; ++i) {
        pcc_curr_client->count_arr[i] = 0;
    }
}

void addClientCountToTotal() {
    for (int i = 0; i < PRINTABLE; ++i) {
        pcc_total->count_arr[i] += pcc_curr_client->count_arr[i];
    }
}

bool clientCountIsEmpty() {
    for (int i = 0; i < PRINTABLE; ++i) {
        if (pcc_curr_client->count_arr[i] != 0) {
            return false;
        }
    }
    return true;
}

void printStatic() {
    for (int i = 0; i < PRINTABLE; ++i) {
        printf("char '%c' : %u times\n", i+32, pcc_total->count_arr[i]);
    }
}

void currClientSignalHandler(int signum) {
    if (clientCountIsEmpty()) {
        printStatic();
        exit(0);
    }
    else {
        stopServerFlag = true;
    }
}

int main(int argc, char **argv) { //general build taken from recitations code
    char dataBuff[MB];
    unsigned short port;
    int listenfd  = -1, connfd = -1;
    int failureInClient = 0;

    struct sockaddr_in serv_addr;
    struct sockaddr_in peer_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);

    if (argc != 2) {
        perror("incorrect number of inputs\n");
        exit(1);
    } else {
        sscanf(argv[1], "%hu", &port);
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0){
        perror("error in initializing of socket from server side");
        exit(1);
    }

    // Build server address
    memset(&serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    //https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) { // set socket to be reusable
        perror("Failed to set socket to SO_REUSEADDR");
        exit(1);
    }
    if(bind(listenfd, (struct sockaddr*) &serv_addr, addrsize) != 0) {
        perror("Bind Failed");
        exit(1);
    }
    if(listen(listenfd, 10) != 0) {
        perror("Listen Failed");
        exit(1);
    }

    struct sigaction handleCurrClient = {.sa_handler = currClientSignalHandler}; // taken for recitation code

    while(1) {
        if (sigaction(SIGINT, &handleCurrClient, NULL) == -1) { // taken for recitation code
            perror("Signal handle registration failed");
            exit(1);
        }
        initPcc();

        // Accept a connection
        connfd = accept(listenfd, (struct sockaddr*) &peer_addr, &addrsize);
        if (connfd < 0 && connfd != -EINTR) {
            exit(1);
            perror("Accept Failed");
        }
        if (connfd < 0) {
            perror("Accept Failed");
            continue; // jump to next client
        }

        // reading N (size of text) from client
        uint32_t expectedLen = readIntFromClient(connfd);
        if (expectedLen < 0){
            close(connfd);
            connfd = -1;
            continue; // jump to next client
        }

        // reading chunks of chars max 1 MB size from client
        int printable = 0, charRead = 0, remChar, chunks = 0, expChunkLen = 0;
        remChar = expectedLen;
        if (expectedLen % MB > 0)
            chunks = (expectedLen / MB) + 1;
        else
            chunks = expectedLen / MB;
        for (int i = 0; i < chunks; ++i) { // reading in chunks of 1 MB
            if (remChar / MB == 0)
                expChunkLen = remChar % MB;
            else
                expChunkLen = MB;
            charRead = readToBuffFromClient(connfd, dataBuff, expChunkLen);
            if (charRead < 0 && failureInClient != 1) {
                failureInClient = 1;
                break;
            }
            remChar -= charRead;
            printable += countPrintableInChunk(dataBuff, expChunkLen);
            clearBuffer(dataBuff);
        }
        if (failureInClient) { // if read from client field due accepted, jump to next client
            failureInClient = 0;
            close(connfd);
            connfd = -1;
            continue;
        }

        // Write C to client
        if (writeIntToClient(connfd, printable) == 0)
            addClientCountToTotal();

        close(connfd);
        if (stopServerFlag) { //part of SIGINT handling
            printStatic();
            exit(0);
        }
    }
    exit(0);
}

int writeBufferToClient(int connfd, char *buff, size_t messageLen, int shifting) { // return 0 if no errors
    int charSend = 0;
    int totalSent = 0;
    while (messageLen - totalSent > 0) {
        charSend = write(connfd, buff + (totalSent + (shifting*MB)), messageLen);
        if (charSend <= 0) {
            perror("error while writing from server to client");
            if (charSend == -ETIMEDOUT || charSend == -ECONNRESET || charSend == -EPIPE || charSend == -EINTR || charSend == 0)
                return -1;
            else
                exit(1);
        }
        totalSent += charSend;
    }
    return 0;
}

int readToBuffFromClient(int connfd, char *buff, size_t messageLen) { // returns number of chars read successfully from client
    int charRead = 0;
    int totalRead = 0;
    while(messageLen - totalRead > 0) {
        charRead = read(connfd, buff + totalRead, messageLen - totalRead);
        if (charRead <= 0) {
            perror("error while reading from client to server");
            if (charRead == -ETIMEDOUT || charRead == -ECONNRESET || charRead == -EPIPE || charRead == -EINTR || charRead == 0)
                return -1;
            else
                exit(1);
        }
        totalRead += charRead;
    }
    return totalRead;
}

//https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
int writeIntToClient(int fd, long int num) { // return 0 if no errors
    uint32_t conv = htonl(num);
    char *dataBuff = (char*)&conv;
    return writeBufferToClient(fd, dataBuff, sizeof(conv), 0);
}

//https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
uint32_t readIntFromClient(int connfd) {  // returns number of chars read successfully from client
    uint32_t recv;
    char *intBuff = (char*)&recv;
    if (readToBuffFromClient(connfd, intBuff, sizeof(uint32_t)) < 0)
        return -1;
    return ntohl(recv);
}

void clearBuffer(char *buff) {
    for (int i = 0; i < MB; ++i) {
        buff[i] = 0;
    }
}

int countPrintableInChunk(char *buff, int buffLen) {
    int counter = 0;
    for (int i = 0; i < buffLen; ++i) {
        if (buff[i] >= 32 && buff[i] <= 126) {
            pcc_curr_client->count_arr[buff[i]-32]++;
            counter++;
        }
    }
    return counter;
}