#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#define SEGMENT_DATA_SIZE 4096
#define SERVER_FILE_PREFIX "./serverFiles/"

void sendMessage (int sockfd, const struct sockaddr * sa_store, const char * message);
double generateRand ();

int main(int argc, char* argv[]) {
    // return if port number not specified
    if (argc < 2) {
        printf("UDP listen port is missing.\n");
        return 1;
    }

    // convert port number to ul
    char* portStr = argv[1];
    unsigned long port;
    port = strtoul(portStr, NULL, 10);
    int sockfd, bindResult;

    // set up address and port number for server to listen on
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(port);
    // initialize socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // bind socket to the predefined address and check if we're successful.
    bindResult = bind(sockfd, (struct sockaddr*) &sa, sizeof sa);
    if (bindResult == -1) {
        printf("Binding failed, exiting.\n");
        close(sockfd);
        return 1;
    }

    char buffer[256 + SEGMENT_DATA_SIZE] = "";
    struct sockaddr_storage sa_store;
    memset(&sa_store, 0, sizeof sa_store);
    socklen_t sl = sizeof sa_store;

    // reply with rtt to help measure RTT
    recvfrom(sockfd, buffer, sizeof buffer, 0, (struct sockaddr *)&sa_store, &sl);
    sendMessage(sockfd, (struct sockaddr *)&sa_store, "rtt");
    memset(buffer, 0, sizeof buffer);

    // start file transfer related communication
    recvfrom(sockfd, buffer, sizeof buffer, 0, (struct sockaddr *)&sa_store, &sl);
    if (strcmp(buffer, "ftp") == 0) {
        printf("Received ftp from client, replying with yes.\n");
        sendMessage(sockfd, (struct sockaddr *)&sa_store, "yes");
    }
    else {
        printf("Didn't received ftp from client, replying with no.\n");
        sendMessage(sockfd, (struct sockaddr *)&sa_store, "no");
    }

    FILE * file = NULL;
    char fileName[256] = "";
    strcpy(fileName, SERVER_FILE_PREFIX);

    unsigned long totalSegmentCount, currentSegmentNumber;
    size_t size;
    char ackNum[256] = "";
    int count = 0, count2 = 0;
    do {
        count2++;
        memset(buffer, 0, sizeof buffer);
        memset(ackNum, 0, sizeof ackNum);
        recvfrom(sockfd, buffer, sizeof buffer, 0, (struct sockaddr *)&sa_store, &sl);
        unsigned long offset = 0;

        // reading total segment count
        char * token = strtok(buffer, ":");
        offset += strlen(token) + 1;
        totalSegmentCount = strtoul(token, NULL, 10);

        // reading current segment number
        token = strtok(NULL, ":");
        strcpy(ackNum, token);
        offset += strlen(token) + 1;
        currentSegmentNumber = strtoul(token, NULL, 10);

        // reading size of data
        token = strtok(NULL, ":");
        offset += strlen(token) + 1;
        size = strtoul(token, NULL, 10);

        // reading file name
        token = strtok(NULL, ":");
        offset += strlen(token) + 1;
        if (file == NULL) {
            strcat(fileName, token);
            file = fopen(fileName, "w");
        }

        if (generateRand() <= 0.1) {
            fwrite(buffer + offset, 1, size, file);
            sendMessage(sockfd, (struct sockaddr *)&sa_store, ackNum);
            printf("Replying with ACK number %s\n", ackNum);
        }
        else {
            count++;
            printf("Dropping this packet.\n");
        }
    } while (currentSegmentNumber < totalSegmentCount);
    printf("Dropped %d out of %d packets.\n", count, count2);
    fclose(file);
	close(sockfd);
	return 0;
}

void sendMessage (int sockfd, const struct sockaddr * sa_store, const char * message) {
    int numByte = sendto(sockfd, message, strlen(message), 0, sa_store, (socklen_t)sizeof *sa_store);
    if (numByte == -1) {
        fprintf(stderr, "Failed to send message, byte sent: %d", numByte);
    }
}

double generateRand() {
    return (double) rand() / (double) RAND_MAX;
}
