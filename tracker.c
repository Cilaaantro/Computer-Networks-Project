#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "messages.h"
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

//prevent 2 workers from creating or modifying tracker file simultaneously
//for managing concurrency
pthread_mutex_t trackerMutex = PTHREAD_MUTEX_INITIALIZER;

//global variable for storing the tracker directory
char trackerDir[256];

//helper to send message to peer
int sendAll(int sock, const char *message){
    size_t total = 0;
    size_t length = strlen(message);

    while (total < length){
        ssize_t sent = send(
            sock,
            message + total,
            length - total,
            0
        );

        if(sent<0 && errno == EINTR)
            continue;

        if (sent<= 0)
            return -1;

        total += (size_t)sent;
    }

    return 0;
}

void handleList(int clientSock){
    //create a new directory for torrents
    DIR *directory;
    struct dirent *entry;

    char filenames[100][256];
    char filesizes[100][64];
    char md5s[100][64];

    int count = 0;

    pthread_mutex_lock(&trackerMutex);

    directory = opendir(trackerDir);

    if(directory==NULL){
        perror("opendir");
        pthread_mutex_unlock(&trackerMutex);
        return;
    }

    //add info of trackers to directory
    while((entry = readdir(directory)) != NULL){
        const char *name = entry->d_name;
        size_t len = strlen(name);

        if(len < 6 || strcmp(name +len - 6, ".track") != 0)
            continue;

        if(count >= 100)
            break;

        char path[1024];

        snprintf(path, sizeof(path), "%s/%s", trackerDir, name);

        FILE *file = fopen(path, "r");
        //check if file opened successfuly
        if (file == NULL)
            continue;

        char filename[256];
        char filesize[64];
        char description[256];
        char md5[64];

        int parsed = fscanf(
            file,
            "Filename: %255s\n"
            "Filesize: %63s\n"
            "Description: %255s\n"
            "MD5: %63s\n",
            filename,
            filesize,
            description,
            md5
        );

        //close the file after reading info
        fclose(file);

        if(parsed !=4)
            continue;

        //copy info into variables
        strcpy(filenames[count], filename);
        strcpy(filesizes[count], filesize);
        strcpy(md5s[count], md5);

        count++;
    }

    closedir(directory);

    pthread_mutex_unlock(&trackerMutex);

    char response[512];

    snprintf(response, sizeof(response), "<REP LIST %d>\n", count);

    //send response to peer
    if(sendAll(clientSock, response) < 0)
        return;

    for(int i =0; i < count; i++){
        snprintf(response, sizeof(response), "<%d %s %s %s>\n", i+1, filenames[i], filesizes[i], md5s[i]);

        if(sendAll(clientSock, response)< 0)
            return;
    }

    sendAll(clientSock, RESP_LIST_TRACKERS_END);

    printf("List of %d trackers sent.\n", count);
}

void handleCreateTracker(int clientSock, const char *message){
    char filename[256];
    char filesize[64];
    char description[256];
    char md5[64];
    char ip[64];
    char port[16];

    //parse out the arguments from the message
    int parsed = sscanf(message, "<createtracker %255s %63s %255s %63s %63s %15[^>]>", filename, filesize, description, md5, ip, port);
    
    if(parsed != 6){
        //send failure msg if parsing failed
        send(clientSock, RESP_CREATE_TRACKER_FAILURE, strlen(RESP_CREATE_TRACKER_FAILURE), 0);
        return;
    }

    //send failure if filename is unsafe
    if(strstr(filename, "..") != NULL || strchr(filename, '/') != NULL){
        send(clientSock, RESP_CREATE_TRACKER_FAILURE, strlen(RESP_CREATE_TRACKER_FAILURE), 0);
    }

    char path[1024];

    snprintf(path, sizeof(path), "%s/%s.track", trackerDir , filename);
    
    pthread_mutex_lock(&trackerMutex);

    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);

    if(fd < 0){
        pthread_mutex_unlock(&trackerMutex);
    

    const char *response = 
        (errno == EEXIST) ? RESP_CREATE_TRACKER_FILE_EXISTS : RESP_CREATE_TRACKER_FAILURE;

    send(clientSock, response, strlen(response), 0);
    return;
    }

    //write info of the new tracker to the file
    FILE *file = fdopen(fd, "w");

    //check if file was opened properly
    if(file == NULL){
        close(fd);
        unlink(path);

        pthread_mutex_unlock(&trackerMutex);

        send(clientSock, RESP_CREATE_TRACKER_FAILURE, strlen(RESP_CREATE_TRACKER_FAILURE), 0);
        return;
    }

    time_t currTime = time(NULL);

    int written = fprintf(
        file,
        "Filename: %s\n"
        "Filesize: %s\n"
        "Description: %s\n"
        "MD5: %s\n"
        "%s:%s:0:%s:%lld\n",
        filename,
        filesize,
        description,
        md5,
        ip,
        port,
        filesize,
        (long long)currTime
    );

    int resultClose =fclose(file);

    //check if writting happened successfully
    if(written < 0|| resultClose !=0){
        unlink(path);
        pthread_mutex_unlock(&trackerMutex);

        send(clientSock, RESP_CREATE_TRACKER_FAILURE, strlen(RESP_CREATE_TRACKER_FAILURE), 0);
        return;
    }

    pthread_mutex_unlock(&trackerMutex);

    send(clientSock, RESP_CREATE_TRACKER_SUCCESS, strlen(RESP_CREATE_TRACKER_SUCCESS), 0);

    printf("Tracker created: %s\n", path);
}

void handleGet(int clientSock, const char * message){

    char filename[256];
    char path[1024];

    //get the filename and make sure it's valid
    if (sscanf(message, "<GET %255[^>]>", filename) != 1){
        printf("Invalid GET request.\n");
        return;
    }

    if(strstr(filename, "..") != NULL || strchr(filename, '/') != NULL || strchr(filename, '\\')){
        printf("Invalid tracker filename.\n");
        return;
    }

    //ensure the filename ends with .track
    size_t length = strlen(filename);

    if(length < 6 || strcmp(filename + length - 6, ".track") != 0){
        printf("Invalid tracker format.\n");
        return;
    }

    snprintf(path, sizeof(path), "%s/%s", trackerDir, filename);

    pthread_mutex_lock(&trackerMutex);

    FILE *file = fopen(path, "r");

    if(file == NULL){
        pthread_mutex_unlock(&trackerMutex);
        perror("fopen");
        return;
    }

    char line[1024];
    char md5[64] = "";

    //find original md5
    while(fgets(line, sizeof(line), file) != NULL){
        if(strncmp(line, "MD5: ", 5) == 0){
            sscanf(line + 5, "%63s", md5);
            break;
        }
    }

    if(md5[0] == '\0'){
        fclose(file);
        pthread_mutex_unlock(&trackerMutex);
        printf("Tracker has no MD5.\n");
        return;
    }

    //go to beginning of the file
    rewind(file);

    //send file contents to peer
    if(sendAll(clientSock, RESP_GET_TRACKER_BEGIN) < 0){
        fclose(file);
        pthread_mutex_unlock(&trackerMutex);
        return;
    }

    while(fgets(line, sizeof(line), file) != NULL){
        if(sendAll(clientSock, line) < 0){
            fclose(file);
            pthread_mutex_unlock(&trackerMutex);
            return;
        }
    }

    fclose(file);
    pthread_mutex_unlock(&trackerMutex);

    char reply[128];

    snprintf(reply, sizeof(reply), RESP_GET_TRACKER_END, md5);

    if(sendAll(clientSock, reply) < 0)
        return;

    printf("Sent tracker file: %s\n", filename);
}

void handleUpdateTracker(int clientSock, const char *message){
    char filename[256];
    char start[64];
    char end[64];
    char ip[64];
    char port[16];

    int parsed = sscanf(message, "<updatetracker %255s %63s %63s %63s %15[^>]>", filename, start, end, ip, port);

    //check if the update tracker is valid
    if (parsed != 5 || strstr(filename, "..") != NULL || strchr(filename, '/') || strchr(filename, '\\') != NULL){
        sendAll(clientSock, "Invalid update tracker.\n");
        return;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.track", trackerDir, filename);

    pthread_mutex_lock(&trackerMutex);

    FILE *file = fopen(path, "r");

    //check if there is a tracker being updated exists
    if(file==NULL){
        pthread_mutex_unlock(&trackerMutex);

        char reply[512];
        snprintf(reply, sizeof(reply), RESP_UPDATE_TRACKER_MISSING, filename);

        sendAll(clientSock, reply);
        return;
    }

    //read the tracker file
    char lines[200][1024];
    int count = 0;

    while(count < 200 && fgets(lines[count], sizeof(lines[0]), file) != NULL){
        count++;
    }

    fclose(file);

    //find a peer that has the same IP and port
    int find = 0;
    time_t theTime = time(NULL);

    for(int i =4; i<count; i++){
        char currentIP[64];
        char currentPort[16];

        if(sscanf(lines[i], "%63[^:]:%15[^:]", currentIP, currentPort) != 2){
            continue;
        }
        if (strcmp(currentIP, ip) == 0 && strcmp(currentPort, port) == 0){
            snprintf(lines[i], sizeof(lines[i]), "%s:%s:%s:%s:%lld\n", ip, port, start, end, (long long)theTime);

            find = 1;
            break;
        }
    }

    //create a new peer if one does not exist with the same IP and port
    if(!find){
        if(count >= 200){
            pthread_mutex_unlock(&trackerMutex);

            char reply[512];

            snprintf(reply, sizeof(reply), RESP_UPDATE_TRACKER_FAILURE, filename);

            sendAll(clientSock, reply);
            return;
        }

        snprintf(lines[count], sizeof(lines[count]), "%s:%s:%s:%s:%lld\n", ip, port, start, end, (long long)theTime);

        count++;
    }

    //rewrite the tracker file to have the new peer
    file = fopen(path, "w");

    if(file == NULL){
        pthread_mutex_unlock(&trackerMutex);

        char reply[512];
        snprintf(reply, sizeof(reply), RESP_UPDATE_TRACKER_FAILURE, filename);

        sendAll(clientSock, reply);
        return;
    }

    int fail = 0;

    for(int i = 0; i< count; i++){
        if(fputs(lines[i], file) == EOF){
            fail = 1;
            break;
        }
    }

    if(fclose(file) != 0){
        fail = 1;
    }
    
    pthread_mutex_unlock(&trackerMutex);

    char reply[512];

    if(fail){
        snprintf(reply, sizeof(reply), RESP_UPDATE_TRACKER_FAILURE, filename);
    }
    else{
        snprintf(reply, sizeof(reply), RESP_UPDATE_TRACKER_SUCCESS, filename);
    }

    sendAll(clientSock, reply);

    if(!fail){
        printf("Tracker was updated: %s\n", filename);
    }
}

void processCommand(int clientSock, const char *message){
    if(strncmp(message, "<createtracker ", 15) == 0){
        // Handle the create tracker command
        printf("CREATE TRACKER command recieved.\n");

        handleCreateTracker(clientSock, message);

    } else if(strncmp(message, "<updatetracker ", 15) == 0){
        // Handle update tracker command
        printf("UPDATE TRACKER command received.\n");

        handleUpdateTracker(clientSock, message);

    } else if(strncmp(message, "<REQ LIST>\n", 11) == 0 || strcmp(message, "<REQ LIST>") == 0){
        // Handle list trackers command
        printf("LIST command received.\n");

        handleList(clientSock);

    } else if(strncmp(message, "<GET ", 5) == 0){
        // Handle get tracker command
        printf("GET command received.\n");

        handleGet(clientSock, message);
    } else {
        // handle an Unknown command
        printf("Received unknown command: %s\n", message);
    }
}

//function each new thread will run to handle client connection
void *handleClient(void  *arg){
    //convert pointer into integer socket descriptor
    int clientSock = *(int *)arg;
    free(arg);

    //return id of the current thread
    printf("Worker thread %lu started.\n", (unsigned long)pthread_self());

    char buffer[2048];

    //recieve data sent by connected peer
    ssize_t bytesRecv = recv(clientSock, buffer, sizeof(buffer) - 1, 0);

    if(bytesRecv > 0){
        buffer[bytesRecv] = '\0';

        printf("Worker thread %lu received: %s\n", (unsigned long)pthread_self(), buffer);

        processCommand(clientSock, buffer);
    }
    else if(bytesRecv == 0){
        printf("Peer disconnected.\n");
    }
    else{
        perror("recv");
    }

    //close connection with peer after finished
    close(clientSock);

    printf("Worker thread %lu done.\n", (unsigned long)pthread_self());

    return NULL;
}

//function to read the sconfig file
int handleConfig(int *port, char *directory, size_t dirSize){
    FILE *file = fopen("sconfig", "r");

    if(file == NULL){
        perror("Cannot open sconfig");
        return -1;
    }

    char line[128];
    char extra;

    //read and check if the port is valid
    if(fgets(line, sizeof(line), file) == NULL || sscanf(line, " %d %c", port, &extra) != 1 || *port < 1 || *port > 65535){
        fprintf(stderr, "Invalid port in sconfig\n");
        fclose(file);
        return -1;
    }

    //read the directory and check if it exists
    if(fgets(line, sizeof(line), file) == NULL){
        fprintf(stderr, "Missing directory in sconfig\n");
        fclose(file);
        return -1;
    }

    line[strcspn(line, "\r\n")] = '\0';

    //check if the directory is valid
    if(line[0] == '\0' || strlen(line) >= dirSize){
        fprintf(stderr, "Invalid directory in sconfig\n");
        fclose(file);
        return -1;
    }

    strcpy(directory, line);

    fclose(file);
    return 0;
}

int main(void){
    //initialize variables for server and client sockets
    int servSock;
    int clientSock;

    //initialize server address structure
    struct sockaddr_in servAddr;

    //create TCP socket
    servSock = socket(AF_INET, SOCK_STREAM, 0);

    //if socket creation failed print error
    if(servSock<0){
        perror("socket");
        return 1;
    }

    // set socket option to reuse address
    int opt = 1;

    //if setsockopt fails, print error and close the socket
    if (setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(servSock);
        return 1;
    }

    //port read from sconfig
    int port;

    if(handleConfig(&port, trackerDir, sizeof(trackerDir)) != 0){
        return 1;
    }

    printf("Configuration complete:\n");
    printf("Port: %d\n", port);
    printf("Directory: %s\n", trackerDir);

    //initialize server address
    memset(&servAddr, 0, sizeof(servAddr));

    //set up server address
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(port);

    //bind socket to the specified port
    if(bind(servSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0){
        perror("bind");
        close(servSock);
        return 1;
    }

    //listen for connections
    if(listen(servSock, 10) < 0){
        perror("listen");
        close(servSock);
        return 1;
    }

    printf("Tracker listening on port %d\n", port);

    while(1){
        clientSock = accept(servSock, NULL, NULL);
        //check if accept failed
        if(clientSock < 0){
            if(errno == EINTR) continue; // Interrupted by signal, retry
            
            perror("accept");
            continue;
        }

        printf("Peer Connected.\n");

        //allocate memory for the client socket
        int *clientArg = malloc(sizeof(int));

        //if it is null, then memory allocation failed, close the socket
        if(clientArg == NULL){
            perror("malloc");
            close(clientSock);
            continue;
        }

        //store peer socket in allocated memory
        *clientArg = clientSock;

        //create new thread to handle peer connection
        pthread_t thread;

        int result = pthread_create(&thread, NULL, handleClient, clientArg);

        //if thread creation failed, print error and close the socket
        if (result != 0){
            errno = result;
            perror("pthread_create");

            free(clientArg);
            close(clientSock);
            continue;
        }

        //detatch the thread after so it can clean up when done
        result = pthread_detach(thread);

        //if detaching the thread failed, print error
        if(result != 0){
            errno = result;
            perror("pthread_detach");
        }
    }

    //close the server socket before exiting
    close(servSock);
    return 0;
}