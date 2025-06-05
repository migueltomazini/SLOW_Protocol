// UdpSocket.cpp
// Este arquivo implementa as funcionalidades da classe UdpSocket.
// Ele contém o código para as chamadas de sistema POSIX (sockets) que permitem
// a comunicação UDP, como `socket()`, `bind()`, `sendto()` e `recvfrom()`.
// Garante que o peripheral possa enviar e receber pacotes SLOW
// através da rede, usando a porta UDP/7033 conforme especificado.

#include "UdpSocket.h"
#include "SlowPacket.h"
#include <iostream>  
#include <cstring>
#include <unistd.h>

// Construtor da classe UdpSocket.
// Inicializa o descritor de arquivo do socket como -1 (inválido) por padrão.
UdpSocket::UdpSocket() : sockfd(-1) {}

// Destrutor da classe UdpSocket.
// Garante que o socket seja fechado se estiver aberto, liberando recursos.
UdpSocket::~UdpSocket() {
    if (sockfd != -1) {
        close(sockfd);
        std::cout << "Socket UDP fechado." << std::endl;
    }
}

/**
 * @brief Cria e liga o socket UDP a uma porta específica na máquina local.
 * Este é o passo necessário para o socket poder receber dados.
 * @param port A porta UDP para a qual o socket será ligado (ex: 7033 para o SLOW Protocol).
 * @return true se o socket foi criado e ligado com sucesso, false caso contrário.
 */
bool UdpSocket::bindSocket(int port) {
    // Cria um socket UDP.
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Erro ao criar socket UDP." << std::endl;
        return false;
    }

    // Prepara a estrutura de endereço do servidor (máquina local).
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;       
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // Liga o socket à porta e endereço definidos.
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Erro ao ligar socket à porta " << port << ". Verifique se a porta está em uso." << std::endl;
        close(sockfd); 
        sockfd = -1;
        return false;
    }
    return true; 
}

/**
 * @brief Resolve um nome de host (ou endereço IP) para uma estrutura de endereço de socket (sockaddr_in).
 * Esta função utiliza o sistema DNS para converter nomes de domínio em endereços IP,
 * @param hostname A string contendo o nome do host (ex: "slow.gmelodie.com") ou um endereço IP literal (ex: "127.0.0.1").
 * @param port A porta numérica associada ao host para a qual o endereço será configurado.
 * @param addr_out Uma referência para a estrutura `sockaddr_in` onde o endereço resolvido será armazenado.
 * @return true se o nome do host foi resolvido com sucesso e a estrutura `addr_out` foi preenchida, false caso contrário.
 */
bool UdpSocket::resolveHostname(const std::string& hostname, int port, struct sockaddr_in& addr_out) {
    struct addrinfo hints, *servinfo, *p;
    int rv;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;   
    hints.ai_socktype = SOCK_DGRAM; 

    // Tenta resolver o nome do host.
    if ((rv = getaddrinfo(hostname.c_str(), std::to_string(port).c_str(), &hints, &servinfo)) != 0) {
        std::cerr << "Erro ao resolver host '" << hostname << "': " << gai_strerror(rv) << std::endl;
        return false;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        std::memcpy(&addr_out, p->ai_addr, p->ai_addrlen);
        break; 
    }

    freeaddrinfo(servinfo);
    return p != NULL;
}


/**
 * @brief Envia um vetor de bytes (dados) para um endereço IP e porta de destino específicos.
 * @param data O vetor de bytes a ser enviado.
 * @param host A string contendo o host do endereço.
 * @param port A porta UDP do destino.
 * @return O número de bytes enviados em caso de sucesso, ou -1 em caso de erro.
 */
ssize_t UdpSocket::sendTo(const std::vector<uint8_t>& data, const std::string& host_or_ip, int port) {
    struct sockaddr_in dest_addr;
    // Tenta resolver o nome do host/IP.
    if (!resolveHostname(host_or_ip, port, dest_addr)) {
        return -1; 
    }

    return sendto(sockfd, data.data(), data.size(), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
}

/**
 * @brief Recebe dados de qualquer remetente através do socket UDP.
 * @param buffer Um vetor de bytes onde os dados recebidos serão armazenados.
 * @return O número de bytes recebidos em caso de sucesso, 0 se não houver dados, ou -1 em caso de erro.
 */
ssize_t UdpSocket::receive(std::vector<uint8_t>& buffer) {
    // Redimensiona o buffer para o tamanho máximo esperado de um pacote SLOW.
    buffer.resize(MAX_SLOW_PACKET_SIZE);
    return recv(sockfd, buffer.data(), buffer.size(), 0);
}

/**
 * @brief Recebe dados de qualquer remetente através do socket UDP, e também obtém
 * o endereço IP e a porta do remetente.
 * @param buffer Um vetor de bytes onde os dados recebidos serão armazenados.
 * @param sender_ip Uma string que será preenchida com o endereço IP do remetente.
 * @param sender_port Um inteiro que será preenchido com a porta do remetente.
 * @return O número de bytes recebidos em caso de sucesso, 0 se não houver dados, ou -1 em caso de erro.
 */
ssize_t UdpSocket::receiveFrom(std::vector<uint8_t>& buffer, std::string& sender_ip, int& sender_port) {
    // Redimensiona o buffer para o tamanho máximo esperado de um pacote SLOW.
    buffer.resize(MAX_SLOW_PACKET_SIZE);
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    // recvfrom: Recebe dados do socket, e preenche a estrutura do remetente.
    ssize_t bytes_received = recvfrom(sockfd, buffer.data(), buffer.size(), 0,
                                     (struct sockaddr*)&sender_addr, &addr_len);

    if (bytes_received > 0) {
        // Converte o endereço IP binário do remetente para string.
        sender_ip = inet_ntoa(sender_addr.sin_addr);
        // Converte a porta do remetente da ordem de bytes de rede para a ordem de bytes do host.
        sender_port = ntohs(sender_addr.sin_port);
    }
    return bytes_received;
}