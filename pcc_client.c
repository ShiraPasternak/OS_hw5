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
#include <unistd.h>
#include <string.h>

#define MB (1024 * 1024)
//#define MB 10

// todo adding perror and exit(1) where needed

int writeBufferToServer(int sockfd, char *buff, size_t messageLen);
int writeIntToServer(int fd, long int num);
void clearBuffer(char *buff);
uint32_t readIntFromServer(int sockfd);
int readToBuffFromServer(int sockfd, char *buff, size_t messageLen);
void cleanUp(FILE *fd, int sockfd);


int main(int argc, char **argv) { //general build taken from recitations code
    char *ip, *path, fileBuff[MB];
    FILE *fd;
    unsigned short port;
    int sockfd = -1;
    struct sockaddr_in serv_addr;

    if (argc != 4) {
        perror("incorrect number of inputs\n");
        exit(1);
    } else {
        ip = argv[1];
        sscanf(argv[2], "%hu", &port);
        path = argv[3];
    }

    fd = fopen(path, "r");
    if(fd == NULL) {
        perror("Can't open file for input path\n");
        exit(1);
    }
    if (fseek(fd, 0, SEEK_END) < 0) {
        perror("error in fseek in file");
        cleanUp(fd, sockfd);
        exit(1);
    }
    long int lenOfFile = ftell(fd);
    if (lenOfFile < 0) {
        perror("error in ftell in file");
        cleanUp(fd, sockfd);
        exit(1);
    }
    //rewind(fd);
    if (fseek(fd, 0, SEEK_SET) < 0) {
        perror("error in fseek in file");
        cleanUp(fd, sockfd);
        exit(1);
    }
    //printf("lenOfFile = %lu\n", (unsigned long)lenOfFile);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; // todo make sure if needed
    serv_addr.sin_port = htons(port);
    int success = inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    if (success == 0) {
        perror("input ip isn't a valid string");
        cleanUp(fd, sockfd);
        exit(1);
    }
    else if (success < 0) {
        perror("error in inet_pton()");
        cleanUp(fd, sockfd);
        exit(1);
    }

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { //SOCK_STREAM for TCP socket
        perror("Could not create socket");
        cleanUp(fd, sockfd);
        exit(1);
    }

    // connect socket to the target address
    if(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect Failed");
        cleanUp(fd, sockfd);
        exit(1);
    }

    if (writeIntToServer(sockfd, lenOfFile) < 0) {
        cleanUp(fd, sockfd);
        exit(1);
    }
    /*uint32_t file_len_network_byte_order = htonl((uint32_t) lenOfFile); // todo handle error
    if (send(sockfd, &file_len_network_byte_order, sizeof(uint32_t), 0) != sizeof(uint32_t)) { // https://www.gta.ufrj.br/ensino/eel878/sockets/htonsman.html
        perror("failed to send len of file to server");
    }*/

    //memset(fileBuff, 0,sizeof(MB));

    int charWrote = 0, remChar, chunks = 0, expChunkLen;
    remChar = lenOfFile;
    if (lenOfFile % MB > 0)
        chunks = (lenOfFile / MB) + 1;
    else
        chunks = lenOfFile / MB;
    for (int i = 0; i < chunks; ++i) { // writing in chunks of 1 MB
        if (remChar / MB == 0)
            expChunkLen = remChar % MB;
        else
            expChunkLen = MB;
        if (fread(fileBuff, expChunkLen, 1, fd) < expChunkLen) {
            perror("Failed reading file");
            cleanUp(fd, sockfd);
            exit(1);
        }
        else {
            if (writeBufferToServer(sockfd, fileBuff, expChunkLen) < 0) {
                cleanUp(fd, sockfd);
                exit(1);
            }
            remChar -= charWrote;
            clearBuffer(fileBuff);
        }
    }
    /*    char c;
    int charCounter = 0, chunksCounter = 0;
    while ((c = fgetc(fd)) != EOF) {
        fileBuff[charCounter] = c;
        charCounter++;
        if (charCounter==MB) {
            if (writeBufferToServer(sockfd, fileBuff, MB) < 0) {
                cleanUp(fd, sockfd);
                exit(1);
            }
            clearBuffer(fileBuff);
            chunksCounter++;
            charCounter = 0;
            printf("charCounter = %d, chunksCounter = %d\n", charCounter);
        }
    }
    if (c == EOF && (((chunksCounter*MB) + charCounter) != lenOfFile)) {
        perror("Failed reading file");
        cleanUp(fd, sockfd);
        exit(1);
    }
    if (charCounter != 0) {
        printf("shit im here1\n");
        if (writeBufferToServer(sockfd, fileBuff, charCounter) < 0) {
            cleanUp(fd, sockfd);
            exit(1);
        }
    }
    printf("shit im here2\n");*/
    uint32_t numPrintableChars = readIntFromServer(sockfd);
    if (numPrintableChars < 0) {
        cleanUp(fd, sockfd);
        exit(1);
    }
    /*
    if (recv(sockfd, &num_of_printable_chars, sizeof(uint32_t), 0) != sizeof(uint32_t)) {
        perror("failed to read num_of_printable_chars from server");
    }*/
    printf("# of printable characters: %u\n", (unsigned int) numPrintableChars);
    cleanUp(fd, sockfd);
    exit(0);
}

void cleanUp(FILE *fd, int sockfd) {
    if (sockfd != -1)
        close(sockfd);
    fclose(fd);
}

uint32_t readIntFromServer(int sockfd) { //https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
    uint32_t recv;
    char *intBuff = (char*)&recv;
    if (readToBuffFromServer(sockfd, intBuff, sizeof(uint32_t)) < 0) {
        return -1;
    }
    return ntohl(recv);
}

int readToBuffFromServer(int sockfd, char *buff, size_t messageLen) {
    int charRead = 0;
    int totalRead = 0;
    while(messageLen - totalRead > 0) {
        charRead = read(sockfd, buff + totalRead, messageLen);
        if(charRead <= 0) {
            perror("error while reading from server to client");
            return -1;
        }
        //buff[bytes_read] = '\0';
        totalRead += charRead;
    }
    return 0;
}

int writeIntToServer(int sockfd, long int num) { // https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
    uint32_t conv = htonl(num);
    char *dataBuff = (char*)&conv;
    return writeBufferToServer(sockfd, dataBuff, sizeof(conv));
}

int writeBufferToServer(int sockfd, char *buff, size_t messageLen) {
    //printf("in writeBufferToServer with len = %d and shift = %d\n", (int)messageLen, shifting);
    int charSend = 0;
    int totalSent = 0;
    while (messageLen - totalSent > 0) {
        charSend = write(sockfd, buff + totalSent, messageLen);
        if (charSend <= 0) {
            perror("error while writing from client to server");
            return -1;
        }
        totalSent += charSend;
    }
    return 0;
}

void clearBuffer(char *buff) {
    for (int i = 0; i < MB; ++i) {
        buff[i] = 0;
    }
}