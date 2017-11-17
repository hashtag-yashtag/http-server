#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <stdexcept>
#include <signal.h>

#include "Utils.hpp"
#include "server/TcpConnection.hpp"
#include "server/Server.hpp"
#include "server/Request.hpp"
#include "server/Response.hpp"
#include "controller/Controller.hpp"
#include "controller/SendFileController.hpp"
#include "controller/TextController.hpp"
#include "controller/ExecScriptController.hpp"
#include "http/HttpStatus.hpp"
#include "error/RequestError.hpp"
#include "error/ResponseError.hpp"
#include "error/ControllerError.hpp"
#include "error/SocketError.hpp"
#include "error/ConnectionError.hpp"
#include "error/TodoError.hpp"

struct protoent *getprotobyname(const char * name);

struct protoent * ptrp = getprotobyname("tcp");

int socketOption, bindSocket, listener;

Server::Server(Config const& config) : m_config(config)
{
    struct sockaddr_in serverAddress;

    m_master = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (m_master < 0) {
      throw SocketError("Socket Connection Failed");
    }

    //Reuse Socket Option for quick reuse
    int optionValue = 1;
    socketOption = setsockopt(m_master, SOL_SOCKET, SO_REUSEADDR, (char *)&optionValue, sizeof(int));

    if (socketOption == -1) {
      throw SocketError("Socket Option Failed");
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(m_config.port);
    serverAddress.sin_addr.s_addr = htons(0);

    int bindSocket = bind(m_master, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (bindSocket == -1) {
      throw SocketError("Bind Failed");
    }

    int listener = listen(m_master, m_config.queue_length);
    if (listener == -1) {
      throw SocketError("listen failed");
    }
}

void Server::run_linear() const
{
    while (true)
    {
        std::string response;
        TcpConnection* conn = new TcpConnection(m_config, m_master);
        handle(conn);

        delete conn;
    }
}


void Server::run_thread_request() const
{
  while (1) {
    TcpConnection* conn = new TcpConnection(m_config, m_master);
    if (m_master >= 0) {

      std::thread _thread([this, conn] () -> void {
        this -> handle(conn);
        delete conn;
      });

      _thread.detach();
    }
  }
}

void Server::run_fork() const
{
    while (1) {
      TcpConnection* connection = new TcpConnection(m_config, m_master);
      if (m_master >= 0) {
        int ret = fork();
        if (ret == 0) {
          signal(SIGCHLD, SIG_DFL);
          handle(connection);
          delete connection;
          close(m_master);
          exit(0);
        }
        else {
          signal(SIGCHLD, SIG_IGN);
        }
      }
      delete connection;
    }
}


void Server::run_thread_pool() const
{
    std::thread threads[m_config.threads];
    for (int i = 0; i < m_config.threads; i++) {
      threads[i] = std::thread([this] { this -> run_linear(); });
    }
    for (int i = 0; i < m_config.threads; i++) {
      threads[i].join();
    }
}

void Server::handle(TcpConnection* conn) const
{

    Controller const* controller = nullptr;

    try
    {
        // creating req will parse the incoming request
        Request req(m_config, *conn);

        // creating res as an empty response
        Response res(m_config, *conn);

        // Printing the request will be helpful to tell what our server is seeing
        req.print();

        std::string path = req.get_path();

        // This will route a request to the right controller
        // You only need to change this if you rename your controllers or add more routes
        if (path == "/hello-world")
        {
            controller = new TextController(m_config, "Hello world!\n");
        }
        else if (path.find("/script") == 0)
        {
            controller = new ExecScriptController(m_config, "/script");
        }
        else
        {
            controller = new SendFileController(m_config);
        }

        // Whatever controller we picked needs to be run with the given request and response
        controller->run(req, res);
    }
    catch (RequestError const& e)
    {
        d_warnf("Error parsing request: %s", e.what());

        Controller::send_error_response(m_config, conn, e.status, "Error while parsing request\n");
    }
    catch (ControllerError const& e)
    {
        d_warnf("Error while handling request: %s", e.what());

        Controller::send_error_response(m_config, conn, HttpStatus::InternalServerError, "Error while handling request\n");
    }
    catch (ResponseError const& e)
    {
        d_warnf("Error while creating response: %s", e.what());

        Controller::send_error_response(m_config, conn, HttpStatus::InternalServerError, "Error while handling response\n");
    }
    catch (ConnectionError const& e)
    {
        // Do not try to write a response when we catch a ConnectionError, because that will likely just throw
        d_errorf("Connection error: %s", e.what());
    }
    catch (TodoError const& e)
    {
        d_errorf("You tried to use unimplemented functionality: %s", e.what());
    }

    // Dont forget about freeing memory!
    delete controller;
}

Server::~Server() noexcept
{
    if (close(m_master) == -1)
    {
        d_error("Could not close master socket");
    }
    if (close(socketOption) == -1)
    {
        d_error("Could not close socketOption");
    }
    if (close(bindSocket) == -1)
    {
        d_error("Could not close bindSocket");
    }
    if (close(listener) == -1)
    {
        d_error("Could not close listener");
    }
}
