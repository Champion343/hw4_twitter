#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <grpc++/grpc++.h>
#include <vector>
#include <deque>
#include <ctime>
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
	//fstream* file;
	//streampos position;
	//int position;
	vector<string> joinTime;
	time_t now;
	
	Room()
	{
		//position = 0;
		//file = new fstream;
		stream = NULL;
	}
	
	Room(string name) : username(name) 
	{
		//position = 0;
		//file = new fstream;
		stream = NULL;
	}
	
	~Room()
	{
		//file->close();
		//delete file;
	}
	
	//new following
	bool addFriend(string person)
	{
		if(findName(person, &following) >= 0)
			return false;
		following.push_back(person);
		now = time(0);
		string date = ctime(&now);
		string hms = date.substr(date.find(":") -2, date.find_last_of(":") +3 - (date.find(":") -2));
		cout << "hms[" << hms << ']' << endl;
		joinTime.push_back(hms);
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
		joinTime.erase(joinTime.begin()+index);
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
	cout << "create chatroom" << endl;
	if(findName(username, &chatRooms) >= 0)
		return false;
	cout << "new room" << endl;
	Room newRoom(username);
	cout << "new room created" << endl;
	cout << "pushing size " << chatRooms.size() << endl;
	chatRooms.push_back(newRoom);
	cout << "created room: " << username << endl;
	return true;
}

bool isLaterthan(string time1, string time2)
{
	return true;
}

string getTimeString(string chatMsg)
{
	//get time from chat message
	string sub = chatMsg.substr(chatMsg.find(" "));
	string timeString = chatMsg.substr(chatMsg.find(" "), sub.find(" ") - chatMsg.find(" "));
	string msg = sub.substr(sub.find(" "));
	cout << "time[ " << timeString << ']' << endl;
	return timeString;
}

void placeIn(string chatMsg, deque<string>* last20, string reference)
{
	if(isLaterthan(reference, getTimeString(chatMsg))) //check when user joined a room
		return;
	if(last20->size() == 0)//first chat message to add
	{
		last20->push_back(chatMsg);
		return;
	}
	deque<string>::iterator it;
	for (it=last20->begin(); it!=last20->end(); ++it) //find repeated chat messages
		if(*it == chatMsg)
			return;
	string timeString = getTimeString(chatMsg);
	
	for (it=last20->begin(); it!=last20->end(); ++it)
	{
		if(isLaterthan(timeString, getTimeString(*it)))
			last20->insert(it, chatMsg); //push latest message to front of deque
		if(last20->size() > 20)
			last20->pop_back(); //pop oldest message
	}
}

//overrides of proto
class FBServiceImpl final : public CRMasterServer::Service 
{
	
	// Login
    Status Login(ServerContext* context, const Message* request, Reply* reply) 
	override 
	{
		cout << "creating room: " << request->username() << endl;
		if(createChatroom(request->username()))
			reply->set_msg("server created room");
		else
			reply->set_msg("server no created room");
		cout << "ok login" << endl;
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
		cout << "chat call" << endl;
		Message firstMsg, reply20;
		stream->Read(&firstMsg);
		cout << "read first msg: " << firstMsg.username() << ' ' << firstMsg.msg() << endl;
		int index = findName(firstMsg.username(), &chatRooms);
		string user = firstMsg.username();
		chatRooms[index].stream = stream;
		cout << "assigned a stream" << endl;
		//get last 20 msgs from following
		//in one loop, find most recent from all followers, save the others, read a new one, compare, etc
		deque<string> recentMsgs;
		string line;
		cout << "first for loop" << endl;
		fstream file;
		for(int j=0; j < (int)chatRooms[index].following.size(); j++)
		{
			int foll = findName(chatRooms[index].following[j], &chatRooms);
			cout << "opening file " << chatRooms[foll].username + ".txt" << endl;
			file.open(chatRooms[foll].username + ".txt");
			if(file.is_open())
			cout << "opened file "<< endl;
			else
				cout << "NO OPEN FILE for reading (╯°□°)╯︵ ┻━┻" << endl;
			while(getline(file, line))
			{
				cout << "got line " << line << endl;
				placeIn(line, &recentMsgs, chatRooms[index].joinTime[j]);
			}
			cout << "closing file" << endl;
			file.close();
			cout << "first for loop in " << j << endl;
		}
		cout << "first for loop ended" << endl;
		//send last 20 messages
		//deque<string>::iterator it;
		if(recentMsgs.size() == 0)
		{
			cout << "wrting nothing for last 20 msgs" << endl;
			reply20.set_msg("no recent msgs");
			stream->Write(reply20);
		}
		cout << "second for loop" << endl;
		for (int i=0; i < 20 && recentMsgs.size() != 0; i++)
		{
			cout << "reply 20 size " << recentMsgs.size() << endl;
			reply20.set_msg(recentMsgs.back());
			cout << "set msg" << endl;
			stream->Write(reply20);
			cout << "write msg" << endl;
			recentMsgs.pop_back();
		}
		cout << "second for loop ended" << endl;
		//when post, loop thru followers and stream out
		Message note;
		//open file with truncation
		file.open(user + ".txt", fstream::out | fstream::trunc);
		if(file.is_open())
			cout << "opened file for append"<< endl;
		else
			cout << "NO OPEN FILE for writing (╯°□°)╯︵ ┻━┻" << endl;
		string lineMsg;
		cout << "while loop" << endl;
		time_t nowtime;
		string date;
		string hms;
		//first message add to file
		lineMsg.clear();
		nowtime = time(0);
		date = ctime(&nowtime);
		hms = date.substr(date.find(":") -2, date.find_last_of(":") +3 - (date.find(":") -2));
		lineMsg = user + ' ' + hms + ' ' + firstMsg.msg();
		file << lineMsg << endl;
		int k;
		cout << "third for loop" << endl;
			for(int i=0; i < (int)chatRooms[index].followers.size(); i++)
			{
				k = findName(chatRooms[index].followers[i], &chatRooms);
				if(index == k)
					continue;
				cout << "writing" << endl;
				if(chatRooms[k].stream != NULL)
					chatRooms[k].stream->Write(note);
				else
					cout << "null stream" << endl;
			}
			cout << "thrid for loop ended" << endl;
		
		while (1) 
		{
			cout << "while1" << endl;
			if(stream->Read(&note))//blocking
			{
			cout << "read something" << endl;
			lineMsg.clear();
			nowtime = time(0);
			date = ctime(&nowtime);
			hms = date.substr(date.find(":") -2, date.find_last_of(":") +3 - (date.find(":") -2));
			lineMsg = user + ' ' + hms + ' ' + note.msg();
			file << lineMsg << endl;
			cout << "4 for loop" << endl;
			for(int i=0; i < (int)chatRooms[index].followers.size(); i++)
			{
				k = findName(chatRooms[index].followers[i], &chatRooms);
				if(index == k)
					continue;
				cout << "writing" << endl;
				if(chatRooms[k].stream != NULL)
					chatRooms[k].stream->Write(note);
				else
					cout << "null stream" << endl;
			}
			cout << "4 for loop ended" << endl;
			}
		}
		file.close();
		cout << "out of while" << endl;
		return Status::OK;
	}
  
};

void RunServer() 
{
  string server_address("0.0.0.0:50023");
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

