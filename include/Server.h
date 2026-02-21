#ifndef SERVER_H
#define SERVER_H

#include "Reactor.h"


class Server
{   
    private:
        static constexpr int MAX_THREADS = 8;
        static constexpr int reactor_cores[] = { 1, 3, 5, 7, 9, 11, 13, 15 };
        std::vector<std::thread> worker_threads;
        static constexpr int PORT = 3490;

        public:
            void run();
};


#endif
