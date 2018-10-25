#include "network_handler.h"
#include "user_handler.h"
#include "access_control.h"
#include "main_controller.h"

void help(std::string file_name="server") {
    std::cout << "Usage:" << std::endl
              << file_name << " {TCP PORT} {UDP PORT}" << std::endl;
}

int main(int argc, char* argv[]) {

    if(argc < 4) {
        help(argv[0]);
        return 0;
    }

    NetworkHandler network(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    UserHandler users;
    AccessControl access;

    // DEBUG
    std::string id = "V_Group_51";
    if(argc == 5) { id = argv[4]; }

    MainController controller(network, users, access, id);
    controller.run();
}
