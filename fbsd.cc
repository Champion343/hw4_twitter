#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <grpc++/grpc++.h>
#include <vector>
#include "fbp.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using fbp::Reply;
using fbp::ListReply;
using fbp::Message;
using fbp::CRMasterServer;
using namespace std;

//find exsiting name
int findName(string username, vector<string>* list)
{
	for(int i=0; i < (int)list->size(); i++)
		if(username == list->at(i))
			return i;
	return -1;
}

struct Room
{
	//name of room and followers
	string username;
	vector<string> followers;
	
	Room(string name) : username(name) {}
	
	//new friend
	bool join(string follower)
	{
		if(findName(follower, &followers) >= 0)
			return false;
		followers.push_back(follower);
		return true;
	}
	
	//unfriended
	bool leave(string follower)
	{
		int index = findName(follower, &followers);
		if( index < 0)
			return false;
		followers.erase(followers.begin()+index-1);
		return true;
	}
};

//Global: all chatrooms
vector<Room> chatRooms;

//find exsiting chatroom
int findName(string username, vector<Room>* list)
{
	for(int i=0; i < (int)list->size(); i++)
		if(username == list->at(i).username)
			return i;
	return -1;
}

//create chat room
bool createChatroom(string username)
{
	if(findName(username, &chatRooms) >= 0)
		return false;
	Room newRoom(username);
	chatRooms.push_back(newRoom);
	cout << "created room " << username << endl;
	return true;
}

//overrides of proto
class FBServiceImpl final : public CRMasterServer::Service {
	// Login
  Status Login(ServerContext* context, const Message* request,
                  Reply* reply) override {
    if(!createChatroom(request->username()))
		cout << "service no" << endl;
	else
		cout << "service yes" << endl;
	reply->set_msg("doine");
    return Status::OK;
  }
  
  // List
  Status List(ServerContext* context, const Message* request,
                  ListReply* reply) override {
    //getList(request->username);
    return Status::OK;
  }

// Join
  Status Join(ServerContext* context, const Message* request,
                  Reply* reply) override {
    //joinChatroom(request->username, request->arguments.(0));
    return Status::OK;
  }

// Leave
  Status Leave(ServerContext* context, const Message* request,
                  Reply* reply) override {
    //leaveChatroom(request->username, request->arguments.(0));
    return Status::OK;
  }

// Chat
  Status Chat(ServerContext* context, const Message* msg,
                  Message* reply) {
    //bistreamMsg(request->username, request->arguments.(0));
    return Status::OK;
  }
  
};

void RunServer() {
  string server_address("0.0.0.0:50081");
  FBServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  unique_ptr<Server> server(builder.BuildAndStart());
  cout << "Server listening on " << server_address << endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}

