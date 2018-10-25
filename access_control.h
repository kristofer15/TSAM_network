#include <map>
#include <string>
#include <iostream>

class AccessControl {
public:
    AccessControl() {
        locks["LISTSERVERS"] = 0b1111;
        locks["ID"] = 0b1110;
        locks["CONNECT"] = 0b1110;
        locks["WHO"] = 0b1110;
        locks["MSG"] = 0b1110;
        locks["LEAVE"] = 0b1110;
        locks["KEEPALIVE"] = 0b1110;
        locks["LISTROUTES"] = 0b1110;
        locks["CMD"] = 0b1110;
        locks["FETCH"] = 0b1110;
        locks["SHUTDOWN"] = 0b1100;
        locks["ADDSERVER"] = 0b1100;
        locks["RSP"] = 0b0010;
        
        locks["META_REQUEST_ID"] = 0b1000;
    }

    ~AccessControl() {}

    bool permit(std::string role, std::string command) {
        int k = generate_key(role);
        return k & locks[command] ? true : false;
    }


private:
    std::map<std::string, int> locks;

    int generate_key(std::string& role) {

        if(role == "control") {
            return 0b0100;
        }
        else if(role == "network") {
            return 0b0010;
        }
        else if(role == "info") {
            return 0b0001;
        }
        else if(role == "root") {
            return 0b1000;
        }
        else {
            return 0b0000;
        }
    }
};