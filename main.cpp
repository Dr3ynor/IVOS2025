#include "server.cpp"

int main()
{
    Server& server = Server::getInstance();
    server.create_socket();
    server.bind_socket();
    server.listen_socket();
    server.run();

    return 0;
}
