#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <cstdlib>
#include <stdio.h>
#include <unistd.h>
#include <map>
#include <queue>

using namespace std;


//This server can support multiple clients simultaneously

//also keeping the chunk size low here (less than 1000B )
//as the send to the socket may block for a long time due to slow client
//the send is blocking till the whole message is copied into the send buffer

//this maintains the state of each connection, (i.e how may bytes
//have been sent, the File desciptor and is added when new client
//asks for a file and is removed when the file is sent and the
//connection is closed
struct State
{
    public:
    int numBytesWritten;  //the number of bytes written
    FILE* sendFiles;  //file to be sent
    int sockfd;   //the file descriptor on which the file is written
};
map<int, State*> table;

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

int transferFile(int sock)
{
    int len = 1000;   //sending 1024 bytes of the size
    char buffer[len];
    bzero(buffer, len);
    
    FILE* file = table[sock]->sendFiles;
    int readBlockSize = fread(buffer, sizeof(char), len, file);
    if(readBlockSize > 0)
    {
        table[sock]->numBytesWritten += readBlockSize;
        if(send(sock, buffer,readBlockSize, 0) < 0)   //only sending the num bytes read
        {
            cout<<"Sending at the server failed\n";
            fclose(file);
            close(sock);
            map<int, State*>::iterator it = table.find(sock);
            delete it->second;  //delete the state space allocated via new
            table.erase(it);
            return 2;
        }
    }
    if(readBlockSize != len)  //then the file is probably read totally
    {
        int totalBytes = table[sock]->numBytesWritten;
        if(feof(file) != 0)  //means end of file reached
        {
            cout<<"TransferDone: "<<totalBytes<<" bytes\n";
        }
        else
        {
            cout<<"Reading failed after "<<totalBytes<<" bytes\n";
        }
        //erase the state from table after closing the file and the socket
        fclose(file);
        close(sock);
        map<int, State*>::iterator it = table.find(sock);
        delete it->second;  //delete the state space allocated via new
        table.erase(it);
        return 2;
    }
    return 1;
}


//whenever the server gets file request, this function is to be called
//returns 0 after new state is created succesfully
//return 1 if unknown command
//return 2 is the file doesnot exist
int newRequest(int connSocket)
{
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
        return 1;
    }
    FILE *sendFile = fopen(filename, "r");
    delete filename;   //this was allocated inside the function getName
    if(sendFile == NULL)
    {
        //this means the file is not present or file cannot be read
        cout<<"FileTransferFail\n";
        cerr<<"The file does not exist here or failed to open the file\n";
        close(connSocket);
        return 2;
    }
    //now create new state
    State* state = new State;
    state->sendFiles = sendFile ;
    state->sockfd = connSocket;
    state->numBytesWritten = 0;
    table[connSocket] = state;

    return 0;
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

    fd_set readFds;  //the main initialSockfd and the new clientfds will be here
    fd_set writeFds;
    fd_set tempRead, tempWrite;
    int fdMax;  //for storing the max
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_SET(initialSockfd, &tempRead);
    fdMax = initialSockfd;

    while(true)
    {
        readFds = tempRead;
        writeFds = tempWrite;
        if (select(fdMax + 1, &readFds, &writeFds, NULL, NULL) == -1)
        {
            continue;
        }
        if(FD_ISSET(initialSockfd, &readFds))
        {//we have got a new incoming connection from a client
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
            if (connSocket > fdMax)
            {
                fdMax = connSocket;
                //cout<<"FD MAX is "<<fdMax<<'\n';
            }
            FD_SET(connSocket, &tempRead);
        }
        for(int i = 0; i <= fdMax; i++)
        {
            if(i == initialSockfd)
            {//we have already covered that case now
                continue;
            }
            if(FD_ISSET(i, &readFds))
            {//we have a client requesting for a file
                FD_CLR(i, &tempRead);
                if(newRequest(i) == 0)
                {
                    FD_SET(i, &tempWrite);
                }
            }
            if(FD_ISSET(i, &writeFds))
            {
                if(table.count(i) != 0) //if there is something in the table
                {
                    if(transferFile(i) == 2)
                    {
                        FD_CLR(i, &tempWrite);
                    }
                }
            }
        }
    }
    close(initialSockfd);
    return 0;
}