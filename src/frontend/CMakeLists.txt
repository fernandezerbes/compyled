add_library(cpy_frontend STATIC AST.cpp Tokenizer.cpp Parser.cpp)

target_include_directories(cpy_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
