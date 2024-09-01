#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>
#include <set>
#include <functional>
#include <mutex>
#pragma once

typedef websocketpp::server<websocketpp::config::asio> server;
using websocketpp::connection_hdl;

class WebSocketServer {
private:
    server ws_server_;
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections_;
    std::mutex connection_mutex_;

public:
    WebSocketServer() {
        ws_server_.clear_access_channels(websocketpp::log::alevel::all);
        ws_server_.clear_error_channels(websocketpp::log::elevel::all);
        ws_server_.init_asio();

        ws_server_.set_open_handler(std::bind(&WebSocketServer::on_open, this, std::placeholders::_1));
        ws_server_.set_close_handler(std::bind(&WebSocketServer::on_close, this, std::placeholders::_1));
        ws_server_.set_message_handler(std::bind(&WebSocketServer::on_message, this, std::placeholders::_1, std::placeholders::_2));
    }

    void run(uint16_t port) {
        ws_server_.listen(port);
        ws_server_.start_accept();
        std::cout << "websocket listening on port " << port << std::endl;
        ws_server_.run();
    }

    void broadcast(const std::string& message) {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        for (const auto& it : connections_) {
            ws_server_.send(it, message, websocketpp::frame::opcode::text);
        }
    }

    void stop() {
        ws_server_.stop_listening();
        std::lock_guard<std::mutex> lock(connection_mutex_);
        for (auto& con : connections_) {
            ws_server_.close(con, websocketpp::close::status::going_away, "server shutdown");
        }
        ws_server_.stop();
    }

private:
    void on_open(const connection_hdl& hdl) {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        connections_.insert(hdl);
        std::cout << "new websocket connection established" << std::endl;
    }

    void on_close(const connection_hdl& hdl) {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        connections_.erase(hdl);
        std::cout << "websocket closed" << std::endl;
    }

    void on_message(const connection_hdl& hdl, const server::message_ptr& msg) {
      //  std::cout << "received message: " << msg->get_payload() << std::endl;
    }
};
