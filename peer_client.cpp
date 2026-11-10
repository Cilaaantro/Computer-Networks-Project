#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>


using namespace std;

bool recvLine(int socketFD, string& line){
  char ch;
  while(line.size() <256){
    ssize_t bytesReceived = recv(socketFD, &ch,1,0);

    if(bytesReceived<=0){
      return false;
    }

    if(ch == '\n'){
      return true;
    }
    line +=ch;
  }
  return false;
}
int main() {

  ifstream config("peerDownloadConfig.cfg");


  string serverIP;
  int port;

  if (!(config >> serverIP >> port) ||
    port < 1 || port > 65535) {
    cerr << "invalid config";
    return 1;
  }

  const size_t fileSize = 2500;
  const size_t chunkSize = 1024;

  size_t chunkNumber = 0;

  //loop chunk by chunk untill all chunks are transfered
  for (size_t offset = 0; offset < fileSize; offset += chunkSize) {
    size_t length = min(chunkSize, fileSize - offset);
  

    //create socket endpoint
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket < 0) {
      perror("socket failed");
      return 1;
    }

    // configure address
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIP.c_str(),
        &serverAddress.sin_addr)!=1){
      cout << "invalid ip" << endl;
      close(clientSocket);
      return 1; 
    }

    //connect to server
    if(connect(clientSocket, (sockaddr*)&serverAddress,
      sizeof(serverAddress))<0){
      perror("connection failed");
      close(clientSocket);
      return 1;
    }

    cout<< "connected to peer server" << endl;

  

    //request message
    string request = "GET test.bin "  +
                 to_string(offset) + " " +
                 to_string(length) + "\n";
    size_t totalSent = 0;

    //keep sending request until its fully sent
    while (totalSent < request.size()) {
      ssize_t bytesSent = send(
        clientSocket,
        request.data() + totalSent,
        request.size() - totalSent,
        MSG_NOSIGNAL
      );
      if (bytesSent <= 0) {
        perror("send failed");
        close(clientSocket);
        return 1;
      }

      totalSent += bytesSent;
    }
    cout << "Requested test.bin" << endl;

    string response;

    if (!recvLine(clientSocket, response)) {
      cerr << "failed to receive response" << endl;
      close(clientSocket);
      return 1;
    }
    if (response != "OK " + to_string(length)) {
      cerr << "server error: " << response << endl;
      close(clientSocket);
      return 1;
    }

    char buffer[1024];
    size_t totalReceived =0;

    while( totalReceived <length){
      ssize_t bytesReceived = recv(
        clientSocket,
        buffer + totalReceived,
        length-totalReceived,
        0
      );

      if(bytesReceived <=0){
        cerr<< "chunk download failed"<<endl;
        close(clientSocket);
        return 1;
      }

      totalReceived += bytesReceived;
    }

    string outputName = "chunk_" + to_string(chunkNumber) + ".bin";
    ofstream outputFile(outputName, ios::binary);

    if (!outputFile.is_open()) {
      cerr << "chunk file failed to create" << endl;
      close(clientSocket);
      return 1;
    }
    outputFile.write(buffer, totalReceived);
    outputFile.close();

    if (!outputFile) {
      cerr << "failed to save chunk" << endl;
      close(clientSocket);
      return 1;
    }

    cout << "Downloaded chunk: "
     << chunkNumber
     << ": " << totalReceived << " bytes" << endl;
    close(clientSocket);
    chunkNumber++;

  }

  cout << "all chunks downloaded" << endl;


  return 0;
}
