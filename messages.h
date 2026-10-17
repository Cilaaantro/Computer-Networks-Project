#ifndef MESSAGES_H
#define MESSAGES_H


// A request for a peer to create a tracker
// arguments: filename, filesize, description, md5, ip-address, port-number
#define REQ_CREATE_TRACKER      "<createtracker %s %s %s %s %s %s>\n"

// Response from tracker to peer for a tracker create request
#define RESP_CREATE_TRACKER_SUCCESS          "<createtracker succ>\n"
#define RESP_CREATE_TRACKER_FAILURE          "<createtracker fail>\n"
#define RESP_CREATE_TRACKER_FILE_EXISTS      "<createtracker ferr>\n"

// request by a peer to update a tracker
// arguments: filename, start_bytes, end_bytes, ip-address, port-number
#define REQ_UPDATE_TRACKER          "<updatetracker %s %s %s %s %s>\n"

// Response from tracker to peer for an update tracker request
#define RESP_UPDATE_TRACKER_SUCCESS        "<updatetracker %s succ>\n"
#define RESP_UPDATE_TRACKER_FAILURE        "<updatetracker %s fail>\n"
#define RESP_UPDATE_TRACKER_MISSING        "<updatetracker %s ferr>\n"

// peer request tracker server to list all tracker
#define REQ_LIST_TRACKERS                               "<REQ LIST>\n"

// tracker server listing tracker files to peer
// arugments: number of files. Item: index, filename, filesize, fileMD5
#define RESP_LIST_TRACKERS_BEGIN                      "<REP LIST %s>\n"
#define RESP_LIST_TRACKERS                            "<%s %s %s %s>\n"
#define RESP_LIST_TRACKERS_END                       "<REP LIST END>\n"

// get a specific tracker file
// arguments: tracker filename
#define REQ_GET_TRACKER                                    "<GET %s>\n"

// tracker sending one tracker file to peer
// arguments: tracker file content, MD5 
#define RESP_GET_TRACKER_BEGIN                       "<REP GET BEGIN>\n"
#define RESP_GET_TRACKER_END                         "<REP GET END %s>\n"

// Peer request a portion of the file from another peer. 1024 byte max limit
// arugments: filename, start byte, end byte
#define REQ_PEER_GET_FILE_CHUNK                   "<GET %s %llu %llu>\n"

// Peer success response on sending a file portion to another peer
// arguments: byte count of file
#define RESP_PEER_GET_FILE_CHUNK_SUCCESS             "<REP data %llu>\n"

// error for when peer request more than 1024 bytes
#define RESP_GET_FILE_CHUNK_FAILURE                    "<GET invalid>\n"

#endif