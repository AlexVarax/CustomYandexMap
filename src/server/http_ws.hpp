#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>

#include "aggregator/aggregator.hpp"
#include "grid/geo_validator.hpp"
#include "parser/json_parser.hpp"

namespace monitor {

class HttpWsServer final {
public:
    HttpWsServer(boost::asio::io_context& ioc,
                 unsigned short port,
                 Aggregator& aggregator,
                 GeoValidator& validator,
                 const std::string& frontend_root);

    void run();
    void stop();
    void broadcast_loop(std::atomic<bool>& running);

private:
    class Listener;
    class WsClient;

    boost::asio::io_context& ioc_;
    unsigned short port_;
    Aggregator& aggregator_;
    GeoValidator& validator_;
    JsonParser parser_;
    std::string frontend_root_;
    std::shared_ptr<Listener> listener_;
};

}  // namespace monitor
