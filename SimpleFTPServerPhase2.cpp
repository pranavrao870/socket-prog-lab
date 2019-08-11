#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <cstdlib>
#include <stdio.h>
#include <unistd.h>

using namespace std;

//same as server phase 1, but the program exits only when the
//bind fails, socket create fails or wrong cmmd line args


//returns -1 if the sending fails else returns total number
//of bytes sent
int transferFile(int sock, FILE* file)
{
    int len = 1024;   //sending 1024 bytes of the size
    char buffer[len];
    bzero(buffer, len);
    int readBlockSize = 0, totalBytes = 0;
    while((readBlockSize = fread(buffer, sizeof(char), len, file)) > 0)
    {
        totalBytes += readBlockSize;
        if(send(sock, buffer,readBlockSize, 0) < 0)   //only sending the num bytes read
        {
            cout<<"Sending at the server failed\n";
            return -1;
        }
        bzero(buffer, len);  //again setting the buffer to be all zeros just in case
    }
    return totalBytes;
}

//returns false if the command is unknown(without any get prefix)
//else true
bool getName(int sock, char** filename)
{
    //now assuming that the filename wont be more than 1019 char
    //'get ' will have 4 char and the last char is '\0'
    int len = 1024;
    char buffer[1024];
    int valread = recv(sock , buffer, len, 0);
    if(buffer[0] == 'g' && buffer[1] == 'e' && buffer[2] == 't')
    {
        *filename = new char[1024];
        int i = 4;
        while(buffer[i] != '\0')
        {
            (*filename)[i - 4] = buffer[i];
            i++;
        }
        (*filename)[i - 4] = buffer[i];
        return true;
    }
    else
    {
        return false;
    }
}

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        cerr<<"Usage: executable PortNum \n";
        exit(1);
    }

    int serverPort = atoi(argv[1]);

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

    //setting our initial socket as passive to accept the incoming client
    //connections
    if(listen(initialSockfd, 5) == -1)   //keeping the tcp buffer as 5
    {
        cerr<<"listen failed\n";
        exit(-1);
    }
    else
    {
        cout<<"ListenDone: "<<serverPort<<endl;
    }

    while(true)
    {
        int connSocket;
        struct sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        if((connSocket = accept(initialSockfd, (struct sockaddr*)&clientAddr, (socklen_t*)&clientAddrLen)) < 0)
        {
            cerr<<"Accept failed\n";
            continue;
        }
        else
        {
            //convert the 32 bits ip address into human readabe ip address and also
            //network ordering to host ordering
            cout<<"Client: "<<inet_ntoa(clientAddr.sin_addr)<<':'<<ntohs(clientAddr.sin_port)<<'\n';
        }


        char* filename;
        bool success = getName(connSocket, &filename);
        if(success)
        {
            cout<<"FileRequested: "<<filename<<'\n';
        }
        else
        {
            cout<<"UnknownCmd\n";
            cerr<<"Unknown command from the client side\n";
            close(connSocket);
            continue;
        }

        FILE *sendFile = fopen(filename, "r");
        delete filename;   //this was allocated inside the function getName
        if(sendFile == NULL)
        {
            //this means the file is not present or file cannot be read
            //and so continue further
            cout<<"FileTransferFail\n";
            cerr<<"The file does not exist here or failed to open the file\n";
            close(connSocket);
            continue;
        }
        int totalBytes = transferFile(connSocket, sendFile);
        fclose(sendFile);
        close(connSocket);
        //the data was sent to the client successfully
        if (totalBytes != -1)
        {
            cout<<"TransferDone: "<<totalBytes<<" bytes\n";
        }
    }
    close(initialSockfd);
    return 0;
}