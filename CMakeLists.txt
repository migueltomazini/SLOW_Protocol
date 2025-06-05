# CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
project(SlowProtocol CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(src)

# Lista todos os arquivos .cpp que compõem o executável do projeto.
add_executable(peripheral
    src/main.cpp
    src/SlowPacket.cpp
    src/UdpSocket.cpp
    src/Peripheral.cpp
    src/Utils.cpp
)

target_link_libraries(peripheral PUBLIC
    pthread
)

# cmake_minimum_required(VERSION 3.10)
# project(SlowProtocol CXX)

# # Define o padrão C++ a ser usado. C++17 é uma boa escolha.
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED True)
# set(CMAKE_CXX_EXTENSIONS OFF) # Garante que as extensões GNU não sejam usadas por padrão

# # Adiciona o diretório 'src' para que o compilador possa encontrar os arquivos de cabeçalho (.h)
# # quando eles são incluídos em outros arquivos (ex: #include "SlowPacket.h").
# include_directories(src)

# # --- NOVO: Executável de Teste para Utils e SlowPacket ---
# add_executable(test_utils_slowpacket
#     src/test_main.cpp # O main.cpp para testar Utils e SlowPacket
#     src/SlowPacket.cpp
#     src/Utils.cpp
# )

# # test_utils_slowpacket não precisa de pthread ou outras libs de rede, apenas as padrões.
# # Se `std::array` ou `std::vector` (incluídos via SlowPacket/Utils) precisarem de alguma lib específica
# # (geralmente não precisam), você adicionaria aqui.


# # Opcional: Adicionar flags de compilação úteis para desenvolvimento
# # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")
# # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

# # Opcional: Configurar diretório de saída para executáveis
# # set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")