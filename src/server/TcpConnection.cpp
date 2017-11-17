#include <string>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>

#include "Utils.hpp"
#include "Config.hpp"
#include "server/TcpConnection.hpp"
#include "error/ConnectionError.hpp"
#include "error/SocketError.hpp"
#include "error/TodoError.hpp"
#include "error/RequestError.hpp"

TcpConnection::TcpConnection(Config const& config, int master_fd) :
m_config(config),
m_master(master_fd),
m_shutdown(false)
{
    m_conn = accept(m_master, NULL, NULL);
    if (m_conn < 0) {
      throw SocketError("Accept Failed");
    }
}

TcpConnection::~TcpConnection() noexcept
{
    d_printf("Closing connection on %d", m_conn);

    if (close(m_conn) == -1) d_errorf("Could not close connection %d", m_conn);
}

void TcpConnection::shutdown()
{
    d_printf("Shutting down connection on %d", m_conn);

    if (::shutdown(m_conn, SHUT_RDWR) == -1) d_errorf("Could not shut down connection %d", m_conn);

    m_shutdown = true;
}

bool TcpConnection::getc(unsigned char* c)
{
    int n = read(m_conn, c, 1);
    if (n > 0) {
      return true;
    }
    return false;
    //throw TodoError("2", "You have to implement reading from connections");
}

void TcpConnection::putc(unsigned char c)
{
    write(m_conn, &c, 1);
}

void TcpConnection::puts(std::string const& str)
{
    size_t size = str.length();
    char * newStr = (char *) malloc (size * sizeof(char *));
    for (size_t i = 0; i < size; i++) {
      newStr[i] = str.at(i);
    }
    write(m_conn, newStr, size);
    free(newStr);
}

void TcpConnection::putbuf(void const* buf, size_t bufsize)
{
    write(m_conn, buf, bufsize);
}
