#ifndef JOINED_THREAD_H
#define JOINED_THREAD_H

#include <thread>
#include <iostream>
#include <type_traits>
#include <vector>
#include <mutex>
#include <functional>

namespace velThread
{
class joined_thread {

    private:
        std::thread t;
    public:
        template<typename Callable, typename ... Args>
        explicit joined_thread(Callable func, Args&&... arguments) : 
        t(std::forward<Callable>(func), std::forward<Args>(arguments)...) {}

        explicit joined_thread(std::thread&& t_) noexcept : t(std::move(t_)) {}

        joined_thread(joined_thread&& j) noexcept : t(std::move(j.t)) {}

        joined_thread& operator=(joined_thread&& j) noexcept {
            if (this->t.joinable()) {
                t.join();
            }
            this-> t = std::move(j.t);
            return *this;
        }

        virtual ~joined_thread() {
            if (t.joinable()) {
                t.join();
            }
        }

        joined_thread(const joined_thread& other) = delete;
        joined_thread& operator=(const joined_thread& other) = delete;
};
}

#endif
