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
    bool toSend;  //if we are sending a file over tcp then this is set to 1
    int numBytesWritten;  //the number of bytes written
    FILE* transFile;  //file to be sent or received
    int sockfd;   //the file descriptor on which the file is written
};
map<int, State*> table;

//parses the incoming command
//if it is get filename, then returns 1
//is it is put filename, then returns 2
//else returns -1
int commandParse(char *buffer, char *filename, char *rem, int *commLen)
{
    //now assuming that the filename wont be more than 1019 char
    //'get ' will have 4 char and the last char is '\0'
    if(buffer[0] == 'g' && buffer[1] == 'e' && buffer[2] == 't')
    {
        int i = 4;
        while(buffer[i] != '\0')
        {
            filename[i - 4] = buffer[i];
            i++;
        }
        filename[i - 4] = buffer[i];
        rem = NULL;
        *commLen = (i + 1);  //the total length of the command is here
        return 1;
    }
    else if (buffer[0] == 'p' && buffer[1] == 'u' && buffer[2] == 't')
    {
        int i = 4;
        while(buffer[i] != '\0')
        {
            filename[i - 4] = buffer[i];
            i++;
        }
        filename[i - 4] = buffer[i];
        rem = &(filename[i+1]);
        *commLen = (i + 1);
        return 2;
    }
    else
    {
        // cout<<'\n'<<buffer<<'\n';
        return -1;
    }
}

int transferFile(int sock)
{
    int len = 1000;   //sending 100 bytes of the size
    char buffer[len];
    bzero(buffer, len);

    if(table[sock]->toSend)
    {
        FILE* file = table[sock]->transFile;
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
    }
    else
    {
        FILE* file = table[sock]->transFile;
        int valread = recv(sock, buffer, len, 0); //note that the recv will return 0 on reading
                                                  //a socket that is closed(or less than something)
                                                  //will check it with opening a connection and closing it
        if(valread > 0)
        {
            table[sock]->numBytesWritten += valread;
            int lenWritten = fwrite(buffer, sizeof(char), valread, file);
            if(lenWritten != valread)
            {
                cerr<<"The File writing at server failed\n";
                fclose(file);
                close(sock);
                map<int,State*>::iterator it = table.find(sock);
                delete it->second;
                table.erase(it);
                return 2;
            }
        }
        else
        {
            cerr<<"Done putting the file at server :"<<table[sock]->numBytesWritten<<'\n';
            fclose(file);
            close(sock);
            map<int,State*>::iterator it = table.find(sock);
            delete it->second;
            table.erase(it);
            return 2;
        }
    }
    return 1;
}


//whenever the server gets file request, this function is to be called
//returns 0 after new state is created succesfully
//return 1 if unknown command
//return 2 is the file doesnot exist
//return 3 is the command is put
int newRequest(int connSocket)
{
    int len = 1024;
    int commLen;
    char *rem;
    char filename[len];
    char buffer[len];            //the rem will store the remaining data
                                 //the data after the command(in case of put)
                                 //command(in get command, it doesn't matter)
    int valread = recv(connSocket , buffer, len, 0);
    int retval = commandParse(buffer, filename, rem, &commLen);
    if(retval == 1)
    {
        cout<<"FileRequested: "<<filename<<'\n';

        FILE *sendFile = fopen(filename, "r");
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
        state->transFile = sendFile ;
        state->sockfd = connSocket;
        state->numBytesWritten = 0;
        state->toSend = true;
        table[connSocket] = state;
        return 0;
    }
    //now if the command is put
    else if (retval == 2)
    {
        FILE *recvFile = fopen(filename, "w");
        if (recvFile == NULL)
        {
            cerr<<"The file to be sent is "<<filename<<'\n';
            cerr<<"The command was put filename but file opening failed\n";
            close(connSocket);
            return 2;
        }
        if (valread != commLen)
        {
            fwrite(rem, sizeof(char), (valread - commLen), recvFile);
        }

        State* state = new State;
        state->transFile = recvFile;
        state->sockfd = connSocket;
        state->numBytesWritten = (valread - commLen);
        state->toSend = false;
        table[connSocket] = state;
        return 3;
    }
    else
    {
        cout<<"UnknownCmd\n";
        cerr<<"Unknown command from the client side\n";
        close(connSocket);
        return 1;
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

    fd_set readFds;  //the main initialSockfd and the new clientfds will be here
    fd_set writeFds;
    fd_set tempRead, tempWrite;
    map<int, bool> consFdsMap, tempFdsMap;
    int fdMax;  //for storing the max
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_SET(initialSockfd, &tempRead);
    tempFdsMap[initialSockfd] = true;
    fdMax = initialSockfd;

    while(true)
    {
        readFds = tempRead;
        writeFds = tempWrite;
        consFdsMap = tempFdsMap;
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
            tempFdsMap[connSocket] = true;
        }
        map<int, bool>::iterator it = consFdsMap.begin();
        for(; it != consFdsMap.end(); it++)
        {
            int i = it->first;
            if(i == initialSockfd)
            {//we have already covered that case now
                continue;
            }
            if(FD_ISSET(i, &readFds))
            {
                if(table.count(i) == 0)
                {//we have a new client who has not communicated yet
                    FD_CLR(i, &tempRead);
                    map<int, bool>::iterator it1 = tempFdsMap.find(i);
                    tempFdsMap.erase(it1);
                    int newret = newRequest(i);
                    if(newret == 0)
                    {
                        FD_SET(i, &tempWrite);
                        tempFdsMap[i] = true;
                    }
                    else if (newret == 3) //the command was put
                    {
                        FD_SET(i, &tempRead);
                        tempFdsMap[i] = true;
                    }
                }
                else
                {//the command was put earlier and now the file is being read
                    if(transferFile(i) == 2)
                    {//if the file reading is complete
                        FD_CLR(i, &tempRead);
                        map<int, bool>::iterator it1 = tempFdsMap.find(i);
                        tempFdsMap.erase(it1);
                    }
                }
            }
            if(FD_ISSET(i, &writeFds))
            {
                if(table.count(i) != 0) //if there is something in the table
                {
                    if(transferFile(i) == 2)
                    {//the writing is complete
                        FD_CLR(i, &tempWrite);
                        map<int, bool>::iterator it1 = tempFdsMap.find(i);
                        tempFdsMap.erase(it1);
                    }
                }
            }
        }
    }
    close(initialSockfd);
    return 0;
}