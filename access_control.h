#include <map>
#include <string>
#include <iostream>

class AccessControl {
public:
    AccessControl() {
        locks["LISTSERVERS"] = 0b111;
        locks["ID"] = 0b110;
        locks["CONNECT"] = 0b110;
        locks["WHO"] = 0b110;
        locks["MSG"] = 0b110;
        locks["LEAVE"] = 0b110;
        locks["KEEPALIVE"] = 0b110;
        locks["LISTROUTES"] = 0b110;
        locks["CMD"] = 0b110;
        locks["FETCH"] = 0b110;
        locks["SHUTDOWN"] = 0b100;
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
            return 0b100;
        }
        else if(role == "network") {
            return 0b010;
        }
        else if(role == "info") {
            return 0b001;
        }
        else {
            return 0b000;
        }
    }
};