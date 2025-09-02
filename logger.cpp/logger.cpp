#include <iostream>
#include <fstream>
#include <string>
#include <ctime>


class Logger {
public:
	enum Level {INFO, WARNING, ERROR};

	Logger(const std::string& filename) {
		file.open(filename, std::ios::app); // Open file for reading
		if (!file.is_open())
		{
			std::cerr << "Error opening file!" << std::endl;
		}

	}

	void log(Level level, const std::string& message) {
		if (file.is_open())
		{
			file <<currnetDateTime()<<"[" <<levelToString(level) <<"]"<< message << std::endl;
		}
	}

private:
	std::ofstream file;


	// This func return local time as a string
	std::string currnetDateTime() {
		std::time_t now = std::time(nullptr);
		char buff[80];
		std::strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
		return buff;

	}

	// This function converts level enum to string
	std::string levelToString(Level level) {
		switch (level) {
		case INFO: return "INFO";
		case WARNING: return "WARNING";
		case ERROR: return "ERROR";
		default: return "UNKNOWN";
		
		}
	}
};

int main()
{
	Logger logger("app.log");
	logger.log(Logger::INFO, "Program Started");
	logger.log(Logger::WARNING, "Low Memory");
	return 0;

}