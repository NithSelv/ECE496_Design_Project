#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <iostream>
#include <sys/resource.h>
#include <time.h>

//This class is used to add/update/store metrics for use by the server
class Metrics_Database {
    private:
	std::map<std::string, std::string> mdb;
	std::map<std::string, std::string>::iterator it;
    public:

	void Add_Metric(const char* name, char* value) {
		this->mdb.insert(std::make_pair(name, value));
	}

	std::string Find_Metric(char* name) { //changed return type from char* (unused so far)
		this->it = mdb.find(name);
		if (this->it != mdb.end()) {
			return this->it->second; //found
		} else {
			return NULL; // not found
		}
	}

	int Set_Metric(const char* name, char* value) {
	    this->it = this->mdb.find(name);
	    if (this->it != this->mdb.end()) { //found
		    this->mdb.erase(it);
		    this->mdb.insert(std::make_pair(name, value));
		    return 0;
	    } else {
		    return -1;
	    }
	}

	std::string Prepare_All_Metrics_Body() { //changed return type from char* (used in server.cpp)
		std::string msg;
		for (this->it=this->mdb.begin(); this->it!=this->mdb.end(); ++it) {
			msg.append(this->it->first);
			msg.append(" ");
			msg.append(this->it->second);
			msg.append("\n");
		}
		return msg;
	}

	void Print() {
		for (this->it=this->mdb.begin(); this->it!=this->mdb.end(); ++it) {
			std::cout << this->it->first << " => " << this->it->second << std::endl;
		}
	}

	int Initialize(double start_time) {
    	    struct rusage stats;
    	    struct timeval now;
    	    char data[4096];
    	    memset((void*)data, 0, sizeof(data));
    	    gettimeofday(&now, NULL);
    	    int ret = getrusage(RUSAGE_SELF, &stats);
    	    if (ret == 0) {
		double cpu_time = stats.ru_utime.tv_sec + 0.000001 * stats.ru_utime.tv_usec;
		sprintf(data, "%f", cpu_time);
    		this->Add_Metric("cpu_time_sec", data);
		memset((void*)data, 0, sizeof(data));
		double calendar_time = now.tv_sec + 0.000001 * now.tv_usec - start_time;
		sprintf(data, "%f", calendar_time);
		this->Add_Metric("calendar_time", data);
    	    }
    	    return ret;
	}

	int Update(double start_time) {
    	    struct rusage stats;
    	    struct timeval now;
    	    char data[4096];
    	    memset((void*)data, 0, sizeof(data));
    	    gettimeofday(&now, NULL);
    	    int ret = getrusage(RUSAGE_SELF, &stats);
    	    if (ret == 0) {
	    	double cpu_time = stats.ru_utime.tv_sec + 0.000001 * stats.ru_utime.tv_usec;
		sprintf(data, "%f", cpu_time);
    		this->Set_Metric("cpu_time_sec", data);
		memset((void*)data, 0, sizeof(data));
		double calendar_time = now.tv_sec + 0.000001 * now.tv_usec - start_time;
		sprintf(data, "%f", calendar_time);
		this->Set_Metric("calendar_time", data);
     	    }
    	    return ret;
	}

};

