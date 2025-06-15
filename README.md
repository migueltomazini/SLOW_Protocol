# Protocolo SLOW (Peripheral)

Este repositório contém a implementação do lado **Peripheral** do protocolo de transporte SLOW. O SLOW é um protocolo ad hoc construído sobre UDP, projetado para controlar o fluxo de dados com funcionalidades de confiabilidade, como controle de fluxo por janela deslizante e fragmentação/remontagem de pacotes.

## Objetivo do Projeto

O objetivo deste projeto é implementar o lado do cliente (peripheral) do protocolo SLOW, que se comunica com um servidor (central) para estabelecer conexões, enviar dados de forma confiável e gerenciar a sessão.

## Como Compilar o Projeto

Este projeto utiliza CMake para gerenciar o processo de compilação. Siga os passos abaixo para compilar o código:

1.  **Clone o Repositório:**
    Primeiro, clone este repositório para sua máquina local:
    ```bash
    git clone [https://github.com/seu-usuario/SLOW_Protocol.git](https://github.com/seu-usuario/SLOW_Protocol.git)
    cd SLOW_Protocol
    ```
    (Lembre-se de substituir `https://github.com/seu-usuario/SLOW_Protocol.git` pelo URL real do seu repositório).

2.  **Crie um Diretório de Build:**
    É uma boa prática criar um diretório separado para os arquivos de build. Isso mantém seu código-fonte limpo.
    ```bash
    mkdir build
    cd build
    ```

3.  **Execute o CMake:**
    A partir do diretório `build`, execute o CMake para gerar os arquivos de build para o seu sistema. O `..` indica que o arquivo `CMakeLists.txt` está no diretório pai.
    ```bash
    cmake ..
    ```

4.  **Compile o Projeto:**
    Após o CMake ter gerado os arquivos, use `make` para compilar o projeto.
    ```bash
    make
    ```
    Se a compilação for bem-sucedida, você encontrará o executável `peripheral` dentro do diretório `build`.

## Como Rodar o Projeto

O executável `peripheral` requer dois argumentos de linha de comando: o endereço IP do central e a porta UDP na qual o central está escutando.

1.  **Navegue até o Diretório de Build:**
    Certifique-se de estar no diretório `build` onde o executável foi gerado:
    ```bash
    cd build
    ```

2.  **Execute o Peripheral:**
    Para testar seu peripheral, você usará o central fornecido pelo enunciado do trabalho, que está disponível em `slow.gmelodie.com` na porta `7033`.
    ```bash
    ./peripheral 142.93.184.175 7033
    ```
    Ao executar, o peripheral tentará iniciar uma conexão 3-way com o central e processar as mensagens conforme o protocolo SLOW.

---

## 👥 Autores

* Antonio Carlos de Almeida Micheli Neto
* Júlia Cavalio Orlando
* Miguel Rodrigues Tomazini