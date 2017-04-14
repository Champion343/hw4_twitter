#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <grpc++/grpc++.h>
#include <ctime>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include "fbp.grpc.pb.h"

using grpc::Channel;
using grpc::ClientReaderWriter;
using grpc::ClientContext;
using grpc::Status;
using fbp::Reply;
using fbp::ListReply;
using fbp::Message;
using fbp::CRMasterServer;
using fbp::Request;
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
  //~Client();
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
    
	  std::string Connect() {
    // Data we are sending to the server.
    Request message;
    // Container for the data we expect from the server.
    Reply reply;

    ClientContext context;

    // The actual RPC.
    Status status = stub_->Connect(&context, message, &reply);

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
	cin.ignore();
	client_message.set_username(user);
	//send initial message declaring username
	stream->Write(client_message);
	//loop through requesting user input and send message to server
	while(1){
    getline(cin, text);
	client_message.set_msg(text);
	stream->Write(client_message);
	}
	//if we ever wanted an exit this would close the stream and exit Chat
	Status status = stream->Finish();
}
  
 private:
  std::unique_ptr<CRMasterServer::Stub> stub_;
};

int main(int argc, char** argv) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  ListReply lreply;
  string client_name;
  string input;
  string room_name;
  string host_name = "128.194.143.156";
  string port = "50031";
  host_name.append(":");
  host_name.append(port);
  //sprintf("%s:%d",host_name,port);
  client_name = argv[1];
  //create connection to server
  Client masterClient(grpc::CreateChannel(
  host_name, grpc::InsecureChannelCredentials()));
  host_name = masterClient.Connect();
  cout << host_name<<endl;
  Client workerClient(grpc::CreateChannel(
  host_name, grpc::InsecureChannelCredentials()));
  string reply = workerClient.Login(client_name);
  std::cout << "Login State: " << reply << std::endl;
  //loop command til client enters CHAT mode
  while(input != "CHAT"){
	  cout << "Please enter a command..." << endl;
	  cin >> input;
	  //Calls List function then prints all rooms and all rooms joined by client
	  if(input == "LIST"){
		lreply = workerClient.List(client_name);
		cout << "All Rooms:" << endl;
		for(int i = 0; i< lreply.all_roomes_size(); ++i)
			cout << lreply.all_roomes(i) << endl;
		cout << "Joined Rooms:" << endl;
		for(int i = 0; i< lreply.joined_roomes_size(); ++i)
			cout << lreply.joined_roomes(i) <<endl;
	  }
	  //Calls join function the prints if successful or not
	  else if(input == "JOIN"){
			cin >> room_name;
			reply = workerClient.Join(client_name, room_name);
			std::cout << "Join: " << reply << std::endl;
		}
	  //Calls leave function the prints if successful or not
	  else if (input  =="LEAVE"){
		cin >> room_name;
		reply = workerClient.Leave(client_name, room_name);
		std::cout << "Leave: " << reply << std::endl;
	  }
	  //Calls chat function and enters chat mode
	  else if(input == "CHAT"){
		workerClient.Chat(client_name);
	  }
	  else
		  cout << "Not a command..." << endl;
  }
  
  return 0;
}
