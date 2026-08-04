cmake_minimum_required(VERSION 3.16)
project(social LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

option(SOCIAL_ENABLE_SANITIZERS "Build with ASan+UBSan (debug builds only)" OFF)
if(SOCIAL_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -g)
    add_link_options(-fsanitize=address,undefined)
endif()

# ---- core library: everything except entry points ----
add_library(social_core
    src/FileHandler.cpp
)
target_include_directories(social_core PUBLIC include)

# ---- console app ----
add_executable(social_console src/main.cpp)
target_link_libraries(social_console PRIVATE social_core)

# ---- HTTP server ----
add_executable(social_server src/server.cpp)
target_link_libraries(social_server PRIVATE social_core)
if(WIN32)
    target_link_libraries(social_server PRIVATE ws2_32)
endif()

# ---- tests ----
enable_testing()
add_executable(test_trie tests/test_trie.cpp)
target_link_libraries(test_trie PRIVATE social_core)
add_test(NAME TrieTests COMMAND test_trie)

add_executable(test_doubly_linked_list tests/test_doubly_linked_list.cpp)
target_link_libraries(test_doubly_linked_list PRIVATE social_core)
add_test(NAME DoublyLinkedListTests COMMAND test_doubly_linked_list)

add_executable(test_filehandler tests/test_filehandler.cpp)
target_link_libraries(test_filehandler PRIVATE social_core)
add_test(NAME FileHandlerTests COMMAND test_filehandler)

add_executable(test_password_hasher tests/test_password_hasher.cpp)
target_link_libraries(test_password_hasher PRIVATE social_core)
add_test(NAME PasswordHasherTests COMMAND test_password_hasher)
