#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std;
// custom send function that keeps sending until all bytes are sent
bool sendBytes(int socketFD, const char* data, size_t length){
  size_t totalSent = 0;

  // doesnt stop loop until all bytes are sent
  while(totalSent < length){
    ssize_t bytesSent = send(
      socketFD,
      data + totalSent,
      length - totalSent,
      MSG_NOSIGNAL
    );
    if(bytesSent <= 0){
      return false;
    }
    totalSent +=bytesSent;
  }
  return true;
}
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

  string request;
  char ch;

  //get request name
  while(request.size() <100){
    ssize_t bytesReceived = recv(clientSocket, &ch, 1, 0);

    if(bytesReceived <=0){
      break;
    }
    if( ch == '\n'){
      break;
    }
    request += ch;
  }
  cout << "Request received: " << request << endl;

  if( request != "GET test.txt"){
    cerr << "invalid file test" <<endl;
    close(clientSocket);
    close(serverSocket);
    return 1;

  }

  //open file
  ifstream file(sharedFolder + "/test.txt", ios::binary);

  if (!file.is_open()) {
    cerr << "Cant open file" << endl;
    close(clientSocket);
    close(serverSocket);
    return 1;
  }

  char buffer[1024];
  //send file
  while(file){
    //reads 1024 bytes at a time
    file.read(buffer, sizeof(buffer));

    //get actual bytes red
    streamsize bytesRead = file.gcount();

    if(bytesRead > 0){
      if(!sendBytes(clientSocket, buffer, bytesRead)){
        cerr<<"error sending file"<<endl;
        close(clientSocket);
        close(serverSocket);
        return 1;
      }
    }
  }

  close(clientSocket);
  close(serverSocket);

  

  return 0;
}