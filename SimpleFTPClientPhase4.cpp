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
    if(argc != 5)
    {
        cerr<<"Usage: executable serverIPAddr:port op fileName receiveInterval\n";
        exit(1);
    }
    //char serverIp[20], serverPort[10];
    string temp = argv[1];
    int receiveInterval = atoi(argv[4])*1000000;  //will recv 1000B every these many ns
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
    command[0] = argv[2][0];
    command[1] = argv[2][1];
    command[2] = argv[2][2];
    command[3] = ' ';
    string filename = argv[3];
    int j;
    for(j = 0; j < filename.size(); j++)
    {
        command[4 + j] = filename[j];
    }
    command[4 + j] = '\0';

    //keep on trying till it succeeds
    if(send(sock, command, 5 + filename.size(), 0) < 0)
    {
        cout<<"Sending at the client failed\n";
        exit(-1);
    }
    // else
    // {
    //     cout<<"The command sent is "<<command<<'\n';
    // }

    if ((argv[2][0] == 'g') && (argv[2][1] == 'e') && (argv[2][2] == 't'))
    {
        int len = 1024;
        char buffer[len];  //the max bytes that are sent is 1024 from the other size
        FILE *recvFile = fopen(argv[3], "w");
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
    }
    else if ((argv[2][0] == 'p') && (argv[2][1] == 'u') && (argv[2][2] == 't'))
    {
        int len = 1024;
        char buffer[len];  //the max bytes that are sent is 1024 from the other size
        FILE *sendFile = fopen(argv[3], "r");
        if(sendFile == NULL)
        {
            cerr<<"File cannot be opened at the client\n";
            exit(3);
        }
        bzero(buffer, len);
        int lenRead = 0, totalBytes = 0;
        while(true)
        {
            int lenRead = fread(buffer, sizeof(char), len, sendFile);
            totalBytes += lenRead;
            if(lenRead > 0)
            {
                if(send(sock, buffer, lenRead, 0) < 0)
                {
                    cerr<<"Sending at client failed\n";
                    fclose(sendFile);
                    close(sock);
                    exit(-1);
                }
            }
            if(lenRead != len)
            {
                cout<<"File sent: "<<totalBytes<<" bytes\n";
                fclose(sendFile);
                close(sock);
                break;
            }
            bzero(buffer, len);
            nanosleep(&time1, &time2);
        }
    }
    return 0;
}