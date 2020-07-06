#pragma once

#include "cqhttp/core/plugin.h"

namespace cqhttp::plugins {
    template <typename WsT>
    static void ws_api_send_result(const std::shared_ptr<typename WsT::Connection> connection,
                                   const ActionResult &result, const json &echo) {
        static const auto TAG = u8"WS API";
        json resp_json = result;
        if (!echo.is_null()) {
            resp_json["echo"] = echo;
        }
        const auto resp_body = resp_json.dump();
        logging::debug(TAG, u8"响应数据已准备完毕：" + resp_body);
        const auto out_message = std::make_shared<typename WsT::OutMessage>();
        *out_message << resp_body;
        connection->send(out_message);
        logging::debug(TAG, u8"响应内容已发送");
    }

    /**
     * Common "on_message" callback for websocket server's api endpoint and reverse websocket api client.
     * \tparam WsT WsServer (websocket server /api/ endpoint) or WsClient (reverse websocket api client)
     */
    template <typename WsT>
    static void ws_api_on_message(
        const std::shared_ptr<typename WsT::Connection> connection,
        const std::shared_ptr<typename WsT::InMessage> message,
        std::function<void(const std::shared_ptr<typename WsT::Connection>, const ActionResult &, const json &)>
            send_result = ws_api_send_result<WsT>) {
        static const auto TAG = u8"WS API";

        const auto ws_message_str = message->string();
        logging::debug(TAG, u8"收到 API 请求：" + ws_message_str);

        json payload;
        try {
            payload = json::parse(ws_message_str);
        } catch (json::parse_error &) {
            // bad JSON
        }
        if (!(payload.is_object() && payload.find("action") != payload.end() && payload["action"].is_string())) {
            logging::debug(TAG, u8"请求中的 JSON 无效或者不是对象");
            send_result(connection, ActionResult(ActionResult::Codes::HTTP_BAD_REQUEST), nullptr);
            return;
        }

        const auto action = payload["action"].get<std::string>();

        auto params = json::object();
        if (payload.find("params") != payload.end() && payload["params"].is_object()) {
            params = payload["params"];
        }

        logging::debug(TAG, u8"开始执行动作 " + action);
        const auto result = call_action(action, params);
        if (result.code != ActionResult::Codes::HTTP_NOT_FOUND) {
            logging::debug(TAG, u8"动作 " + action + u8" 执行成功");
        } else {
            logging::debug(TAG, u8"没有找到相应的处理函数，动作 " + action + u8" 执行失败");
        }

        json echo;
        try {
            echo = payload.at("echo");
        } catch (...) {
        }

        send_result(connection, result, echo);
        logging::info_success(TAG, u8"已成功处理一个 API 请求：" + action);
    }
} // namespace cqhttp::plugins
