#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <fcntl.h>
#include <cerrno>


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
//custome write function to write into one file all at once
bool writeChunk(int outputFD, const char* buffer, size_t length, size_t offset) {
  size_t totalWritten =0 ;

  while(totalWritten<length){
    //allows chunk to be written at specific positions
    ssize_t bytesWritten = pwrite(
      outputFD,
      buffer+ totalWritten,
      length-totalWritten,
      offset+totalWritten
    );
    if(bytesWritten<0 && errno==EINTR){
      continue;
    }

    if(bytesWritten<=0){
      perror("pwrite failed");
      return false;

    }
    totalWritten +=bytesWritten;
  }
  return true;
}
bool downloadChunk(const string& serverIP, int port, size_t offset, size_t length, size_t chunkNumber, int outputFD) {
  //create socket endpoint
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

  if (clientSocket < 0) {
    perror("socket failed");
    return false;
  }

  // configure address
  sockaddr_in serverAddress{};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(port);

  if (inet_pton(AF_INET, serverIP.c_str(),
      &serverAddress.sin_addr)!=1){
    cout << "invalid ip" << endl;
    close(clientSocket);
    return false; 
  }

  //connect to server
  if(connect(clientSocket, (sockaddr*)&serverAddress,
    sizeof(serverAddress))<0){
    perror("connection failed");
    close(clientSocket);
    return false;
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
      return false;
    }

    totalSent += bytesSent;
  }
  cout << "Requested test.bin" << endl;

  string response;

  if (!recvLine(clientSocket, response)) {
    cerr << "failed to receive response" << endl;
    close(clientSocket);
    return false;
  }
  if (response != "OK " + to_string(length)) {
    cerr << "server error: " << response << endl;
    close(clientSocket);
    return false;
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
      return false;
    }

    totalReceived += bytesReceived;
  }

  if (!writeChunk(outputFD, buffer, totalReceived, offset)) {
    close(clientSocket);
    return false;
  }


  cout << "Downloaded chunk: "
    << chunkNumber
    << ": " << totalReceived << " bytes" << endl;
  close(clientSocket);
  return true;


}
//worker function for downloading chunks
void downloadWorker(int threadID, int threadCount, const string& serverIP, int port, size_t fileSize, size_t chunkSize, atomic<bool>& failed,int outputFD) {

  size_t totalChunks = (fileSize + chunkSize -1)/chunkSize;

  for(size_t chunkNumber =threadID; chunkNumber < totalChunks; chunkNumber +=threadCount){
    size_t offset = chunkNumber * chunkSize;
    size_t length =min(chunkSize, fileSize-offset);
    cout << "Thread " << threadID << " downloading chunk " << chunkNumber << endl;
    if (!downloadChunk(serverIP, port, offset, length, chunkNumber,outputFD)) {
      failed = true;
      return;
    }
  }
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

  const int threadCount=3;

  int outputFD = open("downloaded_test.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);

  if (outputFD < 0) {
    perror("failed to create output file");
    return 1;
  }

  if (ftruncate(outputFD, fileSize) == -1) {
    perror("failed to resize output file");
    close(outputFD);
    return 1;
  }

  atomic<bool> failed(false);

  vector<thread>workers;

  //create thread for each worker
  for(int i =0; i<threadCount;i++){
    workers.emplace_back(downloadWorker, i, threadCount,
    cref(serverIP), port, fileSize,chunkSize,ref(failed), outputFD);

  }

  for(auto& worker:workers){
    worker.join();
  }

  if (close(outputFD) == -1) {
    perror("failed to close output file");
    return 1;
  }

  if(failed){
    cerr<< "one or more chunks failed"<<endl;
    return 1;
  }

  
  cout << "download and merging complete" << endl;


  return 0;
}
