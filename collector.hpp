#pragma once
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/exception/all.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include "db.hpp"
#include "client.hpp"
#define CHANNEL_UPDATE_PERIOD 10000
#define IO_CONTEXT_RUN_PERIOD 1000
#define PRINT_LOG false

using boost::asio::ip::tcp;

class Collector
{
private:
    DB *db;
    std::string db_host, db_port, db_name;
    std::string ip, port;
    std::string token, user_name;
    std::vector<Client *> clients;

public:
    Collector();
    ~Collector();
    bool read_params();
    void start(boost::asio::io_context &io_context);
};