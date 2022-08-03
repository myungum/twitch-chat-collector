#include "db.hpp"

string DB::cur_time() {
    char buf_time[20];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buf_time, sizeof(buf_time), "%Y-%m-%d %H:%M:%S", timeinfo);
    return string(buf_time);
}

void DB::insert_loop() {
    
    while (1) {
        try {
            this_thread::sleep_for(chrono::milliseconds(INSERT_PERIOD));

            int scnt = 0;
            vector<bsoncxx::document::value> docs;

            mtx_queue.lock();
            scnt = doc_queue.size();
            mtx_queue.unlock();
            cout << scnt << "..." << std::flush; 
	        if (scnt <= 0)
                continue;

            for (int sidx = 0; sidx < scnt; sidx++) {
                mtx_queue.lock();
                docs.push_back(doc_queue.front());
                doc_queue.pop();
                mtx_queue.unlock();
            }
            mtx_client.lock();
            db["chats"].insert_many(docs);
            mtx_client.unlock();
        }
        catch (int e) {
            printf("DB::insert_loop() ERROR (%d)\n", e);
            return;
        }
    }
}

DB::DB(string host, string port, string db_name) {
    client = mongocxx::client{mongocxx::uri("mongodb://" + host + ":" + port)};
    db = client[db_name];
}

DB::~DB() {
}

void DB::insert(string channel, string user_name, string chat_text) {
	try{
    auto builder = bsoncxx::builder::stream::document{};
    bsoncxx::document::value doc_value = builder
    << "channel" << channel
    << "user" << user_name
    << "text" << chat_text
    << "time" << cur_time()
    << bsoncxx::builder::stream::finalize;

    mtx_queue.lock();
    if (doc_queue.size() < INSERT_QUEUE_MAX) {
    	doc_queue.push(doc_value);
    }
    mtx_queue.unlock();
	}
        catch (int e) {
            printf("DB::insert() ERROR (%d)\n", e);
            return;
        }

}

vector<string> DB::get_channels() {
    vector<string> channels;
        
    mtx_client.lock();
    mongocxx::cursor cursor = db["live_channels"].find({});
    mtx_client.unlock();

    for(auto doc : cursor) {
        bsoncxx::document::element channel = doc["user_login"];
        channels.push_back(channel.get_utf8().value.to_string());
    }
    return channels;
}

void DB::start() {
    th = thread(&DB::insert_loop, this);
    th.detach();
}

void DB::join() {
    th.join();
}
