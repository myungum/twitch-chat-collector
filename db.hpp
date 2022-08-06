#pragma once
#define INSERT_PERIOD 1000
#define INSERT_QUEUE_MAX 300
#include <string>
#include <mutex>
#include <queue>
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>
#include <ctime>
#include <chrono>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

using namespace std;

class DB
{
private:
    string host, port, db_name;
    mongocxx::instance instance{};
    mongocxx::pool pool{mongocxx::uri{}};
    mutex mtx_queue;
    thread th;
    queue<bsoncxx::document::value> doc_queue;
    time_t rawtime;
    struct tm *timeinfo;

    string cur_time();
    void insert_loop();

public:
    DB(string host, string port, string db_name);
    ~DB();
    void insert(string channel, string user_name, string chat_text);
    vector<string> get_channels();
    void start();
    void join();
};
