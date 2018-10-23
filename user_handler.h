#include <vector>
#include <map>
#include <string>

class UserHandler {
public:
    UserHandler() {}
    ~UserHandler() {}

    bool connect_user(int id, std::string user_name) {

        if(connected(id) || connected(user_name)) {
            return false;
        }

        id_user[id] = user_name;
        user_id[user_name] = id;

        return true;
    }

    std::string remove_user(int id) {
        if(!connected(id)) {
            return "";
        }

        std::string user = id_user[id];

        if(!connected(user)) {
            return "";
        }

        id_user.erase(id);
        user_id.erase(user);

        return user;
    }

    std::string user_name(int id) {
        return id_user[id];
    }

    int id(std::string user_name) {
        return user_id[user_name];
    }

    bool connected(std::string user_name) {
        return user_id[user_name] != 0;
    }

    bool connected(int id) {
        return id_user[id] != "";
    }

    std::vector<std::string> user_list() {
        std::vector<std::string> v;
        for(auto pair : user_id) {
            v.push_back(pair.first);
        }

        return v;
    }

private:

    // Keep two maps for quick bidirectional lookup.
    // We don't want to read through all user names to send a message
    std::map<int, std::string> id_user;
    std::map<std::string, int> user_id;
};
