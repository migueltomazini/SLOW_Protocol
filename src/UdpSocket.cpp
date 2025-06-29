/**
 * @file UdpSocket.cpp
 * @brief Implementação da classe UdpSocket para comunicação UDP.
 *
 * Fornece a funcionalidade de comunicação UDP, encapsulando as chamadas de
 * sistema de baixo nível para criar, ligar, enviar e receber dados em um
 * socket UDP, tornando a interação com a rede mais simples e orientada a objetos.
 */

#include "UdpSocket.h"
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <cerrno> // Para errno

UdpSocket::UdpSocket() : sockfd(-1) {}

UdpSocket::~UdpSocket() {
    if (sockfd != -1) {
        close(sockfd);
    }
}

bool UdpSocket::bindSocket(int port) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket");
        return false;
    }
    
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY; // Aceita pacotes de qualquer interface local
    local_addr.sin_port = htons(port);       // 'port = 0' seleciona uma porta efêmera

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Erro ao fazer bind do socket");
        close(sockfd);
        sockfd = -1;
        return false;
    }
    return true;
}

bool UdpSocket::resolveHostname(const std::string& hostname, int port, struct sockaddr_in& addr_out) {
    struct hostent* host_entry = gethostbyname(hostname.c_str());
    if (host_entry == nullptr) {
        std::cerr << "Erro: não foi possível resolver o hostname '" << hostname << "'" << std::endl;
        return false;
    }
    
    memset(&addr_out, 0, sizeof(addr_out));
    addr_out.sin_family = AF_INET;
    memcpy(&addr_out.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
    addr_out.sin_port = htons(port);
    
    return true;
}

ssize_t UdpSocket::sendTo(const std::vector<uint8_t>& data, const std::string& host, int port) {
    if (sockfd == -1) {
        std::cerr << "Erro: socket não está inicializado para envio." << std::endl;
        return -1;
    }
    
    struct sockaddr_in dest_addr;
    if (!resolveHostname(host, port, dest_addr)) {
        return -1;
    }

    ssize_t bytes_sent = sendto(sockfd, data.data(), data.size(), 0, 
                                (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    
    if (bytes_sent < 0) {
        perror("Erro ao enviar dados");
    }
    return bytes_sent;
}

ssize_t UdpSocket::receiveFrom(std::vector<uint8_t>& buffer, std::string& sender_ip, int& sender_port) {
    if (sockfd == -1) {
        std::cerr << "Erro: socket não está ligado para recebimento." << std::endl;
        return -1;
    }

    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    ssize_t bytes_received = recvfrom(sockfd, buffer.data(), buffer.size(), 0,
                                      (struct sockaddr *)&sender_addr, &addr_len);

    if (bytes_received > 0) {
        sender_ip = inet_ntoa(sender_addr.sin_addr);
        sender_port = ntohs(sender_addr.sin_port);
    } else if (bytes_received < 0) {
        // Em um socket com timeout, EAGAIN ou EWOULDBLOCK não são erros fatais,
        // apenas indicam que o timeout foi atingido sem receber dados.
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
             perror("Erro em recvfrom");
        }
    }
    return bytes_received;
}

bool UdpSocket::setReceiveTimeout(int seconds, int microseconds) {
    if (sockfd == -1) return false;

    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = microseconds;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Erro ao configurar o timeout do socket");
        return false;
    }
    return true;
}