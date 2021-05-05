// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2021 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Contains source code from
// https://github.com/mpromonet/webrtc-streamer
//
// This software is in the public domain, furnished "as is", without technical
// support, and with no warranty, express or implied, as to its usefulness for
// any purpose.
// ----------------------------------------------------------------------------

#include "open3d/visualization/webrtc_server/HttpServerRequestHandler.h"

#include <cstring>
#include <iostream>

#include "open3d/utility/Console.h"

namespace open3d {
namespace visualization {
namespace webrtc_server {

int LogMessage(const struct mg_connection *conn, const char *message) {
    fprintf(stderr, "%s\n", message);
    return 0;
}

static struct CivetCallbacks _callbacks;
const struct CivetCallbacks *getCivetCallbacks() {
    memset(static_cast<void *>(&_callbacks), 0, sizeof(_callbacks));
    _callbacks.log_message = &LogMessage;
    return &_callbacks;
}

class RequestHandler : public CivetHandler {
public:
    RequestHandler(HttpServerRequestHandler::HttpFunction &func)
        : func_(func) {}

    bool handle(CivetServer *server, struct mg_connection *conn) {
        bool ret = false;
        const struct mg_request_info *req_info = mg_get_request_info(conn);

        utility::LogInfo("RequestHandler ##########################");
        utility::LogInfo("request_uri: {}", req_info->request_uri);
        utility::LogInfo("local_uri: {}", req_info->local_uri);
        utility::LogInfo("request_method: {}", req_info->request_method);
        if (req_info->query_string) {
            utility::LogInfo("query_string: {}", req_info->query_string);
        } else {
            utility::LogInfo("query_string: {}", "nullptr");
        }
        // utility::LogInfo("remote_addr: {}", req_info->remote_addr);
        utility::LogInfo("content_length: {}", req_info->content_length);
        utility::LogInfo("#########################################");

        // Read input.
        Json::Value in = this->getInputMessage(req_info, conn);

        // Invoke API implementation.
        Json::Value out(func_(req_info, in));

        // Fill out.
        if (out.isNull() == false) {
            std::string answer(Json::writeString(writer_builder_, out));
            utility::LogInfo("Answer: {}", answer);

            mg_printf(conn, "HTTP/1.1 200 OK\r\n");
            mg_printf(conn, "Access-Control-Allow-Origin: *\r\n");
            mg_printf(conn, "Content-Type: text/plain\r\n");
            mg_printf(conn, "Content-Length: %zd\r\n", answer.size());
            mg_printf(conn, "\r\n");
            mg_write(conn, answer.c_str(), answer.size());

            ret = true;
        }

        return ret;
    }
    bool handleGet(CivetServer *server, struct mg_connection *conn) override {
        return handle(server, conn);
    }
    bool handlePost(CivetServer *server, struct mg_connection *conn) override {
        return handle(server, conn);
    }

private:
    HttpServerRequestHandler::HttpFunction func_;
    Json::StreamWriterBuilder writer_builder_;
    Json::CharReaderBuilder reader_builder_;

    Json::Value getInputMessage(const struct mg_request_info *req_info,
                                struct mg_connection *conn) {
        Json::Value json_message;

        // Read input.
        long long tlen = req_info->content_length;
        if (tlen > 0) {
            std::string body;
            long long nlen = 0;
            const long long bufSize = 1024;
            char buf[bufSize];
            while (nlen < tlen) {
                long long rlen = tlen - nlen;
                if (rlen > bufSize) {
                    rlen = bufSize;
                }
                rlen = mg_read(conn, buf, (size_t)rlen);
                if (rlen <= 0) {
                    break;
                }
                body.append(buf, rlen);

                nlen += rlen;
            }

            // Parse in.
            std::unique_ptr<Json::CharReader> reader(
                    reader_builder_.newCharReader());
            std::string errors;
            if (!reader->parse(body.c_str(), body.c_str() + body.size(),
                               &json_message, &errors)) {
                utility::LogWarning("Received unknown message: {}, errors: {}.",
                                    body, errors);
            }
        }
        return json_message;
    }
};

HttpServerRequestHandler::HttpServerRequestHandler(
        std::map<std::string, HttpFunction> &func,
        const std::vector<std::string> &options)
    : CivetServer(options, getCivetCallbacks()) {
    // Register handlers.
    for (auto it : func) {
        this->addHandler(it.first, new RequestHandler(it.second));
    }
}

}  // namespace webrtc_server
}  // namespace visualization
}  // namespace open3d
