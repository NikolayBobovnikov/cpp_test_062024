#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <ostream>
#include <queue>
#include <string>

template <typename... Args>
std::string string_format(const std::string& format, Args... args)
{
	std::ostringstream oss;
	std::size_t size = snprintf(nullptr, 0, format.c_str(), args...);
	std::unique_ptr<char[]> buffer(new char[size + 1]);
	snprintf(buffer.get(), size + 1, format.c_str(), args...);
	oss << buffer.get();
	return oss.str();
}


class ScopeGuard
{
public:
	ScopeGuard(std::function<void()> onExitScope)
		: onExitScope_(onExitScope)
		, dismissed_(false)
	{ }
	~ScopeGuard()
	{
		if(!dismissed_)
			onExitScope_();
	}
	void dismiss()
	{
		dismissed_ = true;
	}

private:
	std::function<void()> onExitScope_;
	bool dismissed_;
};


template <typename T>
class ThreadSafeQueue
{
public:
	void push(T value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		queue_.push(std::move(value));
		cond_var_.notify_one();
	}

	T pop()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		cond_var_.wait(lock, [this] { return !queue_.empty(); });
		T value = std::move(queue_.front());
		queue_.pop();
		return value;
	}

private:
	std::queue<T> queue_;
	std::mutex mutex_;
	std::condition_variable cond_var_;
};
