#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <grpc++/grpc++.h>
#include <vector>
#include <deque>
#include <ctime>
#include <google/protobuf/util/time_util.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
 #include <queue> 
#include <thread>
#include "fbp.grpc.pb.h"

#include <unistd.h>

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;
using fbp::Request;
using fbp::Reply;
using fbp::ListReply;
using fbp::Message;
using fbp::CRMasterServer;

using grpc::Channel;
using grpc::ClientReaderWriter;
using grpc::ClientContext;

using namespace std;

int logic[3] = {0,0,0};
int myPort;
vector<string> otherHosts;
vector<string> otherHosts1;
vector<string> otherHosts2;
deque<Request> buffer;
void RunServer(string server_address);
void checkMaster(string host_name);

//find exsiting name in a vector, get index
int findName(string username, vector<string>* list)
{
	for(int i=0; i < (int)list->size(); i++)
		if(username == list->at(i))
			return i;
	return -1;
}

//Client object used for grpc calls
class ClientM {
 public:
  ClientM(std::shared_ptr<Channel> channel)
      : stub_(CRMasterServer::NewStub(channel)) {}
	 
	string Time() {
	Reply reply;
	Reply send;
	ClientContext context;
	// The actual RPC.
	Status status = stub_->Time(&context, send, &reply);
	// Act upon its status.
	if (status.ok()) {
	return reply.msg();
	} else {
	std::cout << myPort  << status.error_code() << ": " << status.error_message()
			<< std::endl;
	return "fail";
	}
  }
   private:
  std::unique_ptr<CRMasterServer::Stub> stub_;
};
  
//a person's account
struct Room
{
	string username;
	vector<string> followers;
	vector<string> following;
	ServerReaderWriter<Message,Message>* stream;
	vector<string> joinTime; //when subscribing to someone, store the time
	time_t now;
	
	Room()
	{
		stream = NULL;
	}
	
	Room(string name) : username(name) 
	{
		stream = NULL;
	}
	
	~Room(){}
	
	//new following
	bool addFriend(string person)
	{
		if(findName(person, &following) >= 0)
			return false;
		following.push_back(person);
		//get the time H:M:S
		ClientM master(grpc::CreateChannel(
				"128.194.143.156:50031", grpc::InsecureChannelCredentials()));
		//now = time(0);
		//string date = ctime(&now);
		//string hms = date.substr(date.find(":") -2, date.find_last_of(":") +3 - (date.find(":") -2));
		string hms = master.Time();
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
		if( index < 0)
			return false;
		//remove from both following list and time subscribed to person 
		following.erase(following.begin()+index);
		joinTime.erase(joinTime.begin()+index);
		return true;
	}
	
	//lost a follower
	bool unfollowedBy(string person)
	{
		int index = findName(person, &followers);
		if( index < 0)
			return false;
		followers.erase(followers.begin()+index);
		return true;
	}
};

//Global: all chatrooms and bi directional streams
vector<Room> chatRooms;
vector< ServerReaderWriter<Message,Message>* > Streams;

//find exsiting chatroom, return index
int findName(string username, vector<Room>* list)
{
	for(int i=0; i < (int)list->size(); i++)
		if(username == list->at(i).username)
		{
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
	cout << myPort  << "created room: " << username << endl;
	return true;
}

//if time1 is closer to the present than time2 
//return true
bool isLaterthan(string time1, string time2)
{
	//parse H:M:S and convert to int
	int Htime1 = stoi(time1.substr(0,2), NULL);
	int Mtime1 = stoi(time1.substr(3,2), NULL);
	int Stime1 = stoi(time1.substr(6,2), NULL);
	int Htime2 = stoi(time2.substr(0,2), NULL);
	int Mtime2 = stoi(time2.substr(3,2), NULL);
	int Stime2 = stoi(time2.substr(6,2), NULL);
	//compare HMS
	if( Htime1 > Htime2)
		return true;
	if( Htime1 == Htime2)
	{
		if( Mtime1 > Mtime2)
			return true;
		if( Mtime1 == Mtime2)
			if( Stime1 >= Stime2)
				return true;
	}	
	return false;
}

//return H:M:S from "user H:M:S message"
string getTimeString(string chatMsg)
{
	//get time from chat message
	string sub = chatMsg.substr(chatMsg.find(" "));
	string timeString = sub.substr(1, 8);
	return timeString;
}

//place chat message from a chatroom into a deque for sending the 
//latest 20 messages from person's list of subscriptions
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
	for(int i=0; i<(int)last20->size(); i++)//check for repeats
	{
		if(last20->at(i) == chatMsg)
		{
			return;
		}
	}
	string timeString = getTimeString(chatMsg);
	for (it=last20->begin(); it!=last20->end(); ++it)
	{
		if(isLaterthan(timeString, getTimeString(*it)))
		{
			last20->insert(it, chatMsg); //push latest message to front of deque
			if(last20->size() > 20)
				last20->pop_back(); //pop oldest message
			return;
		}
	}
}

bool conn = true;


void assigned_Worker(int port, vector<string>* host1, vector<string>* host2)
{
	host1->clear();
	host2->clear();
	switch(port)
	{
		case 50035://worker1
			host1->push_back("128.194.143.156:50038");
			host2->push_back("128.194.143.213:50039");
			host2->push_back("128.194.143.213:50040");
			break;
		case 50036://worker2
			host1->push_back("128.194.143.156:50038");
			host2->push_back("128.194.143.213:50040");
			host2->push_back("128.194.143.213:50041");
			break;
		case 50037://worker3
			host1->push_back("128.194.143.156:50038");
			host2->push_back("128.194.143.213:50041");
			host2->push_back("128.194.143.213:50039");
			break;
		case 50038://worker4
			host1->push_back("128.194.143.215:50035");
			host1->push_back("128.194.143.215:50036");
			host2->push_back("128.194.143.213:50039");
			host2->push_back("128.194.143.213:50040");
			break;
		case 50039://worker5
			host1->push_back("128.194.143.156:50038");
			host2->push_back("128.194.143.215:50035");
			host2->push_back("128.194.143.215:50036");
			break;
		case 50040://worker6
			host1->push_back("128.194.143.156:50038");
			host2->push_back("128.194.143.215:50036");
			host2->push_back("128.194.143.215:50037");
			break;
		case 50041://worker7
			host1->push_back("128.194.143.156:50038");
			host2->push_back("128.194.143.215:50037");
			host2->push_back("128.194.143.215:50035");
			break;
	}
}

void other_Workers(int port, vector<string>* hosts)
{
	hosts->clear();
	switch(port)
	{
		case 50035://worker1
			hosts->push_back("0.0.0.0:50036");
			hosts->push_back("0.0.0.0:50037");
			hosts->push_back("128.194.143.156:50038");
			hosts->push_back("128.194.143.213:50039");
			hosts->push_back("128.194.143.213:50040");
			hosts->push_back("128.194.143.213:50041");
			break;
		case 50036://worker2
			hosts->push_back("0.0.0.0:50035");
			hosts->push_back("0.0.0.0:50037");
			hosts->push_back("128.194.143.156:50038");
			hosts->push_back("128.194.143.213:50039");
			hosts->push_back("128.194.143.213:50040");
			hosts->push_back("128.194.143.213:50041");
			break;
		case 50037://worker3
			hosts->push_back("0.0.0.0:50035");
			hosts->push_back("0.0.0.0:50036");
			hosts->push_back("128.194.143.156:50038");
			hosts->push_back("128.194.143.213:50039");
			hosts->push_back("128.194.143.213:50040");
			hosts->push_back("128.194.143.213:50041");
			break;
		case 50038://worker4
			hosts->push_back("128.194.143.215:50035");
			hosts->push_back("128.194.143.215:50036");
			hosts->push_back("128.194.143.215:50037");
			hosts->push_back("128.194.143.213:50039");
			hosts->push_back("128.194.143.213:50040");
			hosts->push_back("128.194.143.213:50041");
			break;
		case 50039://worker5
			hosts->push_back("0.0.0.0:50040");
			hosts->push_back("0.0.0.0:50041");
			hosts->push_back("128.194.143.156:50038");
			hosts->push_back("128.194.143.215:50035");
			hosts->push_back("128.194.143.215:50036");
			hosts->push_back("128.194.143.215:50037");
			break;
		case 50040://worker6
			hosts->push_back("0.0.0.0:50039");
			hosts->push_back("0.0.0.0:50041");
			hosts->push_back("128.194.143.156:50038");
			hosts->push_back("128.194.143.215:50035");
			hosts->push_back("128.194.143.215:50036");
			hosts->push_back("128.194.143.215:50037");
			break;
		case 50041://worker7
			hosts->push_back("0.0.0.0:50039");
			hosts->push_back("0.0.0.0:50040");
			hosts->push_back("128.194.143.156:50038");
			hosts->push_back("128.194.143.215:50035");
			hosts->push_back("128.194.143.215:50036");
			hosts->push_back("128.194.143.215:50037");
			break;
	}
}

bool writeOnce(int port)
{
	switch(port)
	{
		case 50035:
			if(myPort ==50038 || myPort == 50039)
				return true;
			break;
		case 50036:
			if(myPort ==50038 || myPort == 50040)
				return true;
			break;
		case 50037:
			if(myPort ==50038 || myPort == 50041)
				return true;
			break;
		case 50038:
			if(myPort ==50035 || myPort == 50039)
				return true;
			break;
		case 50039:
			if(myPort ==50038 || myPort == 50035)
				return true;
			break;
		case 50040:
			if(myPort ==50038 || myPort == 50036)
				return true;
			break;
		case 50041:
			if(myPort ==50038 || myPort == 50037)
				return true;
			break;
	}
	return false;
}

//Client object used for grpc calls
class Client {
 public:
  Client(std::shared_ptr<Channel> channel)
      : stub_(CRMasterServer::NewStub(channel)) {}
  
  bool Ping() {
	Reply reply;
	Request send;
	ClientContext context;
	// The actual RPC.
	Status status = stub_->Ping(&context, send, &reply);
	// Act upon its status.
	if (status.ok()) {
	return true;
	} else {
	std::cout << myPort  << status.error_code() << ": " << status.error_message()
			<< std::endl;
	return false;
	}
  }
  
  //buffer - signal to get missing data
  bool Buffer() {
	Reply reply;
	Request send;
	ClientContext context;
	send.set_username(to_string(myPort));
	// The actual RPC.
	Status status = stub_->Buffer(&context, send, &reply);
	// Act upon its status.
	if (status.ok()) {
	return true;
	} else {
	std::cout << myPort  << status.error_code() << ": " << status.error_message()
			<< std::endl;
	return false;
	}
  }
  
  //update - signal to get room data
  int Update(Request msg) {
	  // Container for the data we expect from the server.
	Request reply;
	ClientContext context;

	// The actual RPC.
	Status status = stub_->Update(&context, msg, &reply);
	//fill in room
	if(reply.username().compare("end of rooms") == 0)
		return -1;
	if(reply.username().compare("logic clock"))
	{
		for(int i=0; i<3; i++)
			logic[i] = atoi(reply.arguments(i).c_str());
		return 1;
	}
	int index = findName(reply.username(),&chatRooms);
	if(index >=0)
	if(reply.arguments(0).compare("followers"))
	{
		for(int i=1; i<reply.arguments_size(); i++)
			chatRooms[index].followers.push_back(reply.arguments(i));
	}
	else if(reply.arguments(0).compare("following"))
	{
		for(int i=1; i<reply.arguments_size(); i++)
			chatRooms[index].following.push_back(reply.arguments(i));
	}
	else if(reply.arguments(0).compare("joinTime"))
	{
		for(int i=1; i<reply.arguments_size(); i++)
			chatRooms[index].joinTime.push_back(reply.arguments(i));
	}
	else //username
	{
		Room roomup(reply.username());
		chatRooms.push_back(roomup);
	}
	return 1;
  }
  
  //Cast - send updates to other workers
  bool Cast(Message fwd) {
	// add logic clock
	//open file
	for(int i=0; i<3; i++)
		fwd.add_clock(to_string(logic[i]));
	// Container for the data we expect from the server.
	Reply reply;
	Request request;
	ClientContext context;
	request.set_username(fwd.username());
	request.add_arguments(to_string(myPort));
	request.add_arguments(fwd.msg());
	request.add_arguments(fwd.username()+' '+fwd.msg());

	// The actual RPC.
	Status status = stub_->Cast(&context, request, &reply);

	// Act upon its status.
	if (status.ok()) {
	return true;
	} else {
	std::cout << myPort  << status.error_code() << ": " << status.error_message()
			<< std::endl;
	return false;
	}
  }
  
  string Time() {
	Reply reply;
	Reply send;
	ClientContext context;
	// The actual RPC.
	Status status = stub_->Time(&context, send, &reply);
	// Act upon its status.
	if (status.ok()) {
	return reply.msg();
	} else {
	std::cout << myPort  << status.error_code() << ": " << status.error_message()
			<< std::endl;
	return "fail";
	}
  }
  
  //Login function send message containing username and receives response
  bool Login(Request message) {
    Request msg;
	
	// Container for the data we expect from the server.
    Reply reply;
    ClientContext context;
	cout << myPort  << "worker client login\n";
	if(message.arguments_size() > 1)
	{
		cout << myPort  << "worker client login >1\n";
		msg.set_username(message.arguments(1));
		msg.add_arguments(message.arguments(2));
		Status status = stub_->Login(&context, msg, &reply);
		
		// Act upon its status.
		if (status.ok()) {
		  return true;
		} else {
		  std::cout << myPort  << status.error_code() << ": " << status.error_message()
					<< std::endl;
		  return false;
		}
	}
	else
	{
		msg = message;
		Status status = stub_->Login(&context, msg, &reply);
		cout << myPort  << "worker client login <1\n";
		// Act upon its status.
		if (status.ok()) {
		  return true;
		} else {
		  std::cout << myPort  << status.error_code() << ": " << status.error_message()
					<< std::endl;
		  return false;
		}
	}
	
    // The actual RPC.
    
	
  }
	//Join function sends a message containing username and the room that is being
	//joined then returns server response
    bool Join(Request message) {
    // Data we are sending to the server.
	Request msg;
	cout << myPort  << "arg size" << message.arguments_size() << endl;
	if(message.arguments_size() > 2)
	{
		cout << myPort  << "join >1\n";
		msg.set_username(message.arguments(1));
		msg.add_arguments(message.arguments(2));
		msg.add_arguments(message.arguments(3));
		// Container for the data we expect from the server.
		Reply reply;

		ClientContext context;

		// The actual RPC.
		Status status = stub_->Join(&context, msg, &reply);

		// Act upon its status.
		if (status.ok()) {
		  return true;
		} else {
		  std::cout << myPort  << status.error_code() << ": " << status.error_message()
					<< std::endl;
		  return false;
		}
	}
	else
	{
			// Container for the data we expect from the server.
			cout << myPort  << "join<1\n";
		Reply reply;
		msg = message;
		ClientContext context;

		// The actual RPC.
		Status status = stub_->Join(&context, msg, &reply);

		// Act upon its status.
		if (status.ok()) {
		  return true;
		} else {
		  std::cout << myPort  << status.error_code() << ": " << status.error_message()
					<< std::endl;
		  return false;
    }
	}
  }
    //Leave function sends a message containing username and the room that is being
	//left then returns server response
    bool Leave(Request message) {
    // Data we are sending to the server.
	Request msg;
	if(message.arguments_size() > 2)
	{
		msg.set_username(message.arguments(1));
		msg.add_arguments(message.arguments(2));
		msg.add_arguments(message.arguments(3));
		// Container for the data we expect from the server.
		Reply reply;

		ClientContext context;

		// The actual RPC.
		Status status = stub_->Leave(&context, msg, &reply);

		// Act upon its status.
		if (status.ok()) {
		  return true;
		} else {
		  std::cout << myPort  << status.error_code() << ": " << status.error_message()
					<< std::endl;
		  return false;
		}
	}
	else
	{
			// Container for the data we expect from the server.
		Reply reply;
		msg = message;
		ClientContext context;

		// The actual RPC.
		Status status = stub_->Leave(&context, msg, &reply);

		// Act upon its status.
		if (status.ok()) {
		  return true;
		} else {
		  std::cout << myPort  << status.error_code() << ": " << status.error_message()
					<< std::endl;
		  return false;
    }
	}
  }
  
 private:
  std::unique_ptr<CRMasterServer::Stub> stub_;
};


//overrides of proto
class FBServiceImpl final : public CRMasterServer::Service 
{
	
	bool first = true;
	int last = 0;
	
	Status Buffer(ServerContext* context, const Request* request, Reply* reply) 
	override 
	{
		string machine ="";
		switch(atoi(request->username().c_str()))
		{
			case 50035:
			case 50036:
			case 50037:
				machine = "128.194.143.215";
				break;
			case 50038:
				machine = "128.194.143.156";
				break;
			case 50039:
			case 50040:
			case 50041:
				machine = "128.194.143.213";
				break;
			
		}
		machine+=':'+request->username();
		Client worker(grpc::CreateChannel(
		machine, grpc::InsecureChannelCredentials()));
		int end = buffer.size();
		Request r;
		for(int i=0; i<end;)
		{
			r = buffer.at(i);
			//for all the buffer messages that match the calling worker
			if(r.username().compare(machine) == 0)
			{
				if(r.arguments(0).compare("Login") == 0)
				{
					worker.Login(r);
				}
				else if(r.arguments(0).compare("Join") == 0)
				{
					worker.Join(r);
				}
				else if(r.arguments(0).compare("Leave") == 0)
				{
					worker.Leave(r);
				}
				else if(r.arguments(0).compare("Cast") == 0)
				{
					Message k;
					k.set_username(r.arguments(1));
					k.set_msg(r.arguments(2));
					worker.Cast(k);
				}
				buffer.erase(buffer.begin()+i);
			}
			else
				i++;
		}
		return Status::OK;
	}
	
	//update a worker
	Status Update(ServerContext* context, const Request* request, Request* roomSend) 
	override 
	{
		if(request->username().compare("logic clock") == 0)
		{
			roomSend->set_username("logic clock");
			for(int i=0; i<3; i++)
				roomSend->add_arguments(to_string(logic[i]));
			return Status::OK;
		}
		int index = atoi(request->username().c_str());
		if(index >= (int)chatRooms.size())
		{
			roomSend->set_username("end of rooms");
			return Status::OK;
		}
		else
			roomSend->set_username(chatRooms.at(index).username);
		
		if(request->arguments(0).compare("followers"))
		{
			for(int i=0; i<(int)chatRooms[index].followers.size(); i++)
				roomSend->add_arguments(chatRooms[index].followers.at(i));
		}
		else if(request->arguments(0).compare("following"))
		{
			for(int i=0; i<(int)chatRooms[index].following.size(); i++)
				roomSend->add_arguments(chatRooms[index].following.at(i));
		}
		else if(request->arguments(0).compare("joinTime"))
		{
			for(int i=0; i<(int)chatRooms[index].joinTime.size(); i++)
				roomSend->add_arguments(chatRooms[index].joinTime.at(i));
		}
		else //username
		{
			
		}
			
		return Status::OK;
	}
	
	//master server tells me to restart a worker
	Status Reset(ServerContext* context, const Request* request, Reply* reply) 
	override 
	{
		cout << myPort  << "reset" << request->username() << endl;
		if(fork() == 0)
		{
			cout << myPort  << "child reset\n";
			myPort = atoi(request->username().substr(request->username().find(':') +1).c_str());
			cout << myPort  << myPort << endl;
			other_Workers(myPort, &otherHosts);
			assigned_Worker(myPort, &otherHosts1, &otherHosts2);
			cout << myPort  << "rooms " << chatRooms.size() << endl;
			thread ping(checkMaster,"128.194.143.156:50031");
			RunServer(request->username());
		}
		return Status::OK;
	}
	
	//ping from master to check if im alive
	Status Ping(ServerContext* context, const Request* request, Reply* reply) 
	override 
	{
		//cout << myPort  << "ping" << endl;
		reply->set_msg("ayy");
		return Status::OK;
	}
	
	// Cast - recieve updates from other workers
    Status Cast(ServerContext* context, const Request* fwd, Reply* reply) 
	override 
	{
		if(writeOnce(atoi(fwd->arguments(0).c_str())))
		{
		//add message to file
		fstream file;
		string user = fwd->username();
		//open file with truncation for writing
		file.open(user + ".txt", fstream::out | fstream::app);
		if(file.is_open())
			cout << myPort  << "opened file for writing"<< endl;
		else
			cout << myPort  << "NO OPEN FILE for writing (╯°□°)╯︵ ┻━┻" << endl;
		string lineMsg;
		lineMsg = fwd->arguments(2);
		file << lineMsg << endl;
		cout << myPort  << "added to file: " << lineMsg << endl;
		file.close();
		cout << myPort  << "updated file" << endl;
		/*
		//update logic clock
		for(int i=0; i<3; i++)
			if(logic[i] < atoi(fwd->clock(i).c_str()))
				logic[i] = atoi(fwd->clock(i).c_str());
		//open file for writing
		file.open("logicClock.txt", fstream::out);
		if(file.is_open())
			cout << myPort  << "opened file for writing"<< endl;
		else
			cout << myPort  << "NO OPEN FILE for writing (╯°□°)╯︵ ┻━┻" << endl;
		for(int i=0; i<3; i++)
			file << logic[i] << endl;*/
		}
		Message note;
		note.set_username(fwd->username());
		note.set_msg(fwd->arguments(1));
		//loop thru followers and post to their screens
		int k;
		int index = findName(fwd->username(), &chatRooms);
		for(int i=0; i < (int)chatRooms[index].followers.size(); i++)
		{
			k = findName(chatRooms[index].followers[i], &chatRooms);
			if(index == k) //do not post to self
				continue;
			cout << myPort  << "writing to stream in cast" << endl;
			if(chatRooms[k].stream != NULL)
			{
				chatRooms[k].stream->Write(note);
				
			}
			else
				cout << myPort  << "null stream" << endl; //follower has not called CHAT yet
		}
		return Status::OK;	
	}
	
	// Login
    Status Login(ServerContext* context, const Request* request, Reply* reply) 
	override 
	{
		cout << myPort  << "creating room: " << request->username() << endl;
		if(createChatroom(request->username()))
		{
			
			reply->set_msg("server created room");
			//broadcast to all other workers
			int j=0;
			Request send;
			send = *request;
			send.add_arguments("ayy");
			cout << myPort  << "attempt broadcast\n";
			if(request->arguments_size() == 0)
			for(int i=0; i<6; i++)
			{
				cout << myPort  << "broadcasting\n" ;
				Client worker(grpc::CreateChannel(otherHosts[i], grpc::InsecureChannelCredentials()));
				
				cout << myPort  << "set argu\n" ;
				if(!worker.Login(send))//grpc fail
				{
					cout << myPort  << otherHosts[i] << "broadcast failed\n";
					//add to buffer
					if(otherHosts[i].compare("128.194.143.156:50038") != 0)//worker4, server wont die
					{
						j++;
						if(j == 3)//server is down
						{
							if(i<3)
							{
								Request buf;
								buf.set_username("128.194.143.215:50035");
								buf.add_arguments("Login");
								buf.add_arguments(send.username());
								buf.add_arguments(to_string(send.arguments_size()));
								buffer.push_back(buf);
								Request buf1;
								buf1.set_username("128.194.143.215:50036");
								buf1.add_arguments("Login");
								buf1.add_arguments(send.username());
								buf1.add_arguments(to_string(send.arguments_size()));
								buffer.push_back(buf1);
								Request buf2;
								buf2.set_username("128.194.143.215:50037");
								buf2.add_arguments("Login");
								buf2.add_arguments(send.username());
								buf2.add_arguments(to_string(send.arguments_size()));
								buffer.push_back(buf2);
							}
							else if(i>3)
							{
								Request buf;
								buf.set_username("128.194.143.215:50039");
								buf.add_arguments("Login");
								buf.add_arguments(send.username());
								buf.add_arguments(to_string(send.arguments_size()));
								buffer.push_back(buf);
								Request buf1;
								buf1.set_username("128.194.143.215:50040");
								buf1.add_arguments("Login");
								buf1.add_arguments(send.username());
								buf1.add_arguments(to_string(send.arguments_size()));
								buffer.push_back(buf1);
								Request buf2;
								buf2.set_username("128.194.143.215:50041");
								buf2.add_arguments("Login");
								buf2.add_arguments(send.username());
								buf2.add_arguments(to_string(send.arguments_size()));
								buffer.push_back(buf2);
							}
						}
					}
				}
				//cout << myPort  << otherHosts[i] << "broadcast succedd\n";
			}
		}
		else
			reply->set_msg("server no created room");
		return Status::OK;	
	}
  
	// List
	Status List(ServerContext* context, const Request* request, ListReply* reply) 
	override 
	{
		//list all chatrooms 
		for(int i=0; i < (int)chatRooms.size(); i++)
		{
			reply->add_all_roomes(chatRooms[i].username);
		}
		//list all rooms joined by user
		int index = findName(request->username(), &chatRooms);
		if(index >=0)
		for(int i=0; i < (int)chatRooms[index].following.size(); i++)
		{
			reply->add_joined_roomes(chatRooms[index].following[i]);
		}
		return Status::OK;
	}

	// Join(able to join own room, not sure if crash)
	Status Join(ServerContext* context, const Request* request, Reply* reply) 
	override 
	{
		//add new friend to user, add user to new friend's followers
		cout << myPort  << request->username() + " tryna join "+request->arguments(0) << endl;
		int user, joining;
		user = findName(request->username(), &chatRooms);
		joining = findName(request->arguments(0), &chatRooms);
		if( user < 0 || joining < 0)
		{
			reply->set_msg("join fail");
			cout << myPort  << "join fail1" << endl;
		}
		else if( chatRooms[user].addFriend(request->arguments(0)) &&
				 chatRooms[joining].addFollower(request->username()) )
				 {
					reply->set_msg("join success");
					cout << myPort  << "join success" << endl;
					//broadcast to all other workers
					int j=0;
					Request send;
					send = *request;
					send.add_arguments("ayy");
					if(request->arguments_size() == 1)
					for(int i=0; i<6; i++)
					{
						Client worker(grpc::CreateChannel(otherHosts[i], grpc::InsecureChannelCredentials()));
						
						if(!worker.Join(send))//grpc fail
						{
							//add to buffer
							if(otherHosts[i].compare("128.194.143.156:50038") != 0)//worker4, server wont die
							{
								j++;
								if(j == 3)//server is down
								{
									if(i<3)
									{
										Request buf;
										buf.set_username("128.194.143.215:50035");
										buf.add_arguments("Join");
										buf.add_arguments(send.username());
										buf.add_arguments(send.arguments(0));
										buf.add_arguments(send.arguments(1));
										buffer.push_back(buf);
										Request buf1;
										buf1.set_username("128.194.143.215:50036");
										buf1.add_arguments("Join");
										buf1.add_arguments(send.username());
										buf1.add_arguments(send.arguments(0));
										buf1.add_arguments(send.arguments(1));
										buffer.push_back(buf1);
										Request buf2;
										buf2.set_username("128.194.143.215:50037");
										buf2.add_arguments("Join");
										buf2.add_arguments(send.username());
										buf2.add_arguments(send.arguments(0));
										buf2.add_arguments(send.arguments(1));
										buffer.push_back(buf2);
									}
									else if(i>3)
									{
										Request buf;
										buf.set_username("128.194.143.215:50039");
										buf.add_arguments("Join");
										buf.add_arguments(send.username());
										buf.add_arguments(send.arguments(0));
										buf.add_arguments(send.arguments(1));
										buffer.push_back(buf);
										Request buf1;
										buf1.set_username("128.194.143.215:50040");
										buf1.add_arguments("Join");
										buf1.add_arguments(send.username());
										buf1.add_arguments(send.arguments(0));
										buf1.add_arguments(send.arguments(1));
										buffer.push_back(buf1);
										Request buf2;
										buf2.set_username("128.194.143.215:50041");
										buf2.add_arguments("Join");
										buf2.add_arguments(send.username());
										buf2.add_arguments(send.arguments(0));
										buf2.add_arguments(send.arguments(1));
										buffer.push_back(buf2);
									}
								}
							}
						}
					}
				 }
		else
		{
			reply->set_msg("join fail");
			cout << myPort  << "join fail2" << endl;
		}
		return Status::OK;
	}

	// Leave
	Status Leave(ServerContext* context, const Request* request, Reply* reply) 
	override 
	{
		//delete person from user's friends, delete user from person's followers
		cout << myPort  << request->username() << " tryna leave " << request->arguments(0) << endl;
		int user, leaving;
		user = findName(request->username(), &chatRooms);
		leaving = findName(request->arguments(0), &chatRooms);
		if( user < 0 || leaving < 0)
		{
			cout << myPort  << "leave fail1" << endl;
			reply->set_msg("leave fail");
		}
		else if( chatRooms[user].unfriend(request->arguments(0)) &&
				 chatRooms[leaving].unfollowedBy(request->username()) )
				 {
			cout << myPort  << "leave success" << endl;
					reply->set_msg("leave success");
					//broadcast to all other workers
					int j=0;
					Request send;
					send = *request;
					send.add_arguments("ayy");
					if(request->arguments_size() == 1)
					for(int i=0; i<6; i++)
					{
						Client worker(grpc::CreateChannel(otherHosts[i], grpc::InsecureChannelCredentials()));
						
						if(!worker.Leave(send))//grpc fail
						{
							//add to buffer
							if(otherHosts[i].compare("128.194.143.156:50038") != 0)//worker4, server wont die
							{
								j++;
								if(j == 3)//server is down
								{
									if(i<3)
									{
										Request buf;
										buf.set_username("128.194.143.215:50035");
										buf.add_arguments("Leave");
										buf.add_arguments(send.username());
										buf.add_arguments(send.arguments(0));
										buf.add_arguments(send.arguments(1));
										buffer.push_back(buf);
										Request buf1;
										buf1.set_username("128.194.143.215:50036");
										buf1.add_arguments("Leave");
										buf1.add_arguments(send.username());
										buf1.add_arguments(send.arguments(0));
										buf1.add_arguments(send.arguments(1));
										buffer.push_back(buf1);
										Request buf2;
										buf2.set_username("128.194.143.215:50037");
										buf2.add_arguments("Leave");
										buf2.add_arguments(send.username());
										buf2.add_arguments(send.arguments(0));
										buf2.add_arguments(send.arguments(1));
										buffer.push_back(buf2);
									}
									else if(i>3)
									{
										Request buf;
										buf.set_username("128.194.143.215:50039");
										buf.add_arguments("Leave");
										buf.add_arguments(send.username());
										buf.add_arguments(send.arguments(0));
										buf.add_arguments(send.arguments(1));
										buffer.push_back(buf);
										Request buf1;
										buf1.set_username("128.194.143.215:50040");
										buf1.add_arguments("Leave");
										buf1.add_arguments(send.username());
										buf1.add_arguments(send.arguments(0));
										buf1.add_arguments(send.arguments(1));
										buffer.push_back(buf1);
										Request buf2;
										buf2.set_username("128.194.143.215:50041");
										buf2.add_arguments("Leave");
										buf2.add_arguments(send.username());
										buf2.add_arguments(send.arguments(0));
										buf2.add_arguments(send.arguments(1));
										buffer.push_back(buf2);
									}
								}
							}
						}
					}
				 }
		else{
			cout << myPort  << "leave fail2" << endl;
			reply->set_msg("leave fail");
		}
		return Status::OK;
	}

	// Chat
	Status Chat(ServerContext* context, ServerReaderWriter<Message,Message>* stream) 
	override
	{
		
		//initial call to chat, setup chat then while loop read/write
		Message firstMsg, reply20;
		stream->Read(&firstMsg);
		int index = findName(firstMsg.username(), &chatRooms);
		string user = firstMsg.username();
		chatRooms[index].stream = stream;
		//get last 20 msgs from subscriptions
		deque<string> recentMsgs;
		string line;
		fstream file;
		for(int j=0; j < (int)chatRooms[index].following.size(); j++)
		{
			int foll = findName(chatRooms[index].following[j], &chatRooms);
			file.open(chatRooms[foll].username + ".txt");
			if(file.is_open())
			cout << myPort  << "opened file for reading recent"<< endl;
			else
				cout << myPort  << "NO OPEN FILE for reading recent (╯°□°)╯︵ ┻━┻" << endl;//not necessarily an error
			//go thru current subscription's file
			while(getline(file, line))
			{
				placeIn(line, &recentMsgs, chatRooms[index].joinTime[j]);
			}
			cout << myPort  << "closing file" << endl;
			file.close();
		}
		//no recent messages from subscriptions
		if(recentMsgs.size() == 0)
		{
			cout << myPort  << "wrting nothing for last 20 msgs" << endl;
			reply20.set_msg("no recent msgs");
			stream->Write(reply20);
		}
		cout << myPort  << "reply 20 size " << recentMsgs.size() << endl;
		//send most recent subscription messages
		for (int i=0; i < 20 && recentMsgs.size() != 0; i++)
		{
			
			reply20.set_msg(recentMsgs.back());
			stream->Write(reply20);
			recentMsgs.pop_back();
		}
		Message note;
		//open file with truncation for writing
		file.open(user + ".txt", fstream::out | fstream::trunc);
		if(file.is_open())
			cout << myPort  << "opened file for writing to self"<< endl;
		else
			cout << myPort  << "NO OPEN FILE for writing to self(╯°□°)╯︵ ┻━┻" << endl;
		string lineMsg;
		time_t nowtime;
		string date;
		string hms;
		int k;
		google::protobuf::Timestamp* temptime;
		Client master(grpc::CreateChannel(
				"128.194.143.156:50031", grpc::InsecureChannelCredentials()));
		assigned_Worker(myPort,&otherHosts1,&otherHosts2);
		//Client worker(grpc::CreateChannel(host_name, grpc::InsecureChannelCredentials()));
		while (1) 
		{   //read client's message
			if(stream->Read(&note))//blocking
			{
				//show when first message got here
				if(first){
					first = false;
					google::protobuf::Timestamp* temptime = new google::protobuf::Timestamp();
					struct timeval tv;
					gettimeofday(&tv, NULL);
					temptime->set_seconds(tv.tv_sec);
					temptime->set_nanos(tv.tv_usec * 1000);
					cout << myPort  << google::protobuf::util::TimeUtil::ToString(*temptime) << endl;
				}
		
				lineMsg.clear();
				//time stamp
				//nowtime = time(0);
				//date = ctime(&nowtime);
				//hms = date.substr(date.find(":") -2, date.find_last_of(":") +3 - (date.find(":") -2));
				
				hms = master.Time();
				//format: username time message
				lineMsg = user + ' ' + hms + ' ' + note.msg();
				file << lineMsg << endl;
				//send to another worker
				Message fwd;
				fwd.set_username(user);
				fwd.set_msg(hms + ' ' + note.msg());
				//broadcast to all other workers
			int j=0;
			for(int i=0; i<6; i++)
			{
				Client worker(grpc::CreateChannel(otherHosts[i], grpc::InsecureChannelCredentials()));
				if(!worker.Cast(fwd))//grpc fail
				{
					//add to buffer
					if(otherHosts[i].compare("128.194.143.156:50038") != 0)//worker4, server wont die
					{
						j++;
						if(j == 3)//server is down
						{
							if(i<3)
							{
								Request buf;
								buf.set_username("128.194.143.215:50035");
								buf.add_arguments("Cast");
								buf.add_arguments(fwd.username());
								buf.add_arguments(fwd.msg());
								buffer.push_back(buf);
								Request buf1;
								buf1.set_username("128.194.143.215:50036");
								buf1.add_arguments("Cast");
								buf1.add_arguments(fwd.username());
								buf1.add_arguments(fwd.msg());
								buffer.push_back(buf1);
								Request buf2;
								buf2.set_username("128.194.143.215:50037");
								buf2.add_arguments("Cast");
								buf2.add_arguments(fwd.username());
								buf2.add_arguments(fwd.msg());
								buffer.push_back(buf2);
							}
							else if(i>3)
							{
								Request buf;
								buf.set_username("128.194.143.215:50039");
								buf.add_arguments("Cast");
								buf.add_arguments(fwd.username());
								buf.add_arguments(fwd.msg());
								buffer.push_back(buf);
								Request buf1;
								buf1.set_username("128.194.143.215:50040");
								buf1.add_arguments("Cast");
								buf1.add_arguments(fwd.username());
								buf1.add_arguments(fwd.msg());
								buffer.push_back(buf1);
								Request buf2;
								buf2.set_username("128.194.143.215:50041");
								buf2.add_arguments("Cast");
								buf2.add_arguments(fwd.username());
								buf2.add_arguments(fwd.msg());
								buffer.push_back(buf2);
							}
						}
					}
					else
					{
						Request buf;
						buf.set_username("128.194.143.156:50038");
						buf.add_arguments("Cast");
						buf.add_arguments(fwd.username());
						buf.add_arguments(fwd.msg());
						buffer.push_back(buf);
					}
				}
			}
				cout << myPort  << "added to file: " << lineMsg << endl;
				//loop thru followers and post to their screens
				for(int i=0; i < (int)chatRooms[index].followers.size(); i++)
				{
					k = findName(chatRooms[index].followers[i], &chatRooms);
					if(index == k) //do not post to self
						continue;
					cout << myPort  << "writing" << endl;
					if(chatRooms[k].stream != NULL)
					{
						chatRooms[k].stream->Write(note);
						
					}
					else
						cout << myPort  << "null stream" << endl; //follower has not called CHAT yet
				}
				++last;
				//wait for 50 messages
				if(last == 50){
					
				google::protobuf::Timestamp* temptime2 = new google::protobuf::Timestamp();
				struct timeval tv;
				gettimeofday(&tv, NULL);
				temptime2->set_seconds(tv.tv_sec);
				temptime2->set_nanos(tv.tv_usec * 1000);
				cout << myPort  << google::protobuf::util::TimeUtil::ToString(*temptime2) << endl;
				}
			}
		}
		file.close();
		cout << myPort  << "out of while" << endl;
		return Status::OK;
	}
  
};
void reconnect(string host_name)
{
	Client worker(grpc::CreateChannel(
		host_name, grpc::InsecureChannelCredentials()));
	while(!conn)
	if(worker.Buffer())
	{
		cout << myPort  << " look for buffer\n";
		conn = true;
	}
}
void checkMaster(string host_name)
{
	Client master(grpc::CreateChannel(
		host_name, grpc::InsecureChannelCredentials()));
	while(1)
	{
		sleep(5);
		//cout << myPort  << "Wping\n";
		if(!master.Ping() && conn)
		{
			conn = false;
			 cout << myPort  << " start reconnect thread\n";
			thread serveroff(reconnect,otherHosts2[0]);
			serveroff.detach();
		}
	}
	
}
//code from helloworld example given greeter_server.cc
void RunServer(string server_address) 
{
  FBServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  unique_ptr<Server> server(builder.BuildAndStart());
  cout << myPort  << "Server listening on " << server_address << endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

vector<Client*> workers;

int main(int argc, char** argv) 
{
  string server_address("0.0.0.0:50032");
  cout << myPort  << "agrc" << argc << endl;
  //cout << myPort  << "agrv0" << argv[0] << endl;
  //cout << myPort  << "agrv1" << argv[1] << endl;
  //cout << myPort  << "agrv2" << argv[2] << endl;
  //cout << myPort  << "agrv3" << argv[3] << endl;
  other_Workers(myPort, &otherHosts);
  assigned_Worker(myPort, &otherHosts1, &otherHosts2);
  thread ping(checkMaster,"128.194.143.156:50031");
  ping.detach();
  if(argc == 4)
  {
	  server_address = "0.0.0.0:"+(string)argv[0];
	  myPort = atoi(argv[0]);
	  other_Workers(myPort, &otherHosts);
	  assigned_Worker(myPort, &otherHosts1, &otherHosts2);
	  RunServer(server_address);
	  //Client worker1(grpc::CreateChannel(
		//host_name, grpc::InsecureChannelCredentials()));
  } 
  else if(argc == 2)
  {
	  cout << "worker 4 initial start\n";
	  server_address = "0.0.0.0:50038";
	  myPort = 50038;
	  other_Workers(myPort, &otherHosts);
	  assigned_Worker(myPort, &otherHosts1, &otherHosts2);
	  RunServer(server_address);
  }
  else if(argc == 12 || 3)//restart as empty
  {
	cout << "worker 4 restart\n";
	server_address = "0.0.0.0:"+(string)argv[0];
	myPort = atoi(argv[0]);
	other_Workers(myPort, &otherHosts);
	assigned_Worker(myPort, &otherHosts1, &otherHosts2);
	string host_name = (string)argv[1];
	Client worker(grpc::CreateChannel(
		host_name, grpc::InsecureChannelCredentials()));
	Request msg;
	int i=0;
	while(1)
	{
		msg.set_username(to_string(i));
		if(worker.Update(msg) == -1)
			break;
		msg.add_arguments("followers");
		worker.Update(msg);
		msg.add_arguments("following");
		worker.Update(msg);
		msg.add_arguments("joinTime");
		worker.Update(msg);
		i++;
	}
	msg.set_username("logic clock");
	worker.Update(msg);
	server_address = "0.0.0.0:"+(string)argv[1];
	  RunServer(server_address);
  }
  else 
  cout << myPort  << "ok" << endl;
  return 0;
}
