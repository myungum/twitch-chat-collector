#pragma once
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/throw_exception.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include "db.hpp"
#define CONNECT_TIMEOUT 180000
#define READ_TIMEOUT 180000
#define PRINT_SOCKET_MSG true
#define PRINT_NOTICE_MSG false
#define PRINT_EXCEPTION_MSG true
using boost::asio::ip::tcp;

class Client
{
public:
    std::string token, user_name, channel;

    Client(boost::asio::io_service &io_service, std::string token, std::string user_name, std::string channel, DB *db);
    void start(boost::asio::io_service &io_service, tcp::resolver::iterator endpoint_iter);
    bool is_closed();
    void stop();

private:
    void start_connect(tcp::resolver::iterator endpoint_iter);
    void handle_connect(const boost::system::error_code &ec, tcp::resolver::iterator endpoint_iter);
    void start_read();
    void write(std::string msg);
    void handle_read(const boost::system::error_code &ec);
    std::vector<std::string> split(const std::string &msg);
    void handle_write(const boost::system::error_code &ec);
    void check_deadline();

    tcp::socket sck;
    boost::asio::streambuf input_buf;
    boost::asio::deadline_timer deadline;
    DB *db;
    std::mutex mtx;
    bool is_stopped;
};
