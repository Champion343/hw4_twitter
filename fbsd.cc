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

//find exsiting name in a vector, get index
int findName(string username, vector<string>* list)
{
	for(int i=0; i < (int)list->size(); i++)
		if(username == list->at(i))
			return i;
	return -1;
}

struct Room
{
	//name of room, followers, and following
	string username;
	vector<string> followers;
	vector<string> following;
	
	Room(string name) : username(name) {}
	
	//new following
	bool addFriend(string person)
	{
		if(findName(person, &following) >= 0)
			return false;
		following.push_back(person);
		return true;
	}
	
	//new follower
	bool addFollower(string follower)
	{
		if(findName(follower, &followers) >= 0)
			return false;
		followers.push_back(follower);
		return true;
	}
	
	//unfollow
	bool unfriend(string person)
	{
		int index = findName(person, &following);
		if( index < 0)
			return false;
		following.erase(following.begin()+index-1);
		return true;
	}
	
	//lost a follower
	bool unfollowedBy(string person)
	{
		int index = findName(person, &followers);
		if( index < 0)
			return false;
		followers.erase(followers.begin()+index-1);
		return true;
	}
};

//Global: all chatrooms
vector<Room> chatRooms;

//find exsiting chatroom, return index
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
	cout << "created room: " << username << endl;
	return true;
}

//overrides of proto
class FBServiceImpl final : public CRMasterServer::Service 
{
	
	// Login
    Status Login(ServerContext* context, const Message* request, Reply* reply) 
	override 
	{
		if(createChatroom(request->username()))
			reply->set_msg("server created room");
		else
			reply->set_msg("server no created room");
		return Status::OK;	
	}
  
	// List
	Status List(ServerContext* context, const Message* request, ListReply* reply) 
	override 
	{
		//set all rooms
		for(int i=0; i < (int)chatRooms.size(); i++)
			reply->set_all_roomes(i, chatRooms[i].username);
		//set all rooms joined by user
		int index = findName(request->username(), &chatRooms);
		for(int i=0; i < (int)chatRooms[index].following.size(); i++)
			reply->set_joined_roomes(i, chatRooms[index].following[i]);
		return Status::OK;
	}

	// Join
	Status Join(ServerContext* context, const Message* request, Reply* reply) 
	override 
	{
		//add new friend to user, add user to new friend's followers
		int user, joining;
		user = findName(request->username(), &chatRooms);
		joining = findName(request->msg(), &chatRooms);
		if( user < 0 || joining < 0)
			reply->set_msg("join fail");
		else if( chatRooms[user].addFriend(request->msg()) &&
				 chatRooms[joining].addFollower(request->username()) )
					reply->set_msg("join success");
		else
			reply->set_msg("join fail");
		return Status::OK;
	}

	// Leave
	Status Leave(ServerContext* context, const Message* request, Reply* reply) 
	override 
	{
		//delete person from user's friends, delete user from person's followers
		int user, leaving;
		user = findName(request->username(), &chatRooms);
		leaving = findName(request->msg(), &chatRooms);
		if( user < 0 || leaving < 0)
			reply->set_msg("leave fail");
		else if( chatRooms[user].unfriend(request->msg()) &&
				 chatRooms[leaving].unfollowedBy(request->username()) )
					reply->set_msg("leave success");
		else
			reply->set_msg("leave fail");
		return Status::OK;
	}

	// Chat
	Status Chat(ServerContext* context, const Message* msg, Message* reply) 
	{
		//bistreamMsg(request->username, request->arguments.(0));
		return Status::OK;
	}
  
};

void RunServer() 
{
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

int main(int argc, char** argv) 
{
  RunServer();

  return 0;
}

