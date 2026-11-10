#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std;

int main() {
  ifstream config("serverThreadConfig.cfg");

  int port;
  string sharedFolder;

  if(!(config>>port>>sharedFolder) ||
    port<1024 || port >65535){
      cerr << "invalid server config";
      return 1;
  }
  
  // create socket endpoint
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

  if (serverSocket == -1) {
      perror("socket failed");
      return 1;
  }

  // setup server's address
  sockaddr_in serverAddress{};

  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

  //bind server's adress
  if(bind(serverSocket, (sockaddr*)&serverAddress,
    sizeof(serverAddress)) ==1){
      perror("bind error");
      close(serverSocket);
      return 1;
  }

  // listen for any connection request
  if(listen(serverSocket,5) == -1){
    perror("listen error");
    close(serverSocket);
    return 1;
  }

  cout << "server listening on port "<< port << endl;

  // accept incoming request
  int clientSocket = accept(serverSocket, nullptr, nullptr);

  if (clientSocket == -1) {
      perror("failed to accept client request");
      close(serverSocket);
      return 1;
  }

  cout << "client connected" << endl;

  close(clientSocket);
  close(serverSocket);

  

  return 0;
}