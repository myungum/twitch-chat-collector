#include "collector.hpp"

Collector::Collector()
{
}

Collector::~Collector()
{
}

bool Collector::read_params()
{
    std::ifstream setting("setting.txt");
    if (!setting.is_open())
    {
        std::cout << "can't open setting.txt" << std::endl;
        return false;
    }

    setting >> db_host >> db_port >> db_name;
    setting >> ip >> port;
    setting >> token >> user_name;
    return true;
}

void Collector::start(boost::asio::io_context &io_context)
{
    // 1. db start
    db = new DB(db_host, db_port, db_name);
    db->start();

    // 2. make threads
    boost::thread_group threads;
    for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i)
    {
        boost::thread th([&io_context]
                         {
                while (1) {
                    boost::this_thread::sleep(boost::posix_time::millisec(IO_CONTEXT_RUN_PERIOD));
                    std::cout << "io_context.run();";
                    io_context.run();
                } });
        threads.add_thread(&th);
    }

    // 3. loop
    tcp::resolver r(io_context);
    while (1)
    {
        // remove stopped clinet
        if (PRINT_LOG)
        {
            std::cout << "start garbage collect\n";
        }

        for (auto p = clients.begin(); p != clients.end();)
        {
            if ((*p)->is_closed())
            {
                delete *p;
                p = clients.erase(p);
            }
            else
            {
                p++;
            }
        }

        if (PRINT_LOG)
        {
            std::cout << "start add channels\n";
        }
        std::vector<std::string> channels = db->get_channels();
        for (auto channel = channels.begin(); channel != channels.end(); channel++)
        {
            bool exists = false;
            for (auto p = clients.begin(); p != clients.end(); p++)
            {
                if ((*p)->channel.compare(*channel) == 0)
                {
                    exists = true;
                    break;
                }
            }

            if (!exists)
            {
                Client *client = new Client(io_context, token, user_name, *channel, db);
                client->start(io_context, r.resolve(tcp::resolver::query(ip, port)));
                clients.push_back(client);
                std::this_thread::sleep_for(std::chrono::milliseconds(JOIN_DELAY));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(CHANNEL_UPDATE_PERIOD));
    }
}