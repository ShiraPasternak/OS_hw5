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

void clearBuffer(char *buff);
int writeBufferToClient(int connfd, char *buff, size_t messageLen, int shifting);
int writeIntToClient(int fd, long int num);
int readToBuffFromClient(int connfd, char *buff, size_t messageLen);
uint32_t readIntFromClient(int connfd);
int countPrintableInChunk(char *buff);


pcc pcc_total[PRINTABLE], pcc_curr_client[PRINTABLE];
bool firstClientFlag = true;
bool totalIsUpdated;

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
    totalIsUpdated = true;
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
    printf("Can't stop me! (%d)\n", signum); // todo delete before submitting
    if (clientCountIsEmpty()) {
        exit(1);
    }
    else {
        if (!totalIsUpdated)
            addClientCountToTotal();
        printStatic();
        exit(1);
    }
}

int main(int argc, char **argv) { //general build taken from recitations code
    char dataBuff[MB];
    unsigned short port;
    int listenfd  = -1, connfd = -1;
    int failureInClient = 0;

    struct sockaddr_in serv_addr;
    struct sockaddr_in my_addr;   // where we actually connected through todo delete
    struct sockaddr_in peer_addr; // where we actually connected to todo delete
    socklen_t addrsize = sizeof(struct sockaddr_in); // todo delete

    if (argc != 2) {
        printf("incorrect number of inputs\n");
        exit(1);
    } else {
        sscanf(argv[1], "%hu", &port);
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
        perror("error in initializing of socket from server side");
    if (listenfd < 0 && listenfd != EINTR)
        exit(1);

    printf("socket created\n");

    memset(&serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    // INADDR_ANY = any local machine address
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    //https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
    if (setsockopt(listenfd, SOL_SOCKET/*getprotobyname("tcp")*/, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) { //todo make sure whats right
        perror("Failed to set socket to SO_REUSEADDR");
        exit(1);
    }
    printf("setsockopt done\n");
    if(bind(listenfd, (struct sockaddr*) &serv_addr, addrsize) != 0) {
        perror("Bind Failed");
        exit(1);
    }
    printf("bind done\n");
    if(listen(listenfd, 10) != 0) {
        perror("Listen Failed");
        exit(1);
    }
    printf("listen done\n");
    struct sigaction handleCurrClient = {.sa_handler = currClientSignalHandler};
    while(1) {
        printf("entered loop\n");
        totalIsUpdated = false;
        if (sigaction(SIGINT, &handleCurrClient, NULL) == -1) { // taken for recitation code
            perror("Signal handle registration failed");
            exit(1);
        }
        initPcc();
        printf("succeeded to init PCC\n");
        // Accept a connection.
        // Can use NULL in 2nd and 3rd arguments
        // but we want to print the client socket details
        connfd = accept(listenfd, (struct sockaddr*) &peer_addr, &addrsize);
        if (connfd < 0)
            perror("Accept Failed");
        if (connfd < 0 && connfd != -EINTR)
            exit(1);
        printf("connection succeeded\n");
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
        printf("starting reading from client - len of file\n");
        // reading phase - len of text
        uint32_t expectedLen = readIntFromClient(connfd);
        if (expectedLen < 0){
            close(connfd);
            continue;
            //failureInClient = 1;
        }
        printf("expectedLen = %lu\n",(unsigned long)expectedLen);

        int printable, chunks = 0;
        // reading phase - text
        memset(dataBuff, 0,sizeof(MB));
        if (expectedLen % MB > 0)
            chunks = expectedLen / MB + 1;
        else
            chunks = expectedLen / MB;
        printf("starting reading from client - file\n");
        for (int i = 0; i < chunks; ++i) { // reading in chunks of 1 MB
            printf("reading the %d-th chunk\n", i);
            if (readToBuffFromClient(connfd, dataBuff, MB) < 0 && failureInClient != 1) {
                failureInClient = 1;
                break;
            }
            printable = countPrintableInChunk(dataBuff);
        }
        if (failureInClient) {
            failureInClient = 0;
            close(connfd);
            continue;
        }
        // writing phase
        if (writeIntToClient(connfd, printable) == 0)
            addClientCountToTotal();
        signal(SIGINT, SIG_DFL);
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

int writeBufferToClient(int connfd, char *buff, size_t messageLen, int shifting) { // return 0 if no errors
    int charSend = 0;
    int totalSent = 0;
    while (messageLen - totalSent > 0) {
        charSend = write(connfd, buff + (totalSent + (shifting*messageLen)), messageLen);
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

//https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
int writeIntToClient(int fd, long int num) { // return 0 if no errors
    uint32_t conv = htonl(num);
    char *dataBuff = (char*)&conv;
    return writeBufferToClient(fd, dataBuff, sizeof(conv), 0);
}

int readToBuffFromClient(int connfd, char *buff, size_t messageLen) {
    printf("in readToBuffFromClient\n");
    int charRead = 0;
    int totalRead = 0;
    while(messageLen - totalRead > 0) {
        charRead = read(connfd, buff + totalRead, messageLen - 1);
        if (charRead <= 0) {
            perror("error while reading from client to server");
            if (charRead == -ETIMEDOUT || charRead == -ECONNRESET || charRead == -EPIPE || charRead == -EINTR || charRead == 0)
                return -1;
            else
                exit(1);
        }
        totalRead += charRead;
    }
    printf("server read : %s\n", buff);
    return 0;
}

//https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
uint32_t readIntFromClient(int connfd) { // returns zero if no errors
    printf("in readIntFromClient\n");
    uint32_t recv;
    char *intBuff = (char*)&recv;
    if (readToBuffFromClient(connfd, intBuff, sizeof(uint32_t)) < 0)
        return -1;
    return ntohl(recv);
}

int countPrintableInChunk(char *buff) {
    int counter = 0;
    for (int i = 0; i < PRINTABLE; ++i) {
        if (buff[i] >= 32 && buff[i] <= 126) {
            pcc_curr_client->count_arr[buff[i]-32]++;
            counter++;
        }
    }
    return counter;
}