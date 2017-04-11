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

#include <thread>
#include "fbp.grpc.pb.h"

#include <thread>
#include <grpc++/grpc++.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

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

//Helper function used to create a Message object given a username and message
Message MakeMessage(const std::string& username, const std::string& msg) {
  Message m;
  m.set_username(username);
  m.set_msg(msg);
  google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
  struct timeval tv;
  gettimeofday(&tv, NULL);
  timestamp->set_seconds(tv.tv_sec);
  timestamp->set_nanos(tv.tv_usec * 1000);
  //timestamp->set_seconds(time(NULL));
  //timestamp->set_nanos(0);
  m.set_allocated_timestamp(timestamp);
  google::protobuf::int64 t1; 
	t1 = google::protobuf::util::TimeUtil::TimestampToNanoseconds(*timestamp);
	cout << " timestamp " << t1 << endl;
  return m;
}

//thread handles reading for bidirectional streaming
void reading(shared_ptr<ClientReaderWriter<Message,Message>> stream)
{
	Message server_message;
	while(stream->Read(&server_message)) {//blocking;
		cout << server_message.username()<<": " <<server_message.msg() << endl;
		
	}
}
//Client object used for grpc calls
class Client {
 public:
  Client(std::shared_ptr<Channel> channel)
      : stub_(CRMasterServer::NewStub(channel)) {}

  //Login function send message containing username and receives response
  std::string Login(const std::string& user) {
    // Data we are sending to the server.
    Request message;
    message.set_username(user);
    // Container for the data we expect from the server.
    Reply reply;
    ClientContext context;

    // The actual RPC.
    Status status = stub_->Login(&context, message, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.msg();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }
	//Join function sends a message containing username and the room that is being
	//joined then returns server response
    std::string Join(const std::string& user, std::string& room) {
    // Data we are sending to the server.
    Request message;
    message.set_username(user);
	message.add_arguments(room);
    // Container for the data we expect from the server.
    Reply reply;

    ClientContext context;

    // The actual RPC.
    Status status = stub_->Join(&context, message, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.msg();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }
    //Leave function sends a message containing username and the room that is being
	//left then returns server response
    std::string Leave(const std::string& user, std::string& room) {
    // Data we are sending to the server.
    Request message;
    message.set_username(user);
	message.add_arguments(room);
    // Container for the data we expect from the server.
    Reply reply;

    ClientContext context;

    // The actual RPC.
    Status status = stub_->Leave(&context, message, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.msg();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }
    
    ListReply List(const std::string& user) {
    // Data we are sending to the server.
    Request message;
    message.set_username(user);
    // Container for the data we expect from the server.
    ListReply reply;

    ClientContext context;

    // The actual RPC.
    Status status = stub_->List(&context, message, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << "RPC failed" << std::endl;
      return reply;
    }
  }
  //Chat function opens a bidirectional stream to the server then sends
  //its username and begins reading and writing to the server
  void Chat(const std::string& user){
	ClientContext context;
	//bidirectional streaming
	std::shared_ptr<ClientReaderWriter<Message,Message>> stream(stub_->Chat(&context));
	string text;
	Message client_message;
	//reading thread
	thread readMsg(reading, stream);
	cout << "Begin Chatting..." << endl;
	//remove anything left over from the command line
	client_message = MakeMessage(user, "");
	//send initial message declaring username
	stream->Write(client_message);
	//loop through requesting user input and send message to server
	unsigned int microseconds;
	string input;
	cin >> input;//wait before entering for loop
	//here we send 50 messages
	for(int i = 0; i <50; i++){
	microseconds = 500;//time interval
	
	usleep(microseconds);
    client_message = MakeMessage(user, "message");
	stream->Write(client_message);
	}
	//if we ever wanted an exit this would close the stream and exit Chat
	Status status = stream->Finish();
}
  
 private:
  std::unique_ptr<CRMasterServer::Stub> stub_;
};

//find exsiting name in a vector, get index
int findName(string username, vector<string>* list)
{
	for(int i=0; i < (int)list->size(); i++)
		if(username == list->at(i))
			return i;
	return -1;
}

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
		now = time(0);
		string date = ctime(&now);
		string hms = date.substr(date.find(":") -2, date.find_last_of(":") +3 - (date.find(":") -2));
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
	cout << "created room: " << username << endl;
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

//overrides of proto
class FBServiceImpl final : public CRMasterServer::Service 
{
	
	bool first = true;
	int last = 0;
	
	// Connect
    Status Connect(ServerContext* context, const Request* request, Reply* reply) 
	override 
	{
		cout << "assigning worker: " << request->username() << endl;
		if(createChatroom(request->username()))
			reply->set_msg("server assigned worker");
		else
			reply->set_msg("server no assigned worker");
		return Status::OK;	
	}
	
	// Login
    Status Login(ServerContext* context, const Request* request, Reply* reply) 
	override 
	{
		cout << "creating room: " << request->username() << endl;
		if(createChatroom(request->username()))
			reply->set_msg("server created room");
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
		cout << request->username() + " tryna join "+request->arguments(0) << endl;
		int user, joining;
		user = findName(request->username(), &chatRooms);
		joining = findName(request->arguments(0), &chatRooms);
		if( user < 0 || joining < 0)
		{
			reply->set_msg("join fail");
			cout << "join fail1" << endl;
		}
		else if( chatRooms[user].addFriend(request->arguments(0)) &&
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
	Status Leave(ServerContext* context, const Request* request, Reply* reply) 
	override 
	{
		//delete person from user's friends, delete user from person's followers
		cout << request->username() << " tryna leave " << request->arguments(0) << endl;
		int user, leaving;
		user = findName(request->username(), &chatRooms);
		leaving = findName(request->arguments(0), &chatRooms);
		if( user < 0 || leaving < 0)
		{
			cout << "leave fail1" << endl;
			reply->set_msg("leave fail");
		}
		else if( chatRooms[user].unfriend(request->arguments(0)) &&
				 chatRooms[leaving].unfollowedBy(request->username()) )
				 {
			cout << "leave success" << endl;
					reply->set_msg("leave success");
				 }
		else{
			cout << "leave fail2" << endl;
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
			cout << "opened file for reading"<< endl;
			else
				cout << "NO OPEN FILE for reading (╯°□°)╯︵ ┻━┻" << endl;//not necessarily an error
			//go thru current subscription's file
			while(getline(file, line))
			{
				placeIn(line, &recentMsgs, chatRooms[index].joinTime[j]);
			}
			cout << "closing file" << endl;
			file.close();
		}
		//no recent messages from subscriptions
		if(recentMsgs.size() == 0)
		{
			cout << "wrting nothing for last 20 msgs" << endl;
			reply20.set_msg("no recent msgs");
			stream->Write(reply20);
		}
		cout << "reply 20 size " << recentMsgs.size() << endl;
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
			cout << "opened file for writing"<< endl;
		else
			cout << "NO OPEN FILE for writing (╯°□°)╯︵ ┻━┻" << endl;
		string lineMsg;
		time_t nowtime;
		string date;
		string hms;
		int k;
		google::protobuf::Timestamp* temptime;
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
					cout << google::protobuf::util::TimeUtil::ToString(*temptime) << endl;
				}
		
				lineMsg.clear();
				//time stamp
				nowtime = time(0);
				date = ctime(&nowtime);
				hms = date.substr(date.find(":") -2, date.find_last_of(":") +3 - (date.find(":") -2));
				//format: username time message
				lineMsg = user + ' ' + hms + ' ' + note.msg();
				file << lineMsg << endl;
				cout << "added to file: " << lineMsg << endl;
				//loop thru followers and post to their screens
				for(int i=0; i < (int)chatRooms[index].followers.size(); i++)
				{
					k = findName(chatRooms[index].followers[i], &chatRooms);
					if(index == k) //do not post to self
						continue;
					cout << "writing" << endl;
					if(chatRooms[k].stream != NULL)
					{
						chatRooms[k].stream->Write(note);
						
					}
					else
						cout << "null stream" << endl; //follower has not called CHAT yet
				}
				++last;
				//wait for 50 messages
				if(last == 50){
					
				google::protobuf::Timestamp* temptime2 = new google::protobuf::Timestamp();
				struct timeval tv;
				gettimeofday(&tv, NULL);
				temptime2->set_seconds(tv.tv_sec);
				temptime2->set_nanos(tv.tv_usec * 1000);
				cout << google::protobuf::util::TimeUtil::ToString(*temptime2) << endl;
				}
				/*google::protobuf::Timestamp msgtime = note.timestamp();
				google::protobuf::Timestamp* temptime = new google::protobuf::Timestamp();
				struct timeval tv;
				gettimeofday(&tv, NULL);
				temptime->set_seconds(tv.tv_sec);
				temptime->set_nanos(tv.tv_usec * 1000);
				//temptime->set_seconds(time(NULL));
				//temptime->set_nanos(0);
				google::protobuf::int64 t1; 
			    google::protobuf::int64 t2;
				t1 = google::protobuf::util::TimeUtil::TimestampToNanoseconds(msgtime);
				t2 = google::protobuf::util::TimeUtil::TimestampToNanoseconds(*temptime);
				string s1= google::protobuf::util::TimeUtil::ToString(google::protobuf::util::TimeUtil::NanosecondsToDuration(
					google::protobuf::util::TimeUtil::TimestampToNanoseconds(msgtime)));
				string s2= google::protobuf::util::TimeUtil::ToString(google::protobuf::util::TimeUtil::NanosecondsToDuration(
					google::protobuf::util::TimeUtil::TimestampToNanoseconds(*temptime)));
				cout << s1 << endl << s2 << endl;
				cout << google::protobuf::util::TimeUtil::ToString(msgtime) << " "
					 << google::protobuf::util::TimeUtil::ToString(*temptime) << endl;
				cout << t1 << endl << t2 << endl << "difference " << t2-t1 << endl;
				cout << atof(s2.c_str()) - atof(s1.c_str()) << endl;*/
			}
		}
		file.close();
		cout << "out of while" << endl;
		return Status::OK;
	}
  
};

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
  cout << "Server listening on " << server_address << endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) 
{
  string server_address("0.0.0.0:50032");
  if(argc == 2)
  {
	  server_address = "0.0.0.0:"+(string)argv[1];
  }
  else
  {
	cout << "default port 0.0.0.0:50032" << endl;
  }
  //extra agruement to determine replica?
  RunServer(server_address);
  
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  ListReply lreply;
  string client_name;
  string input;
  string room_name;
  string host_name = "";
  string port = "";
  host_name.append(":");
  host_name.append(port);
  //sprintf("%s:%d",host_name,port);
  client_name = "";
  //create connection to server
  Client client(grpc::CreateChannel(
      host_name, grpc::InsecureChannelCredentials()));
  std::string reply = client.Login(client_name);
  std::cout << "Login State: " << reply << std::endl;
  
  return 0;
}
