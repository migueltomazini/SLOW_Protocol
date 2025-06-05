// UdpSocket.h
// Este arquivo de cabeçalho define uma interface para a comunicação UDP.
// Ele declara a classe UdpSocket, que abstrai as operações de baixo nível
// de sockets UDP, como a criação, ligação a uma porta e envio/recebimento de dados.
// É a camada responsável por interagir diretamente com o sistema operacional
// para a transmissão e recepção dos datagramas UDP.

#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include <string>
#include <vector>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Porta UDP para o protocolo SLOW
const int SLOW_PORT = 7033;

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    // Inicializa o socket para o peripheral (liga a uma porta específica)
    bool bindSocket(int port);

    // Envia dados para um endereço e porta específicos
    ssize_t sendTo(const std::vector<uint8_t>& data, const std::string& host, int port);

    // Recebe dados
    ssize_t receive(std::vector<uint8_t>& buffer);

    // Recebe dados de um endereço e porta específicos
    ssize_t receiveFrom(std::vector<uint8_t>& buffer, std::string& sender_ip, int& sender_port);

private:
    int sockfd;
    struct sockaddr_in server_addr;

    // Resolve o nome do host
    bool resolveHostname(const std::string& hostname, int port, struct sockaddr_in& addr_out);
};

#endif