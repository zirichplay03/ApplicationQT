cmake_minimum_required(VERSION 3.31)
project(ApplicationQT)
cmake_minimum_required(VERSION 3.16)
project(ApplicationQt LANGUAGES CXX)  # Имя проекта должно совпадать с target

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(untitled1 main.cpp)
# ОТКЛЮЧАЕМ AUTOMOC
set(CMAKE_AUTOMOC OFF)

# АБСОЛЮТНЫЙ путь к Qt (измени под свой путь)
set(QT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Qt/Qt-6.7.0")

# Проверяем что Qt существует
if(NOT EXISTS "${QT_ROOT}/bin/moc.exe")
    message(FATAL_ERROR "Qt not found at: ${QT_ROOT}")
endif()

message(STATUS "Using Qt from: ${QT_ROOT}")

# Создаем приложение с правильным именем
add_executable(ApplicationQt WIN32 main.cpp)  # Имя target должно совпадать с проектом

# ВАЖНО: добавляем include директории
target_include_directories(ApplicationQt PRIVATE
        "${QT_ROOT}/include"
        "${QT_ROOT}/include/QtCore"
        "${QT_ROOT}/include/QtGui"
        "${QT_ROOT}/include/QtWidgets"
)

# Добавляем директорию с библиотеками
target_link_directories(ApplicationQt PRIVATE "${QT_ROOT}/lib")

# Линкуем библиотеки (используем полные пути)
target_link_libraries(ApplicationQt PRIVATE
        "${QT_ROOT}/lib/libQt6Core.a"
        "${QT_ROOT}/lib/libQt6Gui.a"
        "${QT_ROOT}/lib/libQt6Widgets.a"
)

# Добавляем флаг для GUI приложения
target_link_options(ApplicationQt PRIVATE -mwindows)

# Копируем DLL и плагины
add_custom_command(TARGET ApplicationQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Deploying Qt files..."

        # Копируем основные DLL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_ROOT}/bin/Qt6Core.dll"
        "${QT_ROOT}/bin/Qt6Gui.dll"
        "${QT_ROOT}/bin/Qt6Widgets.dll"
        $<TARGET_FILE_DIR:ApplicationQt>

        # Создаем папку platforms и копируем плагин
        COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ApplicationQt>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_ROOT}/plugins/platforms/qwindows.dll"
        "$<TARGET_FILE_DIR:ApplicationQt>/platforms/"

        COMMENT "Copying Qt runtime files"
)