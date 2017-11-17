#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <stdexcept>

#include "server/Request.hpp"
#include "http/HttpStatus.hpp"
#include "server/TcpConnection.hpp"
#include "Config.hpp"
#include "Utils.hpp"
#include "error/RequestError.hpp"
#include "error/ConnectionError.hpp"
#include "error/TodoError.hpp"


Request::Request(Config const& config, TcpConnection& conn) :
    m_config(config),
    m_conn(conn)
{
    std::string request_line = parse_raw_line();
    parse_method(request_line);
    parse_route(request_line);
    parse_version(request_line);

    // the previous three parse_* calls should consume the entire line
    if (!request_line.empty())
    {
        throw RequestError(HttpStatus::BadRequest, "Malformed request-line\n");
    }

    parse_headers();
    parse_body();
}

void Request::parse_method(std::string& raw_line)
{
    std::string prefix_GET("GET");
    std::string raw_substring = raw_line.substr(0, prefix_GET.size());
    if (raw_substring.compare(prefix_GET) != 0) {
      throw RequestError(HttpStatus::MethodNotAllowed, "405 Method Not Allowd\n");
    }
    m_method = prefix_GET;
    raw_line = raw_line.substr(prefix_GET.size() + 1, raw_line.size());

    //throw TodoError("2", "You have to implement parsing methods");
}

void Request::parse_route(std::string& raw_line)
{
    std::string prefix_slash("/");
    std::string raw_substring = raw_line.substr(0, prefix_slash.size());

    if (raw_line.substr(0, prefix_slash.size()) != prefix_slash) {
      throw RequestError(HttpStatus::BadRequest, "400 Bad Request\n");
    }
    int i = 0;
    while(raw_line.at(i) != ' ') {
      m_path += raw_line.at(i);
      i++;
    }
    raw_line = raw_line.substr(i + 1, raw_line.size());
    //throw TodoError("2", "You have to implement parsing routes");
}

void Request::parse_querystring(std::string query, std::unordered_map<std::string, std::string>& parsed)
{
    throw TodoError("6", "You have to implement parsing querystrings");
}

void Request::parse_version(std::string& raw_line)
{
    std::string http10 = "HTTP/1.0";
    std::string http11 = "HTTP/1.1";
    std::string empty;
    if (raw_line.find(http10) != std::string::npos) {
      m_version = "HTTP/1.0";
      //m_conn.puts(m_version);
      //m_conn.puts(" ");
      raw_line = empty;
    }
    else {
      if (raw_line.find(http11) != std::string::npos) {
        m_version = "HTTP/1.1";
        //m_conn.puts(m_version);
        //m_conn.puts(" ");
        raw_line = empty;
      }
      else {
        throw RequestError(HttpStatus::HttpVersionNotSupported, "505 HTTP Version Not Supported");
      }
    }
}

void Request::parse_headers()
{
    std::string line = parse_raw_line();
    while(!line.empty()) {

      if (line.at(line.length() - 2) == '\r' && line.at(line.length() - 1) == '\n' && line.length() == 2) {
        return;
      }
      std::string key;
      std::string value;
      int flag = 0;
      for (size_t i = 0; i < line.size(); i++) {
        if (line.at(i) == ':') {
          flag = 1;
          i++;
        }
        if (flag == 0) {
          key = key + line.at(i);
        }
        else {
          value = value + line.at(i);
        }
      }
      m_headers.insert(std::pair<std::string, std::string>(key, value));
      line = parse_raw_line();
    }
    //throw TodoError("2", "You have to implement parsing headers");
}

void Request::parse_body()
{
    if (m_method == "GET") return;

    throw TodoError("6", "You have to implement parsing request bodies");
}

std::string Request::parse_raw_line()
{
    char * c = new char;
    std::string parsedLine = std::string("");
    int i = 0;
    while(m_conn.getc(((unsigned char *)c))) {
      if (i == m_max_buf - 1) {
        delete c;
        return parsedLine;
      }
      if (*c == '\n' && parsedLine.at(parsedLine.length() - 1) == '\r') {
        parsedLine += *c;
        i++;
        delete c;
        return parsedLine;
      }
      parsedLine += *c;
      i++;
    }
    delete c;
    return parsedLine;
    //throw TodoError("2", "You need to implement line fetching");
}

void Request::print() const noexcept
{
    std::cout << m_method << ' ' << m_path << ' ' << m_version << std::endl;
#ifdef DEBUG
    for (auto const& el : m_headers)
    {
        std::cout << el.first << ": " << el.second << std::endl;
    }

    for (auto const& el : m_query)
    {
        std::cerr << el.first << ": " << el.second << std::endl;
    }

    for (auto const& el : m_body_data)
    {
        std::cerr << el.first << ": " << el.second << std::endl;
    }
#endif
}

bool Request::try_header(std::string const& key, std::string& value) const noexcept
{
    if (m_headers.find(key) == m_headers.end())
    {
        return false;
    }
    else
    {
        value = m_headers.at(key);
        return true;
    }
}

std::string const& Request::get_path() const noexcept
{
    return m_path;
}

std::string const& Request::get_method() const noexcept
{
    return m_method;
}

std::string const& Request::get_version() const noexcept
{
    return m_version;
}

std::unordered_map<std::string, std::string> const& Request::get_headers() const noexcept
{
    return m_headers;
}

std::unordered_map<std::string, std::string> const& Request::get_query() const noexcept
{
    return m_query;
}

std::unordered_map<std::string, std::string> const& Request::get_body() const noexcept
{
    return m_body_data;
}
