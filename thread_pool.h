#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <mutex>
#include <thread>
#include <functional>
#include <vector>
#include <condition_variable>
#include <queue>
#include "safe_queue.h"
#include "joined_thread.h"

namespace thread_pool
{
	enum class TaskType {
		Execute,
		Stop
	};

	struct Task {
		TaskType type;
		std::function<void(std::vector<int>)> task;
		std::vector<int> arguments;
	};
	class ThreadPool
	{
		private:
			TsQueue<Task> _queue;
			std::vector<velThread::joined_thread> _threads;
		public:
		explicit ThreadPool(std::size_t n_threads);
		virtual ~ThreadPool();
		bool push(const Task& task);
	};
}

#endif
