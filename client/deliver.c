//
// Created by xuruijie on 9/15/21.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define FILE_PATH_PREFIX "/homes/x/xuruijie/Documents/361Lab/client/clientFiles/"
#define SEGMENT_DATA_SIZE 4096

void sendMessage (int sockfd, const struct sockaddr * sa_store, const char * message);
void computeSegment(char * segment, long totalSegmentCount, long segmentNumber, size_t size, char * token, char * fileContent);
void timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y);

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Server address and port number is incomplete.\n");
        return 1;
    }

    char file_path [256] = "";
    strcpy(file_path, FILE_PATH_PREFIX);
    char * user_input = malloc(256);
    char * token;
    if (user_input == NULL) {
        printf("No memory.\n");
        return 1;
    }

    while (1) {
        printf("Enter the name of the file that you want to transfer in the form of ftp <file name>: ");
        scanf(" %255[0-9a-zA-Z .]", user_input);
        token = strtok(user_input, " ");
        if (strcmp(token, "ftp") == 0) {
            break;
        }
        else {
            printf("Command not identified, please try again.\n");
        }
    }

    token = strtok(NULL, " ");
    if (token == NULL) {
        printf("File name missing, exiting.\n");
        return 1;
    }
    strcat(file_path, token);

    // checking if file exists
    FILE * file = fopen(file_path, "r");
    long fileLength, totalSegmentCount;
    if (file == NULL) {
        printf("File not found, terminating.\n");
        return 0;
    } else {
        printf("File exists, contacting server.\n");
        fseek(file, 0, SEEK_END);
        fileLength = ftell(file);
        fseek(file, 0, SEEK_SET);
        totalSegmentCount = fileLength / SEGMENT_DATA_SIZE;
        if (fileLength % SEGMENT_DATA_SIZE != 0) {
            totalSegmentCount++;
        }
        //printf("Total segment count is %ld, file length is %ld\n", totalSegmentCount, fileLength);
    }

    // initializing socket
    int sockfd;
    ssize_t numByte;
    char* server_ip = argv[1];
    char* server_port = argv[2];
    unsigned long port;
    port = strtoul(server_port, NULL, 10);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in sa_in;
    memset(&sa_in, 0, sizeof sa_in);
    sa_in.sin_family = AF_INET;
    sa_in.sin_port = htons(port);
    sa_in.sin_addr.s_addr = inet_addr(server_ip);

    // sending ftp to server to indicate that we want to transfer a file
    char * message = "rtt";
    char reply[256];
    memset(reply, 0, sizeof reply);
    struct sockaddr_storage sa_store;
    memset(&sa_store, 0, sizeof sa_store);
    socklen_t sl = sizeof sa_store;

    // measure RTT
    struct timeval tStart, tEnd, result;
    memset(&tStart, 0, sizeof tStart);
    memset(&tEnd, 0, sizeof tEnd);
    memset(&result, 0, sizeof result);
    gettimeofday(&tStart, NULL);
    sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&sa_in, (socklen_t)sizeof sa_in);
    recvfrom(sockfd, reply, sizeof reply, 0, (struct sockaddr *)&sa_store, &sl);
    gettimeofday(&tEnd, NULL);
    timeval_subtract(&result, &tEnd, &tStart);
    printf("RTT is %ld us\n", result.tv_usec);
    memset(reply, 0, sizeof reply);

    // ftp
    message = "ftp";
    numByte = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&sa_in, (socklen_t)sizeof sa_in);
    if (numByte == -1) {
        fprintf(stderr, "Failed to send message, byte sent: %zd", numByte);
    }

    // waiting for server to reply
    recvfrom(sockfd, reply, sizeof reply, 0, (struct sockaddr *)&sa_store, &sl);

    // if received yes from server, we can proceed
    if (strcmp(reply, "yes") == 0) {
        printf("A file transfer can start.\n");
    }

    printf("FILE TRANSFER BEGINS\n");
    char fileContent[SEGMENT_DATA_SIZE];
    char segment[256 + SEGMENT_DATA_SIZE];
    char currentAck [256];
    long segmentNumber = 1;
    size_t sizeReceived;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = result.tv_usec * 4;
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        fprintf(stderr, "Failed to set socket receive timeout option.\n");
    }
    // loop through all segments
    for (;segmentNumber <= totalSegmentCount; segmentNumber++) {
        memset(fileContent, 0, SEGMENT_DATA_SIZE);
        memset(segment, 0, sizeof segment);
        size_t size = fread(fileContent, 1, sizeof fileContent, file);
        computeSegment(segment, totalSegmentCount, segmentNumber, size, token, fileContent);

resendPacket:
        numByte = sendto(sockfd, segment, 256 + SEGMENT_DATA_SIZE, 0, (struct sockaddr *)&sa_in, (socklen_t)sizeof sa_in);
        if (numByte == -1) {
            fprintf(stderr, "Failed to send message, byte sent: %zd", numByte);
        }
        memset(reply, 0, sizeof reply);
        sizeReceived = recvfrom(sockfd, reply, sizeof reply, 0, (struct sockaddr *)&sa_store, &sl);

        if (sizeReceived == -1) {
            printf("Didn't hear from server, re-transmitting packet number %ld.\n", segmentNumber);
            goto resendPacket;
        }

        memset(currentAck, 0, sizeof currentAck);
        sprintf(currentAck, "%ld", segmentNumber);
        printf("Received ACK number %s from server\n", reply);
        if (strcmp(reply, currentAck) != 0) {
            printf("Wrong ACK, terminating.\n");
            break;
        }
    }

    fclose(file);
    free(user_input);
    close(sockfd);
    return 0;
}

void sendMessage (int sockfd, const struct sockaddr * sa_store, const char * message) {
    int numByte = sendto(sockfd, message, strlen(message), 0, sa_store, (socklen_t)sizeof *sa_store);
    if (numByte == -1) {
        fprintf(stderr, "Failed to send message, byte sent: %d", numByte);
    }
}

void computeSegment(char * segment, long totalSegmentCount, long segmentNumber, size_t size, char * token, char * fileContent) {
    unsigned long offset = 0;
    sprintf(segment + offset, "%ld", totalSegmentCount);
    offset = strlen(segment);
    sprintf(segment + offset++, "%c", ':');
    sprintf(segment + offset, "%ld", segmentNumber);
    offset = strlen(segment);
    sprintf(segment + offset++, "%c", ':');
    sprintf(segment + offset, "%zu", size);
    offset = strlen(segment);
    sprintf(segment + offset++, "%c", ':');
    sprintf(segment + offset, "%s", token);
    offset = strlen(segment);
    sprintf(segment + offset++, "%c", ':');
    memcpy(segment + offset, fileContent, size);
}

void timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;
}