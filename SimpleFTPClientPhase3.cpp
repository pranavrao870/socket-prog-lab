#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <cstdlib>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <time.h>

using namespace std;

int main(int argc, char** argv)
{
    if(argc != 4)
    {
        cerr<<"Usage: executable serverIPAddr:port fileToReceive receiveInterval\n";
        exit(1);
    }
    //char serverIp[20], serverPort[10];
    string temp = argv[1];
    int receiveInterval = atoi(argv[3])*1000000;  //will recv 1000B every these many ns
    struct timespec time1, time2;
    time1.tv_sec = 0;
    time1.tv_nsec = receiveInterval;
    const char* serverIp = temp.substr(0, temp.find(':')).c_str();
    int serverPortN = atoi(temp.substr(temp.find(':') + 1, temp.size()).c_str());

    int sock = 0;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cerr<<"socket failed\n";
        exit(-1);
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPortN);
    if(inet_aton(serverIp, &(serverAddr.sin_addr)) == 0)
    {
        cerr<<"Ip conversion failed\n";
        exit(-1);
    }
    memset(&(serverAddr.sin_zero), '\0', 8);

    if(connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
    {
        cerr<<"connection to server failed\n";
        exit(2);
    }
    else
    {
        cout<<"ConnectDone: "<<serverIp<<':'<<serverPortN<<'\n';
    }

    char command[1024];
    command[0] = 'g';
    command[1] = 'e';
    command[2] = 't';
    command[3] = ' ';
    string filename = argv[2];
    int j;
    for(j = 0; j < filename.size(); j++)
    {
        command[4 + j] = filename[j];
    }
    command[4 + j] = '\0';

    //keep on trying till it succeeds
    if(send(sock, command, 1024, 0) < 0)
    {
        cout<<"Sending at the server failed\n";
        exit(-1);
    }

    int len = 1024;
    char buffer[len];  //the max bytes that are sent is 1024 from the other size
    FILE *recvFile = fopen(argv[2], "w");
    if(recvFile == NULL)
    {
        cerr<<"File cannot be created at the client\n";
        exit(3);
    }
    bzero(buffer, len);
    int lenRecvd = 0, totalBytes = 0;
    while((lenRecvd = recv(sock, buffer, len, 0)) > 0)
    {
        totalBytes += lenRecvd;
        int lenWritten = fwrite(buffer, sizeof(char), lenRecvd, recvFile);
        if(lenWritten < lenRecvd)
        {
            cerr<<"Data cannot be written properly at the client at the client\n";
            exit(3);
        }
        bzero(buffer, len);
        nanosleep(&time1, &time2);
    }

    fclose(recvFile);
    close(sock);
    cout<<"FileWritten: "<<totalBytes<<" bytes\n";
    return 0;
}