#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <cstdlib>
#include <stdio.h>
#include <unistd.h>

using namespace std;

//using simple fstream fuctions results in problems while
//sending the image and other non-text files due to ascii
//corruption. So referring to the fread and fwite


int main(int argc, char** argv)
{
    if(argc != 3)
    {
        cerr<<"Usage: executable PortNum fileToTransfer\n";
        exit(1);
    }

    int serverPort = atoi(argv[1]);
    char* filename = argv[2];

    int initialSockfd;
    if((initialSockfd = socket(PF_INET, SOCK_STREAM, 0)) == 0)
    {
        cerr<<"socket failed\n";
        exit(-1);
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    memset(&(serverAddr.sin_zero), '\0', 8);
    if (bind(initialSockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
    {
        cerr<<"bind failed\n";
        exit(2);
    }
    else
    {
        cout<<"BindDone: "<<serverPort<<endl;
    }

    if(listen(initialSockfd, 5) == -1)
    {
        cerr<<"listen failed\n";
        close(initialSockfd);
        exit(-1);
    }
    else
    {
        cout<<"ListenDone: "<<serverPort<<endl;
    }

    int connSocket;
    struct sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    if((connSocket = accept(initialSockfd, (struct sockaddr*)&clientAddr, (socklen_t*)&clientAddrLen)) < 0)
    {
        cerr<<"Accept failed\n";
        close(initialSockfd);
        exit(-1);
    }
    else
    {
        //convert the 32 bits ip address into human readabe ip address and also
        //network ordering to host ordering
        cout<<"Client: "<<inet_ntoa(clientAddr.sin_addr)<<':'<<ntohs(clientAddr.sin_port)<<'\n';
    }

    int len = 1024;   //sending 1024 bytes of the size
    char buffer[len];
    FILE *sendFile = fopen(filename, "r");
    if(sendFile == NULL)
    {
        cerr<<"The file does not exist here or failed to open the file\n";
        close(connSocket);
        close(initialSockfd);
        exit(3);
    }
    bzero(buffer, len);
    int readBlockSize = 0, totalBytes = 0;
    while((readBlockSize = fread(buffer, sizeof(char), len, sendFile)) > 0)
    {
        totalBytes += readBlockSize;
        if(send(connSocket, buffer,readBlockSize, 0) < 0)   //only sending the num bytes read
        {
            cerr<<"Sending at the server failed\n";
            close(connSocket);
            close(initialSockfd);
            exit(-1);
        }
        bzero(buffer, len);  //again setting the buffer to be all zeros just in case
    }
    fclose(sendFile);
    close(connSocket);
    close(initialSockfd);
    cout<<"TransferDone: "<<totalBytes<<" bytes\n";
    return 0;
}