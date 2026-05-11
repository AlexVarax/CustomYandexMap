#include "server/http_ws.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>

namespace monitor {

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

class HttpWsServer::WsClient : public std::enable_shared_from_this<HttpWsServer::WsClient> {
public:
    explicit WsClient(websocket::stream<tcp::socket> ws) : ws_(std::move(ws)) {}

    void start(http::request<http::string_body> req) {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.accept(req);
        read_loop();
    }

    bool send_text(const std::string& msg) {
        beast::error_code ec;
        ws_.text(true);
        ws_.write(boost::asio::buffer(msg), ec);
        return !ec;
    }

private:
    void read_loop() {
        beast::flat_buffer buffer;
        while (ws_.is_open()) {
            beast::error_code ec;
            ws_.read(buffer, ec);
            if (ec) {
                break;
            }
            buffer.clear();
        }
    }

    websocket::stream<tcp::socket> ws_;
};

class HttpWsServer::Listener : public std::enable_shared_from_this<HttpWsServer::Listener> {
public:
    Listener(boost::asio::io_context& ioc,
             tcp::endpoint endpoint,
             Aggregator& aggregator,
             GeoValidator& validator,
             JsonParser& parser,
             const std::string& frontend_root)
        : acceptor_(ioc),
          socket_(ioc),
          aggregator_(aggregator),
          validator_(validator),
          parser_(parser),
          frontend_root_(frontend_root) {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    }

    void run() {
        do_accept();
    }

    void stop() {
        beast::error_code ec;
        acceptor_.close(ec);
    }

    void broadcast(const std::string& msg) {
        std::vector<std::shared_ptr<WsClient>> alive;
        alive.reserve(ws_clients_.size());
        for (const auto& weak : ws_clients_) {
            if (auto client = weak.lock()) {
                if (client->send_text(msg)) {
                    alive.push_back(client);
                }
            }
        }
        ws_clients_.clear();
        for (const auto& c : alive) {
            ws_clients_.push_back(c);
        }
    }

private:
    void do_accept() {
        acceptor_.async_accept(socket_, [self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                self->handle_connection(std::move(self->socket_));
            }
            if (self->acceptor_.is_open()) {
                self->do_accept();
            }
        });
    }

    static std::string read_file(const std::string& path) {
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (!in.good()) {
            return {};
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::string metrics_json() const {
        std::ostringstream ss;
        ss << "{\"queue_depth\":" << aggregator_.queue_depth()
           << ",\"dropped\":" << aggregator_.dropped()
           << ",\"rejected_out_of_radius\":" << validator_.rejected_out_of_radius() << "}";
        return ss.str();
    }

    void handle_connection(tcp::socket socket) {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        beast::error_code ec;
        http::read(socket, buffer, req, ec);
        if (ec) {
            return;
        }

        if (websocket::is_upgrade(req) && req.target() == "/ws") {
            auto client = std::make_shared<WsClient>(websocket::stream<tcp::socket>(std::move(socket)));
            ws_clients_.push_back(client);
            client->start(std::move(req));
            return;
        }

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "internet-monitor");
        res.set(http::field::content_type, "application/json; charset=utf-8");
        res.keep_alive(req.keep_alive());

        if (req.method() == http::verb::post && req.target() == "/ingest") {
            InputRecord parsed{};
            std::string error;
            if (!parser_.parse(req.body(), parsed, error)) {
                std::cout << "[PARSER] Rejected payload: " << error << std::endl;
                res.body() = "{\"status\":\"ignored\"}";
            } else {
                std::string log_line;
                if (!validator_.validate_or_reject(parsed.lat, parsed.lon, log_line)) {
                    std::cout << log_line << std::endl;
                    res.body() = "{\"status\":\"ignored_out_of_radius\"}";
                } else {
                    const Sample sample{parsed.lat, parsed.lon, parsed.ts, parsed.flags};
                    (void)aggregator_.enqueue(sample);
                    res.body() = "{\"status\":\"accepted\"}";
                }
            }
        } else if (req.method() == http::verb::get && req.target() == "/metrics") {
            res.body() = metrics_json();
        } else if (req.method() == http::verb::get && req.target() == "/") {
            res.set(http::field::content_type, "text/html; charset=utf-8");
            const std::string html = read_file(frontend_root_ + "/index.html");
            res.body() = html.empty() ? "<h1>frontend/index.html not found</h1>" : html;
        } else {
            res.result(http::status::not_found);
            res.body() = "{\"error\":\"not_found\"}";
        }

        res.prepare_payload();
        http::write(socket, res, ec);
        socket.shutdown(tcp::socket::shutdown_send, ec);
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    Aggregator& aggregator_;
    GeoValidator& validator_;
    JsonParser& parser_;
    std::string frontend_root_;
    std::vector<std::weak_ptr<WsClient>> ws_clients_;
};

HttpWsServer::HttpWsServer(boost::asio::io_context& ioc,
                           unsigned short port,
                           Aggregator& aggregator,
                           GeoValidator& validator,
                           const std::string& frontend_root)
    : ioc_(ioc),
      port_(port),
      aggregator_(aggregator),
      validator_(validator),
      frontend_root_(frontend_root) {}

void HttpWsServer::run() {
    listener_ = std::make_shared<Listener>(
        ioc_,
        tcp::endpoint(tcp::v4(), port_),
        aggregator_,
        validator_,
        parser_,
        frontend_root_);
    listener_->run();
}

void HttpWsServer::stop() {
    if (listener_) {
        listener_->stop();
    }
}

void HttpWsServer::broadcast_loop(std::atomic<bool>& running) {
    using namespace std::chrono_literals;
    while (running.load(std::memory_order_acquire)) {
        auto deltas = aggregator_.consume_deltas();
        if (!deltas.empty() && listener_) {
            std::ostringstream ss;
            ss << "{\"type\":\"delta\",\"cells\":[";
            for (std::size_t i = 0; i < deltas.size(); ++i) {
                const auto& d = deltas[i];
                ss << "{\"cell_id\":\"" << d.cell_id << "\",\"total\":" << d.total << ",\"availability\":[";
                for (std::size_t j = 0; j < d.availability.size(); ++j) {
                    ss << d.availability[j];
                    if (j + 1 < d.availability.size()) {
                        ss << ",";
                    }
                }
                ss << "]}";
                if (i + 1 < deltas.size()) {
                    ss << ",";
                }
            }
            ss << "]}";
            listener_->broadcast(ss.str());
        }
        std::this_thread::sleep_for(500ms);
    }
}

}  // namespace monitor
