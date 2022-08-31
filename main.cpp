#include "collector.hpp"

int main()
{
    boost::asio::io_context io_context;
    Collector collector;

    if (collector.read_params())
    {
        collector.start(io_context);
    }

    return 0;
}
