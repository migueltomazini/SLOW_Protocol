// main.cpp
// Este é o ponto de entrada principal da aplicação do peripheral SLOW.
// Ele é responsável por inicializar o objeto Peripheral, configurando a conexão
// com o central (IP e porta) e iniciando o ciclo de vida do protocolo.
// Através dele, o programa começa a se comunicar, gerenciar a sessão
// e realizar as operações de envio/recebimento de pacotes SLOW.

#include "Peripheral.h" // Inclui o cabeçalho da classe Peripheral
#include <iostream>   // Para entrada/saída (std::cerr, std::cout)
#include <string>     // Para std::string
#include <vector>     // Para std::vector (se for usar dados de exemplo)
#include <thread>     // Para std::this_thread::sleep_for
#include <chrono>     // Para std::chrono::seconds

int main(int argc, char* argv[]) {
    // Verifica se o número correto de argumentos de linha de comando foi fornecido.
    // Espera-se: nome_do_executavel <central_ip> <central_port>
    if (argc != 3) {
        std::cerr << "Uso: " << argv[0] << " <central_ip> <central_port>" << std::endl;
        return 1; // Retorna 1 para indicar um erro.
    }

    // Extrai o endereço IP e a porta do central dos argumentos de linha de comando.
    std::string central_ip = argv[1];
    int central_port = std::stoi(argv[2]); // Converte a string da porta para inteiro.

    // Cria uma instância da classe Peripheral, passando o IP e a porta do central.
    Peripheral peripheral(central_ip, central_port);

    // Tenta iniciar o peripheral (ligar o socket UDP).
    if (!peripheral.start()) {
        std::cerr << "Falha ao iniciar o peripheral. Encerrando." << std::endl;
        return 1; // Retorna 1 se o início falhar.
    }

    std::cout << "Tentando estabelecer conexao com o Central em " << central_ip << ":" << central_port << std::endl;

    // Inicia a conexão 3-way.
    // O peripheral envia o pacote CONNECT inicial.
    if (peripheral.sendConnect()) {
        std::cout << "Pacote CONNECT inicial enviado. Aguardando resposta do Central..." << std::endl;
        // O loop 'run()' da classe Peripheral irá lidar com o recebimento do pacote SETUP
        // e a transição para o estado CONNECTED.

        // Uma pausa curta pode ser útil para dar tempo ao central para responder ao CONNECT
        // antes de entrar no loop principal ou tentar enviar dados.
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Inicia o loop principal de execução do peripheral.
        // Este loop será responsável por receber pacotes, gerenciar retransmissões,
        // janelas deslizantes, etc.
        peripheral.run();
    } else {
        std::cerr << "Falha ao enviar o pacote CONNECT inicial. Encerrando." << std::endl;
        return 1;
    }

    // O código abaixo desta linha pode não ser alcançado se peripheral.run()
    // contiver um loop infinito (como é o caso da nossa implementação atual).
    // Se você implementar um mecanismo de parada para run(), este código pode ser útil.

    /*
    // Exemplo de como você poderia enviar dados APÓS a conexão ser estabelecida
    // (isto precisaria ser chamado APÓS o estado do peripheral se tornar CONNECTED)
    std::string message_to_send = "Ola Central, este eh um teste de dados do Peripheral!";
    std::vector<uint8_t> data_payload(message_to_send.begin(), message_to_send.end());

    // Em uma aplicação real, você teria uma forma de chamar sendData
    // e sendDisconnect com base em eventos ou lógica de aplicação,
    // e não imediatamente após o run().
    // if (peripheral.getCurrentState() == CONNECTED) { // Exemplo hipotético de checagem de estado
    //     peripheral.sendData(data_payload);
    //     std::this_thread::sleep_for(std::chrono::seconds(5));
    //     peripheral.sendDisconnect();
    // }
    */

    std::cout << "Peripheral encerrado." << std::endl;
    return 0; // Retorna 0 para indicar sucesso.
}