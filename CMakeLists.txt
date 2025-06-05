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

-Wall: Habilita todos os avisos de compilação.
