#include "db.hpp"

string DB::cur_time()
{
    char buf_time[20];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buf_time, sizeof(buf_time), "%Y-%m-%d %H:%M:%S", timeinfo);
    return string(buf_time);
}

void DB::insert_loop()
{

    while (1)
    {
        this_thread::sleep_for(chrono::milliseconds(INSERT_PERIOD));

        int scnt = 0;
        vector<bsoncxx::document::value> docs;

        mtx_queue.lock();
        scnt = doc_queue.size();
        mtx_queue.unlock();

        if (PRINT_INSERT_COUNT) {
            std::cout << scnt << "..." << std::flush;
        }

        if (scnt <= 0) {
            continue;
        }

        for (int sidx = 0; sidx < scnt; sidx++)
        {
            mtx_queue.lock();
            docs.push_back(doc_queue.front());
            doc_queue.pop();
            mtx_queue.unlock();
        }
        
        auto client = pool.acquire();
        (*client)[db_name]["chats"].insert_many(docs);
    }
}

DB::DB(string host, string port, string db_name)
{
    this->host = host;
    this->port = port;
    this->db_name = db_name;
}

DB::~DB()
{
}

void DB::insert(string channel, string user_name, string chat_text)
{
    auto builder = bsoncxx::builder::stream::document{};
    bsoncxx::document::value doc_value = builder
        << "channel" << channel
        << "user" << user_name
        << "text" << chat_text
        << "time" << cur_time()
        << bsoncxx::builder::stream::finalize;

    mtx_queue.lock();
    if (doc_queue.size() < INSERT_QUEUE_MAX)
    {
        doc_queue.push(doc_value);
    }
    mtx_queue.unlock();
}

vector<string> DB::get_channels()
{
    vector<string> channels;

    auto client = pool.acquire();
    mongocxx::cursor cursor = (*client)[db_name]["live_channels"].find({});

    for (auto doc : cursor)
    {
        bsoncxx::document::element channel = doc["user_login"];
        channels.push_back(channel.get_utf8().value.to_string());
    }
    return channels;
}

void DB::insert_

void DB::start()
{
    th = thread(&DB::insert_loop, this);
    th.detach();
}

void DB::join()
{
    th.join();
}
