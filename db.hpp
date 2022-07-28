#pragma once
#define SQL_BUF_SIZE 1024 * 1000
#define INSERT_PERIOD 1000
#include <string>
#include <mutex>
#include <queue>
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>
#include <ctime>
#include <chrono>
#include "/usr/include/mysql/mysql.h"

using namespace std;

class DB {
private:
    MYSQL conn_opt_insert, conn_opt_select;
    MYSQL* conn_insert, * conn_select;
    mutex m;
    thread th;
    queue<string*> sql_queue;
    time_t rawtime;
    struct tm* timeinfo;
    
    string cur_time();
    void insert_loop();

public:
    DB();
    ~DB();
    bool connect(string host, string port, string user_id, string user_pw, string db_name);
    void insert(string channel, string user_name, string chat_text);
    vector<string> get_channels();
    void start();
    void join();
};
