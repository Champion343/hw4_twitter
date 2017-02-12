#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <grpc++/grpc++.h>
#include <vector>
#include <deque>
#include "fbp.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
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
	ServerReaderWriter<Message,Message>* stream;
	fstream file;
	streampos position = 0;
	vector<string> joinTime;
	
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
		cout << "unfriend index: " << index << endl;
		if( index < 0)
			return false;
		following.erase(following.begin()+index);
		return true;
	}
	
	//lost a follower
	bool unfollowedBy(string person)
	{
		int index = findName(person, &followers);
		cout << "unfollow index: " << index << endl;
		if( index < 0)
			return false;
		followers.erase(followers.begin()+index);
		return true;
	}
	
	void newMsg(string chatMsg)
	{
		
	}
};

//Global: all chatrooms
vector<Room> chatRooms;
vector< ServerReaderWriter<Message,Message>* > Streams;

//find exsiting chatroom, return index
int findName(string username, vector<Room>* list)
{
	cout << "finding name...";
	for(int i=0; i < (int)list->size(); i++)
		if(username == list->at(i).username)
		{
			cout << "found" << endl;
			return i;
		}
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
		//exit(1);
		cout << "tryna list" << endl;
		cout << "loop1: " << (int)chatRooms.size() << endl;
		for(int i=0; i < (int)chatRooms.size(); i++)
		{
			cout << "adding to listreply all rooms: " << chatRooms[i].username << endl;
			reply->add_all_roomes(chatRooms[i].username);
		}
		//set all rooms joined by user
		int index = findName(request->username(), &chatRooms);
		cout << "index: " << index << " loop2: " << (int)chatRooms[index].following.size() << endl;
		for(int i=0; i < (int)chatRooms[index].following.size(); i++)
			{
			cout << "adding to listreply all joined rooms: " << chatRooms[index].following[i] << endl;
			reply->add_joined_roomes(chatRooms[index].following[i]);
			reply->get_joined_roomes(chatRooms[index].following[i]);
			}
		return Status::OK;
	}

	// Join(join own room) take time now, when chat command this is upper bound for time
	Status Join(ServerContext* context, const Message* request, Reply* reply) 
	override 
	{
		//add new friend to user, add user to new friend's followers
		cout << "tryna join" << endl;
		int user, joining;
		user = findName(request->username(), &chatRooms);
		joining = findName(request->msg(), &chatRooms);
		if( user < 0 || joining < 0)
		{
			reply->set_msg("join fail");
			cout << "join fail1" << endl;
		}
		else if( chatRooms[user].addFriend(request->msg()) &&
				 chatRooms[joining].addFollower(request->username()) )
				 {
					reply->set_msg("join success");
					cout << "join success" << endl;
				 }
		else
		{
			reply->set_msg("join fail");
			cout << "join fail2" << endl;
		}
		return Status::OK;
	}

	// Leave
	Status Leave(ServerContext* context, const Message* request, Reply* reply) 
	override 
	{
		//delete person from user's friends, delete user from person's followers
		cout << request->username() << " tryna leave " << request->msg() << endl;
		int user, leaving;
		user = findName(request->username(), &chatRooms);
		leaving = findName(request->msg(), &chatRooms);
		cout << "size " << chatRooms.size() << " indexs " << user << "  " << leaving << endl;
		if( user < 0 || leaving < 0)
		{
			cout << "leave fail1" << endl;
			reply->set_msg("leave fail");
		}
		else if( chatRooms[user].unfriend(request->msg()) &&
				 chatRooms[leaving].unfollowedBy(request->username()) )
				 {
			cout << "leave success" << endl;
					reply->set_msg("leave success");
				 }
		else{
			cout << "leave fail2" << endl;
			reply->set_msg("leave fail");
		}
		cout << "returning from leave" << endl;
		return Status::OK;
	}

	// Chat
	Status Chat(ServerContext* context, ServerReaderWriter<Message,Message>* stream) 
	override
	{
		//bistreamMsg(request->username, request->arguments.(0));
		//initial call to chat, setup chat then while loop
		Message firstMsg;
		stream->Read(&firstMsg);
		int index = findName(firstMsg.username, &chatRooms);
		chatRooms[index].stream = stream;
		//get last 20 msgs from following
		//in one loop, find most recent from all followers, save the others, read a new one, compare, etc
		deque<string> recentMsgs;
		string line;
		for(int i=0; i < 20; i++)
		{
			for(int j=0; j < (int)chatRooms[index].following.size(); j++)
			{
				int foll = findName(chatRooms[index].following[j], &chatRooms);
				chatRooms[foll].file.open(user + ".txt");
				if(chatRooms[foll].position == 0)
				{
					getline(chatRooms[foll].file, line);
					chatRooms[foll].position = chatRooms[foll].file.tellg();
				}
				else
				{
					chatRooms[foll].file.seekg(chatRooms[foll].position);
					getline(chatRooms[foll].file, line);
					chatRooms[foll].position = chatRooms[foll].file.tellg();
				}
				placeIn(line, &recentMsgs);
			}
		}
		//when post, loop thru followers and stream out
		std::vector<Message> received_msgs;
		Message note;
		while (stream->Read(&note)) {
		  for (const Message& n : received_msgs) {
			if (n.location().latitude() == note.location().latitude() &&
				n.location().longitude() == note.location().longitude()) {
			  stream->Write(n);
			}
		  }
		  received_notes.push_back(note);
		}
		return Status::OK;
	}
  
};

void placeIn(string chatMsg, deque<string>* last20)
{
	if(last20->size() == 0)
	{
		last20->push_back(chatMsg);
		return
	}
	for(int i=0; i < last20->size(); i++)
	string sub = chatMsg.substr(line.find(" "));
	string timeString = chatMsg.substr(line.find(" "), chatMsg.find(" ") - sub.find(" "));
	string msg = sub.substr(sub.find(" ");
	cout << "date: " << timeString << endl;
	deque<string>::iterator it = last20->begin(); 
	if(
}

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

