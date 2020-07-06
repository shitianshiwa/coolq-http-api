#pragma once

#include "cqhttp/core/plugin.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "cqhttp/plugins/web/vendor/simple_web/client_ws.hpp"
#include "cqhttp/plugins/web/vendor/simple_web/client_wss.hpp"

namespace cqhttp::plugins {
    struct WebSocketReverse : Plugin {
        WebSocketReverse() = default;
        std::string name() const override { return "websocket_reverse"; }

        void hook_enable(Context &ctx) override;
        void hook_disable(Context &ctx) override;
        void hook_after_event(EventContext<cq::Event> &ctx) override;

        bool good() const override {
            return (!api_ || api_->connected()) && (!event_ || event_->connected())
                   && (!universal_ || universal_->connected());
        }

    private:
        bool use_ws_reverse_;

        class ClientBase {
        public:
            explicit ClientBase(const std::string &url, const std::string &access_token,
                                const unsigned long reconnect_interval, const bool reconnect_on_code_1000)
                : url_(url),
                  access_token_(access_token),
                  reconnect_interval_(reconnect_interval),
                  reconnect_on_code_1000_(reconnect_on_code_1000) {}

            virtual ~ClientBase() = default;

            virtual std::string name() = 0;

            virtual void start();
            virtual void stop();

            virtual bool started() const { return started_; }
            virtual bool connected() const { return connected_; }

        protected:
            virtual void init();
            virtual void connect();
            virtual void disconnect();

            template <typename WsClientT>
            void init_ws_reverse_client(std::shared_ptr<WsClientT> client);

            std::string url_;
            std::string access_token_;
            std::chrono::milliseconds reconnect_interval_;
            bool reconnect_on_code_1000_;

            std::atomic_bool started_ = false;
            std::atomic_bool connected_ = false;

            union Client {
                std::shared_ptr<SimpleWeb::SocketClient<SimpleWeb::WS>> ws;
                std::shared_ptr<SimpleWeb::SocketClient<SimpleWeb::WSS>> wss;

                Client() : ws(nullptr) {}
                ~Client() {}
            };

            Client client_;
            std::optional<bool> client_is_wss_;
            std::thread thread_;

            bool should_reconnect_ = false;
            std::thread reconnect_worker_thread_;
            bool reconnect_worker_running_ = false;
            std::mutex mutex_;
            std::condition_variable cv_;

            void notify_should_reconnect() {
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    should_reconnect_ = true;
                }
                cv_.notify_all();
            }

            void notify_reconnect_worker_stop_running() {
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    reconnect_worker_running_ = false;
                }
                cv_.notify_all(); // this will notify the reconnect worker to stop
            }
        };

        class ApiClient final : public ClientBase {
        public:
            using ClientBase::ClientBase;
            std::string name() override { return "API"; }

        protected:
            void init() override;
        };

        std::shared_ptr<ApiClient> api_;

        class EventClient : public ClientBase {
        public:
            using ClientBase::ClientBase;
            std::string name() override { return "Event"; }

            void push_event(const json &payload);

        protected:
            void init() override;
        };

        std::shared_ptr<EventClient> event_;

        class UniversalClient final : public EventClient {
        public:
            using EventClient::EventClient;
            std::string name() override { return "Universal"; }

        protected:
            void init() override;
        };

        std::shared_ptr<UniversalClient> universal_;
    };

    static std::shared_ptr<WebSocketReverse> websocket_reverse = std::make_shared<WebSocketReverse>();
} // namespace cqhttp::plugins
