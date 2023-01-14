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

// todo adding perror and exit(1) where needed

void writeBufferToServer(int sockfd, char *buff, size_t messageLen, int shifting);
void writeIntToServer(int fd, long int num);
void clearBuffer(char *buff);
uint32_t readIntFromServer(int sockfd);
void readToBuffFromServer(int sockfd, char *buff, size_t messageLen);

int main(int argc, char **argv) { //general build taken from recitations code
    char *ip, *path, fileBuff[MB];
    FILE *fd;
    unsigned short port;
    int sockfd = -1;
    struct sockaddr_in serv_addr;
    struct sockaddr_in my_addr;   // where we actually connected through todo delete
    struct sockaddr_in peer_addr; // where we actually connected to todo delete
    socklen_t addrsize = sizeof(struct sockaddr_in ); // todo delete

    if (argc != 4) {
        printf("incorrect number of inputs\n");
        exit(1);
    } else {
        ip = argv[1];
        sscanf(argv[2], "%hu", &port);
        path = argv[3];
    }

    fd = fopen(path, "r");
    if(fd < 0) {
        perror("Can't open file for input path");
        exit(1);
    }
    fseek(fd, 0, SEEK_END);
    long int lenOfFile = ftell(fd);

    memset(&serv_addr, 0, sizeof(serv_addr));
    // serv_addr.sin_family = AF_INET; todo make sure if needed
    serv_addr.sin_port = htons(port);
    int success = inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    if (success == 0) {
        perror("input ip isn't a valid string");
        exit(1);
    }
    else {
        perror("error in inet_pton()");
        exit(1);
    }

    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) //SOCK_STREAM for TCP socket
    {
        perror("Could not create socket");
        exit(1);
    }

    printf("Client: connecting...\n"); // todo delete at the end
    // connect socket to the target address
    if(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect Failed");
        exit(1);
    }

    // todo delete at the end
    // print socket details
    getsockname(sockfd, (struct sockaddr*) &my_addr,   &addrsize);
    getpeername(sockfd, (struct sockaddr*) &peer_addr, &addrsize);
    printf("Client: Connected. \n"
           "\t\tSource IP: %s Source Port: %d\n"
           "\t\tTarget IP: %s Target Port: %d\n",
           inet_ntoa((my_addr.sin_addr)),    ntohs(my_addr.sin_port),
           inet_ntoa((peer_addr.sin_addr)),  ntohs(peer_addr.sin_port));
    //todo until here

    writeIntToServer(sockfd, lenOfFile);
    /*uint32_t file_len_network_byte_order = htonl((uint32_t) lenOfFile); // todo handle error
    if (send(sockfd, &file_len_network_byte_order, sizeof(uint32_t), 0) != sizeof(uint32_t)) { // https://www.gta.ufrj.br/ensino/eel878/sockets/htonsman.html
        perror("failed to send len of file to server");
    }*/

    memset(fileBuff, 0,sizeof(MB));
    char c;
    int charCounter = 0, chunksCounter = 0;
    while ((c = fgetc(fd)) != EOF)
    {
        fileBuff[charCounter] = c;
        charCounter++;
        if (charCounter==MB) {
            writeBufferToServer(sockfd, fileBuff, MB, chunksCounter);
        }
        clearBuffer(fileBuff);
        chunksCounter++;
    }

    uint32_t numPrintableChars = readIntFromServer(sockfd);
    /*
    if (recv(sockfd, &num_of_printable_chars, sizeof(uint32_t), 0) != sizeof(uint32_t)) {
        perror("failed to read num_of_printable_chars from server");
    }*/
    printf("# of printable characters: %u\n", (unsigned int) numPrintableChars);

    fclose(fd);
    close(sockfd);
    exit(0);
}

uint32_t readIntFromServer(int sockfd) { //https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
    uint32_t recv;
    char *intBuff = (char*)&recv;
    readToBuffFromServer(sockfd, intBuff, sizeof(uint32_t));
    return ntohl(recv);
}

void readToBuffFromServer(int sockfd, char *buff, size_t messageLen) {
    int charRead = 0;
    int totalRead = 0;
    while(messageLen - totalRead > 0) {
        charRead = read(sockfd, buff + totalRead, messageLen - 1);
        if(charRead <= 0)
            perror("error while reading from server to client"); // todo consider to modify
        //buff[bytes_read] = '\0';
        totalRead += charRead;
    }
}

void writeIntToServer(int sockfd, long int num) { // https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
    uint32_t conv = htonl(num);
    /*char *dataBuff;
    memset(dataBuff, 0,sizeof(conv));*/
    char *dataBuff = (char*)&conv;
    writeBufferToServer(sockfd, dataBuff, sizeof(conv), 0);
}

void clearBuffer(char *buff) {
    for (int i = 0; i < MB; ++i) {
        buff[i] = 0;
    }
}

void writeBufferToServer(int sockfd, char *buff, size_t messageLen, int shifting) {
    int charSend = 0;
    int totalSent = 0;
    while (messageLen - totalSent > 0) {
        charSend = write(sockfd, buff + (totalSent + (shifting*messageLen)), messageLen);
        if (charSend <= 0) {
            perror("error while writing from client to server"); // todo consider to modify
        }
        totalSent += charSend;
    }
}
