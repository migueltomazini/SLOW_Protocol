/**
 * @file UdpSocket.h
 * @brief Definição da classe UdpSocket, uma abstração para comunicação UDP.
 *
 * Esta classe encapsula as chamadas de sistema de baixo nível (POSIX sockets)
 * para criar, ligar, enviar e receber datagramas UDP, simplificando a
 * interação com a rede para as camadas superiores da aplicação.
 */

#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include <string>
#include <vector>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    // Desabilita construtores de cópia e operadores de atribuição para evitar duplicação de sockets.
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool bindSocket(int port);
    ssize_t sendTo(const std::vector<uint8_t>& data, const std::string& host, int port);
    ssize_t receiveFrom(std::vector<uint8_t>& buffer, std::string& sender_ip, int& sender_port);
    bool setReceiveTimeout(int seconds, int microseconds);

private:
    int sockfd;
    
    bool resolveHostname(const std::string& hostname, int port, struct sockaddr_in& addr_out);
};

#endif // UDP_SOCKET_H