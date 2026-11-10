#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>


using namespace std;


int main(int argc, char* argv[]) {

  if (argc != 3){
    cout<< "invalid ammount of arguments"<<endl;
    return 1;
  }

  const char* serverIP = argv[1];
  int port = stoi(argv[2]);

  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

  if (clientSocket < 0) {
    perror("socket failed");
    return 1;
  }

  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);

  if (inet_pton(AF_INET, serverIP,
      &serverAddress.sin_addr)!=1){
    cout << "invalid ip" << endl;
    close(clientSocket);
    return 1; 
  }

  if(connect(clientSocket, (sockaddr*)&serverAddress,
    sizeof(serverAddress))<0){
    perror("connection failed");
    close(clientSocket);
    return 1;
  }

  cout<< "connected to peer server" << endl;

  close(clientSocket);

  return 0;
}
