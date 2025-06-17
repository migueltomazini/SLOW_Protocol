/**
 * @file main.cpp
 * @brief Ponto de entrada para a aplicação de teste do Periférico SLOW.
 *
 * Este executável demonstra o ciclo de vida completo da comunicação do Periférico
 * com o Central utilizando o protocolo SLOW. Ele realiza as seguintes etapas de
 * forma automatizada:
 * 1. Estabelecimento de conexão (handshake de 3 vias).
 * 2. Envio de um pacote de dados simples e espera pela confirmação (ACK).
 * 3. Envio de dados maiores para testar a funcionalidade de fragmentação.
 * 4. Envio de um pedido de desconexão para encerrar a sessão.
 * A interação do usuário foi removida em favor de esperas cronometradas para
 * criar um fluxo de teste contínuo e demonstrativo.
 */

#include "Peripheral.h"
#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <chrono>
#include <limits> // Para std::numeric_limits

/**
 * @brief Imprime um relatório de status detalhado do periférico.
 * @param p Referência constante ao objeto Peripheral.
 */
void print_status(const Peripheral& p) {
    std::cout << "---[ STATUS REPORT ]---" << std::endl;
    std::cout << "  - Estado:         " << p.getStateAsString() << std::endl;
    std::cout << "  - Session STTL:   " << p.getSessionSTTL() << std::endl;
    std::cout << "  - Próximo SeqNum: " << p.getCurrentSeqNum() << std::endl;
    std::cout << "  - Pacotes sem ACK:" << p.getUnackedPacketCount() << std::endl;
    std::cout << "-----------------------" << std::endl;
}

/**
 * @brief Imprime as instruções de uso do programa.
 * @param prog_name Nome do executável.
 */
void print_usage(const char* prog_name) {
    std::cerr << "Uso: " << prog_name << " <central_ip> <central_port>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string central_ip = argv[1];
    int central_port;
    try {
        central_port = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "Erro: Porta inválida '" << argv[2] << "'" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    std::cout << ">>> INICIANDO TESTE COMPLETO DO PERIFÉRICO SLOW <<<" << std::endl;
    std::cout << "Conectando ao Central em " << central_ip << ":" << central_port << std::endl;

    try {
        Peripheral peripheral(central_ip, central_port);

        // A thread de rede é iniciada para ouvir as respostas do Central em segundo plano.
        std::thread network_thread([&peripheral]() {
            peripheral.run();
        });

        // --- ETAPA 1: ESTABELECIMENTO DA CONEXÃO ---
        std::cout << "\n--- ETAPA 1: Iniciando conexão ---" << std::endl;
        if (!peripheral.start()) {
            if (network_thread.joinable()) network_thread.join();
            return 1;
        }

        std::cout << "Aguardando conclusão do handshake..." << std::endl;
        auto start_time = std::chrono::steady_clock::now();
        while (!peripheral.isConnected()) {
            if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(5)) {
                std::cerr << "\nFALHA: Handshake não concluído em 5 segundos." << std::endl;
                if (network_thread.joinable()) network_thread.join();
                return 1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "\n>>> Conexão estabelecida com sucesso! Estado PÓS-HANDSHAKE:" << std::endl;
        print_status(peripheral);

        std::cout << "\n(Pausa de 2 segundos para visualização...)\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // --- ETAPA 2: ENVIO DE DADOS SIMPLES ---
        std::cout << "\n--- ETAPA 2: Enviando um pacote de dados simples ---" << std::endl;
        std::string message = "Teste com STTL da sessao.";
        std::vector<uint8_t> data_to_send(message.begin(), message.end());
        
        peripheral.sendData(data_to_send);
        
        std::cout << "Pacote enviado. Aguardando confirmação (ACK)..." << std::endl;
        start_time = std::chrono::steady_clock::now();
        while (peripheral.getUnackedPacketCount() > 0) {
            if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10)) {
                std::cerr << "\nFALHA: Não foi recebido ACK para o pacote simples em 10 segundos." << std::endl;
                break; // Sai do loop para continuar para a desconexão
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (peripheral.getUnackedPacketCount() == 0) {
            std::cout << ">>> Confirmação (ACK) para pacote simples recebida!" << std::endl;
        }
        print_status(peripheral);

        std::cout << "\n(Pausa de 2 segundos para visualização...)\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // --- ETAPA 3: ENVIO DE DADOS FRAGMENTADOS ---
        std::cout << "\n--- ETAPA 3: Enviando dados para fragmentação ---" << std::endl;
        std::vector<uint8_t> large_data(1500, 'B'); // Gera 1500 bytes de dados
        peripheral.sendData(large_data);

        std::cout << "Pacotes fragmentados enviados. Aguardando confirmações..." << std::endl;
        start_time = std::chrono::steady_clock::now();
        while (peripheral.getUnackedPacketCount() > 0) {
            if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(15)) {
                std::cerr << "\nFALHA: Não foram recebidos todos os ACKs para os fragmentos em 15 segundos." << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        if (peripheral.getUnackedPacketCount() == 0) {
            std::cout << ">>> Todas as confirmações (ACKs) para os fragmentos foram recebidas!" << std::endl;
        }
        print_status(peripheral);

        std::cout << "\n(Pausa de 2 segundos para visualização...)\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // --- ETAPA 4: DESCONEXÃO ---
        std::cout << "\n--- ETAPA 4: Enviando pedido de desconexão ---" << std::endl;
        peripheral.sendDisconnect();
        
        std::cout << "Aguardando 3 segundos para finalização da thread de rede..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        if (network_thread.joinable()) {
            network_thread.join();
        }

        std::cout << "\n>>> TESTE CONCLUÍDO <<<" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Erro inesperado na aplicação: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}