/*
 * server.cpp -- a stream socket server demo
*/
#include "Server.h"

void Server::run()
{
    for (int i{0}; i < MAX_THREADS; ++i) {
        worker_threads.emplace_back([this, i](){
            Reactor reactor(i, this->PORT);
            reactor.pin_to_core(this->reactor_cores[i]);
            reactor.run();
        });
    }

    for (auto& t : worker_threads) {
        if (t.joinable()) t.join();
    }
}
    







