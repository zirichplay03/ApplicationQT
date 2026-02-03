cmake_minimum_required(VERSION 3.10)

project(QtApp)

set(CMAKE_CXX_STANDARD 17)

# Включаем автоматическую обработку moc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Находим компоненты Qt
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# У

# Указываем файлы проекта
set(PROJECT_SOURCES main.cpp MainWindow.cpp ComPort.cpp Enod.cpp)

set(HEADERS
        MainWindow.h
        ComPortBase.h
        Enod.h
)

# Добавляем исполняемый файл
add_executable(QtApp ${PROJECT_SOURCES})

target_link_libraries(QtApp PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
)


# Отключаем консольное окно для MinGW
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-subsystem,windows")
endif()