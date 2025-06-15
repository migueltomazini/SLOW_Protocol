// UdpSocket.cpp
// Este arquivo implementa a classe UdpSocket, fornecendo a funcionalidade
// de comunicação UDP. Ele encapsula as chamadas de sistema de baixo nível
// para criar, ligar, enviar e receber dados em um socket UDP, tornando a
// interação com a rede mais simples e orientada a objetos para o resto da aplicação.

#include "UdpSocket.h"
#include <iostream>   // Para std::cerr (saída de erro)
#include <unistd.h>   // Para close()
#include <cstring>    // Para memset() e memcpy()

UdpSocket::UdpSocket() : sockfd(-1) {
    // O construtor inicializa o file descriptor do socket como -1,
    // um valor inválido que indica que o socket ainda não foi criado.
    // Isso nos permite verificar facilmente se o socket está aberto.
    memset(&server_addr, 0, sizeof(server_addr));
}

UdpSocket::~UdpSocket() {
    // O destrutor garante que, se o socket foi aberto (sockfd != -1),
    // ele seja fechado para liberar os recursos do sistema operacional.
    // Isso evita "resource leaks".
    if (sockfd != -1) {
        close(sockfd);
    }
}

bool UdpSocket::bindSocket(int port) {
    // 1. Criar o descritor de arquivo do socket
    // AF_INET: para a família de endereços IPv4.
    // SOCK_DGRAM: para o protocolo UDP (datagramas).
    // 0: protocolo padrão para SOCK_DGRAM, que é UDP.
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Erro ao criar socket" << std::endl;
        return false;
    }
    
    // Configurar o endereço para o qual vamos ligar (bind) o socket
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    // INADDR_ANY: aceita conexões de qualquer interface de rede da máquina.
    local_addr.sin_addr.s_addr = INADDR_ANY;
    // htons (host to network short): converte o número da porta para a ordem de bytes da rede.
    local_addr.sin_port = htons(port);

    // 2. Ligar (bind) o socket a um endereço e porta
    // Isso é necessário para poder receber pacotes em uma porta específica.
    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        std::cerr << "Erro ao fazer bind do socket na porta " << port << std::endl;
        close(sockfd); // Limpa o socket se o bind falhar
        sockfd = -1;
        return false;
    }

    return true;
}

// Implementação do helper privado para resolver o nome do host
bool UdpSocket::resolveHostname(const std::string& hostname, int port, struct sockaddr_in& addr_out) {
    // gethostbyname é uma função que converte um nome de host (ex: "slow.gmelodie.com")
    // em uma estrutura que contém seu endereço IP.
    struct hostent* host_entry = gethostbyname(hostname.c_str());
    if (host_entry == nullptr) {
        std::cerr << "Erro: não foi possível resolver o hostname '" << hostname << "'" << std::endl;
        return false;
    }
    
    // Preenche a estrutura sockaddr_in com as informações do destino
    addr_out.sin_family = AF_INET;
    // Copia o endereço IP resolvido para a estrutura de endereço.
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
    // Resolve o hostname para obter o endereço IP do destino
    if (!resolveHostname(host, port, dest_addr)) {
        return -1;
    }

    // sendto: envia os dados para o destino especificado.
    // data.data(): ponteiro para os dados do vector.
    // data.size(): tamanho dos dados a serem enviados.
    // 0: flags (nenhuma especial neste caso).
    ssize_t bytes_sent = sendto(sockfd, data.data(), data.size(), 0, 
                                (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    
    if (bytes_sent < 0) {
        std::cerr << "Erro ao enviar dados para " << host << ":" << port << std::endl;
    }
    
    return bytes_sent;
}

ssize_t UdpSocket::receive(std::vector<uint8_t>& buffer) {
     if (sockfd == -1) {
        std::cerr << "Erro: socket não está ligado para recebimento." << std::endl;
        return -1;
    }
    // recv: é uma chamada bloqueante que espera até que dados cheguem no socket.
    // O buffer deve ter tamanho suficiente para receber o maior pacote esperado.
    // O valor retornado é o número de bytes recebidos.
    ssize_t bytes_received = recv(sockfd, buffer.data(), buffer.size(), 0);

    if (bytes_received < 0) {
        std::cerr << "Erro ao receber dados." << std::endl;
    }

    return bytes_received;
}

bool UdpSocket::setReceiveTimeout(int seconds, int microseconds) {
    if (sockfd == -1) {
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = microseconds;

    // SO_RCVTIMEO é a opção de socket para definir o timeout de recebimento
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "Erro ao configurar o timeout do socket" << std::endl;
        return false;
    }
    return true;
}


ssize_t UdpSocket::receiveFrom(std::vector<uint8_t>& buffer, std::string& sender_ip, int& sender_port) {
    if (sockfd == -1) {
        std::cerr << "Erro: socket não está ligado para recebimento." << std::endl;
        return -1;
    }

    // ADICIONADO: Declaração das variáveis que estavam faltando
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    // recvfrom: similar ao recv, mas também preenche uma estrutura com
    // o endereço do remetente (IP e porta).
    ssize_t bytes_received = recvfrom(sockfd, buffer.data(), buffer.size(), 0,
                                      (struct sockaddr *)&sender_addr, &addr_len);

    if (bytes_received > 0) {
        sender_ip = inet_ntoa(sender_addr.sin_addr);
        sender_port = ntohs(sender_addr.sin_port);
    } else if (bytes_received < 0) {
        // Em um socket com timeout, EAGAIN ou EWOULDBLOCK é retornado quando o timeout ocorre.
        // Isso não é um erro fatal, é um comportamento esperado para sair do bloqueio.
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
             perror("Erro em recvfrom"); // Imprime o erro real do sistema, se houver um.
        }
    }

    return bytes_received;
}