#ifndef SAFE_QUEUE
#define SAFE_QUEUE

#include <vector>
#include <mutex>
#include <condition_variable>

// A threadsafe-queue.
// Technically this isn't a queue since its backed by a vector but its good enough.
template <class T>
class SafeQueue
{
public:
  SafeQueue(void)
    : q()
    , m()
    , c()
  {}

  ~SafeQueue(void)
  {}

  // Add an element to the queue.
  void push(T t)
  {
    std::lock_guard<std::mutex> lock(m);
    q.push_back(t);
    c.notify_one();
  }

  // Get the "front"-element.
  // If the queue is empty, wait till a element is avaiable.
  T pop(void)
  {
    std::unique_lock<std::mutex> lock(m);
    while(q.empty())
    {
      // release lock as long as the wait and reaquire it afterwards.
      c.wait(lock);
    }
    T val = q.front();
    q.pop();
    return val;
  }

  T operator[](size_t i)
  {
    std::unique_lock<std::mutex> lock(m);
    return q[i];
  }

  bool empty(void)
  {
    return q.empty();
  }

  size_t size(void) {
    return q.size();
  }

  void clear(void) {
    q.clear();
  }

private:
  std::vector<T> q;
  mutable std::mutex m;
  std::condition_variable c;
};
#endif

