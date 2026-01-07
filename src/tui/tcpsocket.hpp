// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <vector>
#include <functional>

using std::string;
using std::vector;
using std::function;

static_assert(sizeof(uint32_t) == 4, "uint32_t size is not 4 bytes");

constexpr uint32_t MagicNumber = 0x37549260U;

class TCPSocket {
public:
    TCPSocket(int32_t port, bool listen_global, function<void(const string &)> println_log) : port(port), listen_global(listen_global), println_log(println_log) {}

    string receive_blocked() {
        if (socketfd < 0) {
            int listenfd = socket(AF_INET, SOCK_STREAM, 0);
            if (listenfd < 0) {
                throw std::runtime_error("Socket creation failed on port " + std::to_string(port));
            }
            int opt = 1;
            setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = listen_global ? INADDR_ANY : htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(port);
            if (bind(listenfd, (sockaddr *)&addr, sizeof(addr)) < 0) {
                close(listenfd);
                throw std::runtime_error("Socket bind failed on port " + std::to_string(port));
            }
            if (listen(listenfd, 1) < 0) {
                close(listenfd);
                throw std::runtime_error("Socket listen failed on port " + std::to_string(port));
            }
            {
                // listen with 1 second timeout
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(listenfd, &fds);
                timeval tv;
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                int ret = select(listenfd + 1, &fds, nullptr, nullptr, &tv);
                if (ret < 0) {
                    close(listenfd);
                    throw std::runtime_error("Socket select failed on port " + std::to_string(port));
                } else if (ret == 0 || !FD_ISSET(listenfd, &fds)) {
                    close(listenfd);
                    return "";
                }
            }
            socklen_t client_len = sizeof(client_addr);
            socketfd = accept(listenfd, (sockaddr *)&client_addr, &client_len);
            close(listenfd);
            if (socketfd < 0) {
                throw std::runtime_error("Socket accept failed on port " + std::to_string(port));
            }
            string client_ip = inet_ntoa(client_addr.sin_addr);
            println_log("Socket connected from " + client_ip + " on port " + std::to_string(port));
        }
        
        auto read_exact = [&](const uint32_t size, void * buf) -> bool {
            uint32_t bytes_read = 0;
            int64_t ret = 0;
            while (bytes_read < size) {
                int64_t ret = recv(socketfd, (char *)buf + bytes_read, size - bytes_read, 0);
                if (ret <= 0) {
                    break;
                }
                bytes_read += ret;
            }
            if (ret < 0) {
                println_log("Socket receive failed on port " + std::to_string(port) + ", closing connection.");
                close(socketfd);
                socketfd = -1;
                return false;
            } else if (ret == 0) {
                // double check for EOF
                char buf[1];
                if (recv(socketfd, buf, 1, MSG_PEEK) == 0) {
                    println_log("Socket closed by peer on port " + std::to_string(port));
                    close(socketfd);
                    socketfd = -1;
                }
                return false;
            }
            return true;
        };

        uint32_t magic = 0;
        if (!read_exact(sizeof(magic), &magic)) {
            return "";
        }
        if (magic != MagicNumber) {
            // invalid magic number, close socket
            println_log("Socket received invalid magic number on port " + std::to_string(port) + ", closing connection.");
            close(socketfd);
            socketfd = -1;
            return "";
        }

        uint32_t msg_size = 0;
        if (!read_exact(sizeof(msg_size), &msg_size)) {
            return "";
        }
        string result(msg_size, 0);
        if (!read_exact(msg_size, &result[0])) {
            return "";
        }
        return result;
    }

    void send_blocked(const string &data) {
        if (socketfd < 0) {
            return;
        }
        uint32_t magic = MagicNumber;
        uint32_t msg_size = data.size();
        vector<uint8_t> send_buf(sizeof(magic) + sizeof(msg_size) + msg_size);
        memcpy(send_buf.data(), &magic, sizeof(magic));
        memcpy(send_buf.data() + sizeof(magic), &msg_size, sizeof(msg_size));
        memcpy(send_buf.data() + sizeof(magic) + sizeof(msg_size), data.data(), msg_size);
        uint32_t bytes_sent = 0;
        while (bytes_sent < send_buf.size()) {
            int64_t ret = send(socketfd, send_buf.data() + bytes_sent, send_buf.size() - bytes_sent, 0);
            if (ret <= 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                close(socketfd);
                socketfd = -1;
                throw std::runtime_error("Socket send failed on port " + std::to_string(port));
            }
            bytes_sent += ret;
        }
    }

private:
    int32_t port = 0;
    bool listen_global = false;
    function<void(const string &)> println_log;

    int socketfd = -1;
    sockaddr_in client_addr;
};

