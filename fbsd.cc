#include <iostream>
#include <memory>
#include <string>

#include <grpc++/grpc++.h>

#include "fpb.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using fpb::Request;
using fpb::Reply;
using fpb::ListReply;
using fpb::Message;
using fbp::CRMasterServer;

// Logic and data behind the server's behavior.
class FBServiceImpl final : public CRMasterServer::Service {
  Status Login(ServerContext* context, const Request* request,
                  Reply* reply) override {
    createChatroom(request->username);
    return Status::OK;
  }
};

// Logic and data behind the server's behavior.
class FBServiceImpl final : public CRMasterServer::Service {
  Status List(ServerContext* context, const Request* request,
                  ListReply* reply) override {
    getList(request->username);
    return Status::OK;
  }
};

// Logic and data behind the server's behavior.
class FBServiceImpl final : public CRMasterServer::Service {
  Status Join(ServerContext* context, const Request* request,
                  Reply* reply) override {
    joinChatroom(request->username, request->arguments.(0));
    return Status::OK;
  }
};

// Logic and data behind the server's behavior.
class FBServiceImpl final : public CRMasterServer::Service {
  Status Leave(ServerContext* context, const Request* request,
                  Reply* reply) override {
    leaveChatroom(request->username, request->arguments.(0));
    return Status::OK;
  }
};

// Logic and data behind the server's behavior.
class FBServiceImpl final : public CRMasterServer::Service {
  Status Chat(ServerContext* context, const Message* msg,
                  Message* reply) override {
    bistreamMsg(request->username, request->arguments.(0));
    return Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  FBServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}

