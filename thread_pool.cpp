#include "thread_pool.h"

namespace
{	
	/*
	 * Behavior:
	 * 	Handles the threads by putting them in an eternal while loop
	 * 	where they look for the top element (if exists) and will execute it
	 * 	if it is TaskType execute, or will shut down the thread if is
	 * 	TaskType Stop
	 * Exception:
	 * 	None?
	 * Parameters:
	 * 	safe_queue<Task>: A thread-safe queue of tasks for the threads to 
	 * 	commence
	 * Returns:
	 * 	Function (as this should be the entry point of a jthread)
	 */
	auto make_thread_handler(thread_pool::TsQueue<thread_pool::Task>& _queue) 
	{
		return std::jthread{
			[&_queue]{
				while (true)
				{
					auto const elem = _queue.pop();
					switch (elem.type) {
					case thread_pool::TaskType::Execute:
						elem.task(elem.arguments);
						break;
					case thread_pool::TaskType::Stop:
						return;
					}
				}
			}
		};
	}
}

/*
 * Behavior:
 * 	Creates a given number of threads and puts them in the pool
 * Exceptions:
 * 	None?
 * Return:
 * 	None
 * Parameters:
 * 	std::size_t number of threads to put in pool
 */
thread_pool::ThreadPool::ThreadPool(std::size_t n_threads)
{
	for (std::size_t i{0}; i < n_threads; ++i)
	{
		_threads.push_back(make_thread_handler(_queue));
	}
}

/*
 * Behavior:
 * 	Destroys a ThreadPool by injecting a stop objective for each running thread
 * Exception:
 * 	bad_alloc
 * Return:
 * 	None
 * Parameters:
 * 	None
 */

thread_pool::ThreadPool::~ThreadPool()
{
	const Task stop_task{TaskType::Stop, {}, {}};
	for (std::size_t i{0}; i < _threads.size(); ++i)
	{
		push(stop_task);
	}
}

/*
 * Behavior:
 * 	Pushes a task object into the task queue for threads to commence
 * Exceptions:
 * 	bad_alloc
 * Return:
 * 	True if succsessful
 * Parameters:
 * 	const Task&: A task to push onto the task priority queue
*/
bool thread_pool::ThreadPool::push(const Task& task)
{
	_queue.push(task);
	return true;
}
