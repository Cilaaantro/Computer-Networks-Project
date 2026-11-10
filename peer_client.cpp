#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


using namespace std;


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

  //close connection
 

  //request message
  string request = "GET test.txt\n";
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
  cout << "Requested test.txt" << endl;

  //create file for download
  ofstream outputFile("downloaded_test.txt", ios::binary);

  if (!outputFile.is_open()) {
    cerr << "Cannot create output file" << endl;
    close(clientSocket);
    return 1;
  }

  char buffer[1024];
  ssize_t bytesReceived;
  size_t totalReceieved;

  // receive the file
  while ((bytesReceived = recv (
    clientSocket,
    buffer,
    sizeof(buffer),
  0))>0){
    //write received data to file
    outputFile.write(buffer, bytesReceived);
    if (!outputFile) {
      cerr << "Error writing file" << endl;
      close(clientSocket);
      return 1;
    }
    totalReceieved += bytesReceived;
    cout<< ""<< totalReceieved << "bytes received" <<endl;

  }
  if (bytesReceived < 0) {
    perror("recv failed");
    close(clientSocket);
    return 1;
  }

  outputFile.close();

  if (!outputFile) {
    cerr << "Error closing output file" << endl;
    close(clientSocket);
    return 1;
  }

  cout << "Download finished!" << endl;
  close(clientSocket);

  return 0;
}
