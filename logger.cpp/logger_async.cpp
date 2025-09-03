#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <ctime>
#include <vector>

// Logger class - writes log messages to a file asynchronously 
class Logger {
public:

	enum Level {INFO, WARNING, ERROR};

	// CTOR - opens the file and starts the worker thread
	Logger(const std::string& filename) : file(filename, std::ios::app), exit_flag(false) // open file in append mode and set exit flag
	{
		if (!file.is_open())
		{
			std::cerr << "Failed to open log file: " << filename << std::endl;
		}
		
		// start a seperate thread that will write log messages from the queue
		worker = std::thread(&Logger::workerLoop, this);

	}

	// Delete copy constructor and assignment (simplifies usage)
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	~Logger() {
		{
			std::unique_lock<std::mutex> lk(mtx);
			exit_flag = true;
		}

		cv.notify_one(); //wake up worker thread it's time to shutdown
		if (worker.joinable()) worker.join(); // wait for the worker thread to finish
		if (file.is_open()) file.close();


	}

	void log(Level level, const std::string& message) {
		std::string line = formatLine(level, message);
		{
			std::unique_lock<std::mutex> lk(mtx);
			q.push(std::move(line)); // add the message to the queue, use move for more efficent 

		}
		cv.notify_one(); //notify worker that there is a message in the queue
	}

	// we need this when i need to know when the is empty (all logs are written)
	void waitEmpty() {
		std::unique_lock<std::mutex> lk(mtx);
		cv_empty.wait(lk, [this] {return q.empty(); }); //wait until the queue is empty

	}





private:
	std::ofstream file;
	std::thread worker; // the thread that will be responsible for writing to file - background thread
	std::mutex mtx; // for locking
	
	std::condition_variable cv;
	std::condition_variable cv_empty; // 

	std::queue<std::string> q; // queue of messages
	bool exit_flag; // Job is finished flag

	//return current date and time
	static std::string currentDateTime() {
		std::time_t now = std::time(nullptr); //
		char buf[64];
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
		return std::string(buf);
	}

	// convert level to string
	static const char* levelToStr(Level l) {
		switch (l) {
		case INFO: return "INFO";
		case WARNING: return "WARN";
		case ERROR: return "ERROR";
		default: return "UNK";
		}
	}

	// Concatinates date + level + message to a single message (string)
	std::string formatLine(Level level, const std::string& msg) {
		return currentDateTime() + " [" + levelToStr(level) + "] " + msg;
	}

	// Worker thread function - here we will handle messages in queue and writing to file
	void workerLoop() {
		while(true) {
			std::string item;
			{
				std::unique_lock<std::mutex> lk(mtx);

				//the thread does not waste CPU - it sleeps untill:
				//new log msg is pushed into the queue
				//the logger is shutting down (someone did exit_flag = true)
				cv.wait(lk, [this] {return exit_flag || !q.empty(); });


				// when woken up:
				// we will exit this in case the exit_flag is true and the queue is empty -> shutdown
				if (exit_flag && q.empty())
				{
					return; // thred finish
				}

				// Else handle the next msg in the queue
				item = std::move(q.front());
				q.pop();

			}

			// critical section ended we needed to protect the access to queue - it was a shared resource
			// so we are outdside the lock

			if (file.is_open())
			{
				file << item << std::endl;
				file.flush(); // force write to disk
			}
			


			// if the queue is empty now -> notify anyone waiting with waitEmpty()
			{

				std::unique_lock<std::mutex> lk(mtx);
				if (q.empty())
				{
					cv_empty.notify_all();

				}

			}
		}
	}



};


int main() {
	Logger logger("app.log");

	// function to simulate log writing from multiple threads
	auto worker = [&](int id) {
		for (int i = 0; i < 200; ++i)
		{
			logger.log(Logger::INFO, "thread " + std::to_string(id) + "message" + std::to_string(i));

		}
	};

	std::vector<std::thread> threads;
	threads.emplace_back(worker, 1); // first worker
	threads.emplace_back(worker, 2); //second worker
	threads.emplace_back([&]() { // thied thread will log warnings errors
		for (int i = 0; i < 50; ++i)
		{
			logger.log(Logger::WARNING, "background warning " + std::to_string(i));
		}
	});

	// wait for all threads to finish
	for (auto &t : threads)
	{
		if (t.joinable())
		{
			t.join();
		}

	}

	// main thread is logging final Error
	logger.log(Logger::ERROR, "All worker threads finished");

	// wait untill all queued messages are written before program exits
	logger.waitEmpty();

	return 0;
	

}