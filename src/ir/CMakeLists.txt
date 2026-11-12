add_library(cpy_ir STATIC IRGeneration.cpp)

# Find the libraries that correspond to the LLVM components that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader)

# Link against LLVM libraries
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_link_libraries(cpy_ir PRIVATE ${llvm_libs} cpy_frontend)
target_compile_definitions(cpy_ir PRIVATE ${LLVM_DEFINITIONS_LIST})

target_include_directories(cpy_ir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(cpy_ir SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
