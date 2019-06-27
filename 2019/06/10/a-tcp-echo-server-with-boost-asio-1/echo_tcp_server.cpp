/*
 * Copyright (c) 2018 Ally of Intelligence Technology Co., Ltd. All rights reserved.
 *
 * Created by WuKun on 5/24/19.
 * Contact with:wk707060335@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http: *www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>

class AsioThreadPool
{
public:
    // the constructor just launches some amount of threads
    AsioThreadPool(int threadNum = std::thread::hardware_concurrency())
            : work_guard_(boost::asio::make_work_guard(service_))
    {
        // one io_context and multi thread
        for (int i = 0; i < threadNum; ++i)
        {
            threads_.emplace_back([this] () { service_.run(); });
        }
    }

    AsioThreadPool(const AsioThreadPool &) = delete;
    AsioThreadPool &operator=(const AsioThreadPool &) = delete;

    boost::asio::io_context &getIOService()
    {
        return service_;
    }

    void stop()
    {
        // Once the work object is destroyed, the service will stop.
        work_guard_.reset();
        for (auto &t: threads_) {
            t.join();
        }
    }
private:
    boost::asio::io_context service_;

    typedef boost::asio::io_context::executor_type ExecutorType;
    boost::asio::executor_work_guard<ExecutorType> work_guard_;
    std::vector<std::thread> threads_;
};


using boost::asio::ip::tcp;
class TCPConnection : public std::enable_shared_from_this<TCPConnection>
{
public:
    typedef boost::shared_ptr<TCPConnection> pointer;

    TCPConnection(boost::asio::io_context &io_context)
            : socket_(io_context),
              strand_(io_context) {}

    tcp::socket &socket() {  return socket_;  }
    void start() {  doRead();  }

private:
    void doRead()
    {
        auto self = shared_from_this();
        socket_.async_read_some(
                boost::asio::buffer(buffer_, buffer_.size()),
                boost::asio::bind_executor(strand_, [this, self](boost::system::error_code ec,
                                                                 std::size_t bytes_transferred)
                {
                    // error_code process here
                    if (!ec) {
                        doWrite(bytes_transferred);
                    }
                }));

        // Notice:
        // You’ll capture the shared pointer to the session in the lambdas
        // that process connection events. As long as there’s an event the session
        // is waiting for, the session object won’t be deleted, because the
        // lambda that handles that event will hold an instance of the shared pointer.
        // when there are no more events you want to process, the object will be deleted.
    }

    void doWrite(std::size_t length)
    {
        auto self = shared_from_this();
        boost::asio::async_write(
                socket_, boost::asio::buffer(buffer_, length),
                boost::asio::bind_executor(strand_, [this, self](boost::system::error_code ec,
                                          std::size_t /* bytes_transferred */)
                             {
                                 if (!ec) {  doRead();  }
                             }));
    }

private:
    tcp::socket socket_;
    boost::asio::io_context::strand strand_;
    std::array<char, 8192> buffer_;
};

class EchoServer
{
public:
    EchoServer(boost::asio::io_context &io_context, unsigned short port)
            : io_context(io_context),
              acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        // The constructor initialises an acceptor to listen on TCP port.
        doAccept();
    }

    // The function doAccept() creates a socket and initiates
    // an asynchronous accept operation to wait for a new connection.
    void doAccept()
    {
        auto conn = std::make_shared<TCPConnection>(io_context);
        acceptor_.async_accept(conn->socket(),
                               [this, conn](boost::system::error_code ec)
                               {
                                   // services the client request
                                   if (!ec) {  conn->start();  }

                                   // then calls start_accept() to initiate
                                   // the next accept operation.
                                   this->doAccept();
                               });
    }

private:
    boost::asio::io_context &io_context;
    tcp::acceptor acceptor_;
};

int main(int argc, char *argv[])
{
    AsioThreadPool pool(4);

    unsigned short port = 5800;
    EchoServer server(pool.getIOService(), port);

    return 0;
}