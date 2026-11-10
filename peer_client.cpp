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
  close(clientSocket);

  return 0;
}
