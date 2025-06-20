# CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
project(SlowProtocol CXX)

# Define o padrão C++ a ser usado. C++17 é uma boa escolha.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # Garante que as extensões GNU não sejam usadas por padrão

# Adiciona o diretório 'src' para que o compilador possa encontrar os arquivos de cabeçalho.
include_directories(src)

# Lista todos os arquivos .cpp que compõem o executável principal do projeto.
add_executable(peripheral
    src/main.cpp
    src/Peripheral.cpp
    src/SlowPacket.cpp
    src/UdpSocket.cpp
    src/Utils.cpp
)

# Linka a biblioteca pthread, necessária para std::thread em algumas plataformas.
target_link_libraries(peripheral PUBLIC
    pthread
)
