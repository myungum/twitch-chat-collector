#include "db.hpp"

std::string DB::cur_date()
{
    char buf_time[20];
    time_t raw_time;
    struct tm *time_info;

    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(buf_time, sizeof(buf_time), "%Y-%m-%d", time_info);
    return std::string(buf_time);
}

std::string DB::cur_time()
{
    char buf_time[20];
    time_t raw_time;
    struct tm *time_info;

    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(buf_time, sizeof(buf_time), "%H:%M:%S", time_info);
    return std::string(buf_time);
}

std::string DB::cur_datetime()
{
    return cur_date() + " " + cur_time();
}

void DB::insert_loop()
{

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(INSERT_PERIOD));

        int doc_cnt = 0;
        std::vector<bsoncxx::document::value> docs;

        mtx_queue.lock();
        doc_cnt = doc_queue.size();
        mtx_queue.unlock();

        if (PRINT_INSERT_COUNT)
        {
            std::cout << doc_cnt << "..." << std::flush;
        }

        // save db status
        auto client = pool.acquire();
        auto db_status = bsoncxx::builder::stream::document{}
                         << "chats_per_sec" << doc_cnt
                         << "datetime" << cur_datetime()
                         << bsoncxx::builder::stream::finalize;
        (*client)[db_name]["status"].insert_one(db_status.view());

        if (doc_cnt <= 0)
        {
            continue;
        }

        // queue -> vector
        for (int i = 0; i < doc_cnt; i++)
        {
            mtx_queue.lock();
            docs.push_back(doc_queue.front());
            doc_queue.pop();
            mtx_queue.unlock();
        }
        (*client)[db_name]["chats"].insert_many(docs);
    }
}

DB::DB(std::string host, std::string port, std::string db_name)
{
    this->host = host;
    this->port = port;
    this->db_name = db_name;
}

DB::~DB()
{
}

void DB::insert(std::string channel, std::string user_name, std::string chat_text)
{
    auto builder = bsoncxx::builder::stream::document{};
    bsoncxx::document::value doc_value = builder
                                         << "channel" << channel
                                         << "user" << user_name
                                         << "text" << chat_text
                                         << "date" << cur_date()
                                         << "time" << cur_time()
                                         << bsoncxx::builder::stream::finalize;

    mtx_queue.lock();
    if (doc_queue.size() < INSERT_QUEUE_MAX)
    {
        doc_queue.push(doc_value);
    }
    mtx_queue.unlock();
}

std::vector<std::string> DB::get_channels()
{
    std::vector<std::string> channels;

    auto client = pool.acquire();
    mongocxx::cursor cursor = (*client)[db_name]["live_channels"].find({});

    for (auto doc : cursor)
    {
        bsoncxx::document::element channel = doc["user_login"];
        channels.push_back(channel.get_utf8().value.to_string());
    }
    return channels;
}

void DB::start()
{
    th = std::thread(&DB::insert_loop, this);
    th.detach();
}

void DB::join()
{
    th.join();
}
