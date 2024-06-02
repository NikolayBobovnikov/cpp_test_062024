#include <filesystem>
#include <functional>
#include <memory>
#include <ostream>
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
