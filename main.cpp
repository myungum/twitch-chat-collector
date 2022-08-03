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

int main()
{
    try
    {
        DB* db;
        string db_host, db_port, db_name;
        string ip, port;
        string token, user_name;
        boost::asio::io_context io_context;
        tcp::resolver r(io_context);
        ifstream setting("setting.txt");
        
        bool running = false;

        if (!setting.is_open()) {
            cout << "can't open setting.txt" << endl;
            return 0;
        }


        // db connect
        setting >> db_host >> db_port >> db_name;
        db = new DB(db_host, db_port, db_name);
        db->start();


        // client
        vector<Client*> clients;
        setting >> ip >> port;
        setting >> token >> user_name;

        // threads never die~
        boost::thread_group threads;
        for (unsigned i = 0; i < thread::hardware_concurrency(); ++i) {
            boost::thread th([&io_context] {
                try {
		while (1) {
                    boost::this_thread::sleep(boost::posix_time::millisec(IO_CONTEXT_RUN_PERIOD));
                    cout << "io_context.run();";
                    io_context.run();
                }
		}
		catch (boost::exception const& e) {
		std::cerr << "Exception : " << boost::diagnostic_information(e) << std::endl;
		}
            });
            threads.add_thread(&th);
        }

        while (1) {
            // remove stopped clinet
	        cout << "start garbage collect\n";
            for (auto p = clients.begin(); p != clients.end(); ) {
                if ((*p)->is_disposed) {
                    delete* p;
                    p = clients.erase(p);
                }
                else {
                    p++;
                }
            }
	        cout << "start add channels\n";
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
                    Client* client = new Client(io_context, token, user_name, *channel, db);
                    client->start(io_context, r.resolve(tcp::resolver::query(ip, port)));
                    clients.push_back(client);
                    std::this_thread::sleep_for(std::chrono::milliseconds(JOIN_DELAY));
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(CHANNEL_UPDATE_PERIOD));
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
