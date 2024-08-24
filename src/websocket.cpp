//
// Created by Devang Jaiswal on 8/24/24.
//

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>
#include <set>
#include <functional>

typedef websocketpp::server<websocketpp::config::asio> server;
using websocketpp::connection_hdl;

class WebSocketServer {
private:
    server ws_server;
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections;

public:
    WebSocketServer() {
        ws_server.init_asio();

        ws_server.set_open_handler(std::bind(&WebSocketServer::on_open, this, std::placeholders::_1));
        ws_server.set_close_handler(std::bind(&WebSocketServer::on_close, this, std::placeholders::_1));
        ws_server.set_message_handler(std::bind(&WebSocketServer::on_message, this, std::placeholders::_1, std::placeholders::_2));
    }

    void run(uint16_t port) {
        ws_server.listen(port);
        ws_server.start_accept();
        std::cout << "websocket server listening on port " << port << std::endl;
        ws_server.run();
    }

    void broadcast(const std::string& message) {
        for (auto it : connections) {
            ws_server.send(it, message, websocketpp::frame::opcode::text);
        }
    }

    void stop() {
        ws_server.stop_listening();
        for (auto& con : connections) {
            ws_server.close(con, websocketpp::close::status::going_away, "server shutdown");
        }
        ws_server.stop();
    }

private:
    void on_open(connection_hdl hdl) {
        connections.insert(hdl);
        std::cout << "new websocket connection" << std::endl;
    }

    void on_close(connection_hdl hdl) {
        connections.erase(hdl);
        std::cout << "websocket connection closed" << std::endl;
    }

    void on_message(connection_hdl hdl, server::message_ptr msg) {
        std::cout << "received message: " << msg->get_payload() << std::endl;
        // Handle incoming messages if needed
    }
};

