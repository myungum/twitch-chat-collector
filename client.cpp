#include "client.hpp"

Client::Client(boost::asio::io_context &io_context, std::string token, std::string user_name, std::string channel, DB *db)
    : is_stopped(false),
      sck(io_context),
      deadline(io_context)
{
    this->token = token;
    this->user_name = user_name;
    this->channel = channel;
    this->db = db;
}

void Client::start(boost::asio::io_context &io_context, tcp::resolver::iterator endpoint_iter)
{
    start_connect(endpoint_iter);
    deadline.async_wait(boost::bind(&Client::check_deadline, this));
}

bool Client::is_closed()
{
    bool result = false;
    mtx.lock();
    result = is_stopped;
    mtx.unlock();
    return result;
}

void Client::stop()
{
    if (!is_stopped)
    {
        is_stopped = true;
        sck.close();
        deadline.cancel();
        if (PRINT_ABOUT_SOCKET) {
            std::cout << "Stopped : " << channel << "\n";
        }
    }
}

void Client::start_connect(tcp::resolver::iterator endpoint_iter)
{
    if (endpoint_iter != tcp::resolver::iterator())
    {
        if (PRINT_ABOUT_SOCKET) {
            std::cout << "Trying " << endpoint_iter->endpoint() << "(" << channel + ")\n";
        }
        deadline.expires_from_now(boost::posix_time::milliseconds(CONNECT_TIMEOUT));
        sck.async_connect(endpoint_iter->endpoint(),
                          boost::bind(&Client::handle_connect,
                                      this, _1, endpoint_iter));
    }
    else
    {
        stop();
    }
}

void Client::handle_connect(const boost::system::error_code &ec,
                            tcp::resolver::iterator endpoint_iter)
{
    mtx.lock();
    if (!is_stopped)
    {
        if (!sck.is_open())
        {
            std::cout << "Connect timed out\n";
            start_connect(++endpoint_iter);
        }

        else if (ec)
        {
            std::cout << "Connect error: " << ec.message() << "\n";
            sck.close();
            start_connect(++endpoint_iter);
        }
        else
        {
            if (PRINT_ABOUT_SOCKET) {
                std::cout << "Connected to " << endpoint_iter->endpoint() << "(" << channel + ")\n";
            }
            start_read();
            write("CAP REQ :twitch.tv/commands\r\n");
            write("PASS " + token + "\r\n");
            write("NICK " + user_name + "\r\n");
            write("JOIN #" + channel + "\r\n");
        }
    }
    mtx.unlock();
}

void Client::start_read()
{
    // Set a deadline for the read operation.
    deadline.expires_from_now(boost::posix_time::milliseconds(READ_TIMEOUT));

    // Start an asynchronous operation to read a newline-delimited message.
    boost::asio::async_read_until(sck, input_buf, '\n',
                                  boost::bind(&Client::handle_read, this, _1));
}

void Client::write(std::string msg)
{

    boost::asio::async_write(sck, boost::asio::buffer(msg.c_str(), msg.length()),
                             boost::bind(&Client::handle_write, this, _1));
}

void Client::handle_read(const boost::system::error_code &ec)
{
    mtx.lock();
    if (!is_stopped)
    {
        if (!ec)
        {
            // Extract the newline-delimited message from the buffer.
            std::string line;
            std::istream is(&input_buf);
            std::getline(is, line);

            // Empty messages are heartbeats and so ignored.
            if (!line.empty())
            {
                if (line.substr(0, 4).compare("PING") == 0)
                {
                    write("PONG " + line.substr(5, line.length() - 5) + "\n");
                }
                else {
                    std::vector<std::string> args = split(line);
                    if (args.size() > 3 && args[1].compare("PRIVMSG") == 0)
                    {
                        std::string user_name = args[0].substr(1, args[0].find('!') - 1);
                        std::string chat_text = args[3].substr(1, args[3].length() - 1);

                        chat_text.erase(remove(chat_text.begin(), chat_text.end(), '\''), chat_text.end());
                        chat_text.erase(remove(chat_text.begin(), chat_text.end(), '\"'), chat_text.end());
                        chat_text.erase(remove(chat_text.begin(), chat_text.end(), '\\'), chat_text.end());
                        chat_text.erase(remove(chat_text.begin(), chat_text.end(), '%'), chat_text.end());
                        chat_text.erase(remove(chat_text.begin(), chat_text.end(), '_'), chat_text.end());

                        db->insert(channel, user_name, chat_text);
                    }
                    else if (PRINT_OTHER_MSG && 
                    (line.find("CLEARCHAT") != std::string::npos || 
                    line.find("CLEARMSG") != std::string::npos || 
                    (line.find(" NOTICE") != std::string::npos && line.find("Login unsuccessful") == std::string::npos))){
                        std::cout << line << endl;
                    }
                }
            }

            start_read();
        }
        else
        {
            if (PRINT_ABOUT_SOCKET) {
                std::cout << "Error on receive: " << ec.message() << "\n";
            }
            stop();
        }
    }
    mtx.unlock();
}

std::vector<std::string> Client::split(const std::string &msg)
{
    std::vector<std::string> result;

    for (int i = 0, idx, pre_idx = -1; i < 3; i++)
    {

        idx = msg.find(' ', pre_idx + 1);
        if (idx != -1)
        {
            result.push_back(msg.substr(pre_idx + 1, idx - pre_idx - 1));
            pre_idx = idx;
            if (i == 2 && idx + 1 < msg.length())
            {
                result.push_back(msg.substr(idx + 1, msg.length()));
            }
        }
        else
        {
            return result;
        }
    }
    return result;
}

void Client::handle_write(const boost::system::error_code &ec)
{
    mtx.lock();
    if (!is_stopped)
    {
        if (!ec)
        {
            if (PRINT_ABOUT_SOCKET) {
                std::cout << "#";
            }
        }
        else
        {
            std::cout << "Write fail"
                      << "\n";
            stop();
        }
    }
    mtx.unlock();
}

void Client::check_deadline()
{
    mtx.lock();
    if (!is_stopped)
    {
        if (deadline.expires_at() <= deadline_timer::traits_type::now())
        {
            stop();
        }
        else
        {
            deadline.async_wait(boost::bind(&Client::check_deadline, this));
        }
    }
    mtx.unlock();
}
