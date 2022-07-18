#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <fstream>
#include <iostream>
#include <string>

#include "db.hpp"
#include "client.hpp"
#define CHANNEL_UPDATE_DELAY 10000

int main()
{
    try
    {
        DB* db;
        string db_host, db_port;
        string db_user_id, db_user_pw, db_name;
        string ip, port;
        string token, user_name;
        boost::asio::io_service io_service;
        tcp::resolver r(io_service);
        ifstream setting("setting.txt");


        if (!setting.is_open()) {
            cout << "can't open setting.txt" << endl;
            return 0;
        }


        // db connect
        setting >> db_host >> db_port;
        setting >> db_user_id >> db_user_pw >> db_name;
        db = new DB();
        if (!db->connect(db_host, db_port, db_user_id, db_user_pw, db_name)) {
            return 0;
        }
        db->start();


        // client
        vector<Client*> clients;
        setting >> ip >> port;
        setting >> token >> user_name;
        while (1) {
            // remove stopped clinet
            for (auto p = clients.begin(); p != clients.end(); ) {
                if ((*p)->is_disposed) {
                    delete* p;
                    p = clients.erase(p);
                }
                else {
                    p++;
                }
            }

            vector<string> channels = db->get_channels();
            for (auto channel = channels.begin(); channel != channels.end(); channel++) {
                bool exists = false;
                for (auto p = clients.begin(); p != clients.end(); p++) {
                    if ((*p)->channel.compare(*channel) == 0) {
                        exists = true;
                        break;
                    }
                }

                if (!exists) {
                    Client* client = new Client(io_service, token, user_name, *channel, db);
                    client->start(io_service, r.resolve(tcp::resolver::query(ip, port)));
                    clients.push_back(client);
                    std::this_thread::sleep_for(std::chrono::milliseconds(JOIN_DELAY));
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(CHANNEL_UPDATE_DELAY));
        }

    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}