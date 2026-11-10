#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <thread>

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

//receive request
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
void handleClientRequest(int clientSocket, const string& sharedFolder){
  string request;
  if(!recvLine(clientSocket, request)){
    cerr<<"failed to receive request" <<endl;
    return;
  }
  cout << "receieved request: "<<request<<endl;
  
  istringstream parser(request);

  string command;
  string filename;
  long long offset;
  long long length;
  if (!(parser >> command >> filename >> offset >> length)) {
    cerr << "invalid request format" << endl;
    return;
  }

  string extra;
  //validate rerquest
  if (command != "GET" ||
    (parser >> extra) ||
    filename.empty() ||
    filename.find('/') != string::npos ||
    filename.find('\\') != string::npos ||
    filename.find("..") != string::npos ||
    offset < 0 ||
    length <= 0 ||
    length > 1024) {

    string error = "<GET invalid>\n";

    sendBytes(clientSocket, error.c_str(), error.size());

    return;
  }
  ifstream file(sharedFolder + "/" + filename, ios::binary);
  if (!file.is_open()) {
    cerr << "Cant open fiile" << endl;
    string error = "<GET invalid>\n";
    sendBytes(clientSocket, error.c_str(), error.size());
    return;
  }

  //go to end of file to determine file size
  file.seekg(0, ios::end);
  streamoff fileSize = file.tellg();

  //make sure bytes exist
  if (fileSize < 0 ||
    offset > fileSize ||
    length > fileSize - offset) {

    string error = "<GET invalid>\n";
    sendBytes(clientSocket, error.c_str(), error.size());
    return;
  } 

  file.seekg(offset, ios::beg);

  char buffer[1024];

  file.read(buffer, length);

  if (file.gcount() != length) {
    cerr << "chunk read fail" << endl;
    return;
  }
  
  //send requested chunk
  string header = "OK " + to_string(length)+ "\n";

  if(!sendBytes(clientSocket, header.c_str(), header.size())){
    cerr << "Failed to send header response"<<endl;
    return;
  }

  if(!sendBytes(clientSocket, buffer, length)){
    cerr << "failed to send chunk"<<endl;
    return;
  }
  cout<< "sent" << length << " bytes starting at " << offset <<endl;



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
    sizeof(serverAddress)) ==-1){
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


  while (true){
    int clientSocket = accept(serverSocket, nullptr ,nullptr);
    if (clientSocket < 0) {
    perror("request accept failed");
    continue;
    }

    cout << "client connected" << endl;

    //makes server multi threaded
    thread([clientSocket, sharedFolder]() {

      handleClientRequest(clientSocket, sharedFolder);

      close(clientSocket);

    }).detach();


  }
  return 0;

}