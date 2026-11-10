#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


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
  string request = "GET test.bin 0 1024\n";
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
  if (response != "OK 1024") {
    cerr << "server error: " << response << endl;
    close(clientSocket);
    return 1;
  }

  char buffer[1024];
  size_t totalReceived =0;

  while( totalReceived <1024){
    ssize_t bytesReceived = recv(
      clientSocket,
      buffer + totalReceived,
      1024-totalReceived,
      0
    );

    if(bytesReceived <=0){
      cerr<< "chunk download failed"<<endl;
      close(clientSocket);
      return 1;
    }

    totalReceived += bytesReceived;
  }

  ofstream outputFile("chunk_0.bin", ios::binary);

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
    << totalReceived << " bytes" << endl;
  close(clientSocket);



  return 0;
}
