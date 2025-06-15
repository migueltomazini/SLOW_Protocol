// main.cpp - VERSÃO DE DEBUG AVANÇADO
// Foco: Monitorar o estado do Periférico em tempo real.

#include "Peripheral.h"
#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <chrono>

// Função para pausar a execução e aguardar o usuário.
void press_enter_to_continue() {
    std::cout << "\n[PAUSA] Pressione Enter para continuar..." << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

// NOVA FUNÇÃO: Imprime um relatório de status detalhado do periférico.
void print_status(const Peripheral& p) {
    std::cout << "---[ STATUS REPORT ]---" << std::endl;
    std::cout << "  - Estado:         " << p.getStateAsString() << std::endl;
    std::cout << "  - Session STTL:   " << p.getSessionSTTL() << std::endl;
    std::cout << "  - Próximo SeqNum: " << p.getCurrentSeqNum() << std::endl;
    std::cout << "  - Pacotes sem ACK:" << p.getUnackedPacketCount() << std::endl;
    std::cout << "-----------------------" << std::endl;
}

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

    std::cout << ">>> INICIANDO TESTE DE CONEXÃO E STTL <<<" << std::endl;
    std::cout << "Conectando ao Central em " << central_ip << ":" << central_port << std::endl;

    try {
        Peripheral peripheral(central_ip, central_port);

        // ETAPA 1: INICIAR CONEXÃO
        std::cout << "\n--- ETAPA 1: Iniciando conexão ---" << std::endl;
        if (!peripheral.start()) {
            return 1;
        }

        std::thread network_thread([&peripheral]() {
            peripheral.run();
        });

        std::cout << "Aguardando handshake (3 segundos)..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        std::cout << "\n>>> Verificando estado PÓS-HANDSHAKE:" << std::endl;
        print_status(peripheral);
        press_enter_to_continue();

        // ETAPA 2: ENVIAR DADOS SIMPLES E MONITORAR
        std::cout << "\n--- ETAPA 2: Enviando um pacote de dados simples e iniciando monitoramento ---" << std::endl;
        std::string message = "Teste com STTL da sessao.";
        std::vector<uint8_t> data_to_send(message.begin(), message.end());
        
        std::cout << ">>> Estado ANTES de enviar dados:" << std::endl;
        print_status(peripheral);

        peripheral.sendData(data_to_send);
        
        std::cout << "\n>>> Pacote enviado. Iniciando ciclo de monitoramento por 12 segundos." << std::endl;
        std::cout << ">>> Observe os logs da thread de rede e as mudanças no STATUS REPORT." << std::endl;

        bool ack_received = false;
        for (int i = 0; i < 12; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "\n(Monitoramento: T+" << i + 1 << "s)" << std::endl;
            print_status(peripheral);

            // Se a fila de pacotes sem ACK ficou vazia, o ACK foi recebido!
            if (peripheral.getUnackedPacketCount() == 0) {
                std::cout << ">>> SUCESSO! O ACK foi recebido e processado." << std::endl;
                ack_received = true;
                break;
            }
        }
        
        if (!ack_received) {
             std::cout << "\n>>> FALHA! O ACK não foi recebido no tempo esperado. O timeout deve ter ocorrido." << std::endl;
        }

        press_enter_to_continue();

        // ETAPA 3: ENVIAR DADOS FRAGMENTADOS
        std::cout << "\n--- ETAPA 3: Enviando dados para fragmentação ---" << std::endl;
        std::vector<uint8_t> large_data(1500, 'B');
        peripheral.sendData(large_data);
        std::cout << "Aguardando 5 segundos para processamento dos fragmentos..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        print_status(peripheral);
        press_enter_to_continue();

        // ETAPA 4: DESCONECTAR
        std::cout << "\n--- ETAPA 4: Enviando pedido de desconexão ---" << std::endl;
        peripheral.sendDisconnect();
        
        std::cout << "Aguardando finalização da thread de rede..." << std::endl;
        if(network_thread.joinable()) {
            network_thread.join();
        }

        std::cout << "\n>>> TESTE CONCLUÍDO <<<" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Erro inesperado na aplicação: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}