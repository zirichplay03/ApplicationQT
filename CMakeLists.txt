cmake_minimum_required(VERSION 3.16)  # Повысьте версию для Qt6
project(QtApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Отключите GNU расширения для совместимости

# Включаем автоматическую обработку moc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)  # Добавьте это для UI файлов

# Находим компоненты Qt
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Если нужен Network, но у вас его нет в MSYS2
# find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# Указываем файлы проекта
set(PROJECT_SOURCES
        main.cpp
        MainWindow.cpp
        ComPort.cpp
        Enod.cpp
        # Добавьте все .cpp файлы
)

set(HEADERS
        MainWindow.h
        ComPort.h      # Исправьте имя, если у вас ComPortBase.h
        Enod.h
        # Добавьте все .h файлы
)

# Добавляем исполняемый файл
add_executable(QtApp WIN32 ${PROJECT_SOURCES} ${HEADERS})

# Добавляем include директории
target_include_directories(QtApp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Связываем с библиотеками Qt
target_link_libraries(QtApp PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        # Qt6::Network  # Раскомментируйте если установлен
)

# Для Windows добавляем дополнительные библиотеки
if(WIN32)
    target_link_libraries(QtApp PRIVATE setupapi)

    # Отключаем консольное окно (GUI приложение)
    if(MINGW OR MSYS)
        target_link_options(QtApp PRIVATE -mwindows)
    endif()
endif()

# Копирование DLL (опционально, для удобства)
add_custom_command(TARGET QtApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Build complete!"
        COMMENT "QtApp built successfully"
)