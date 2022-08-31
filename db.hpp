#pragma once
#define INSERT_PERIOD 1000
#define INSERT_QUEUE_MAX 300
#define PRINT_INSERT_COUNT true
#include <string>
#include <mutex>
#include <queue>
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

class DB
{
private:
    std::string host, port, db_name;
    mongocxx::instance instance{};
    mongocxx::pool pool{mongocxx::uri{}};
    std::mutex mtx_queue;
    std::thread th;
    std::queue<bsoncxx::document::value> doc_queue;
    
    std::string cur_date();
    std::string cur_time();
    std::string cur_datetime();
    void insert_loop();

public:
    DB(std::string host, std::string port, std::string db_name);
    ~DB();
    void insert(std::string channel, std::string user_name, std::string chat_text);
    std::vector<std::string> get_channels();
    void start();
    void join();
};
