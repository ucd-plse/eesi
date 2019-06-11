project(eesi)
set(eesi_VERSION_MAJOR 0)
set(eesi_VERSION_MINOR 1)

cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)

include_directories(eesi)
include_directories(llvm-passes)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS} ${COMPILE_FLAGS})
link_directories(${LLVM_INSTALL_PREFIX}/lib /usr/lib/x86_65-linux-gnu)

find_package(Boost COMPONENTS program_options REQUIRED)

set(EESI_FILES
        eesi/main.cpp
        )
set(PASS_FILES
        llvm-passes/ReturnPropagation.cpp
        llvm-passes/ReturnPropagationPointer.cpp
        llvm-passes/ReturnedValues.cpp
        llvm-passes/ReturnConstraints.cpp
        llvm-passes/ReturnConstraintsPointer.cpp
        llvm-passes/ErrorBlocks.cpp
        llvm-passes/MissingChecks.cpp
        llvm-passes/DefinedFunctions.cpp
        llvm-passes/CalledFunctions.cpp
        llvm-passes/Common.cpp
        )

# This cannot be a shared library because LLVM uses globals for options.
# The MODULE target can be loaded by opt on the command line.
# The SHARED target is linked into main.
add_library(mypasses MODULE ${PASS_FILES})
add_library(eesillvm SHARED ${PASS_FILES})
#set_target_properties(mypasses PROPERTIES COMPILE_FLAGS -fno-exceptions)
#set_target_properties(eesillvm PROPERTIES COMPILE_FLAGS -fno-exceptions)

llvm_map_components_to_libnames(llvm_libs support core irreader analysis)
target_link_libraries(eesillvm ${llvm_libs} ${Boost_LIBRARIES} glog)

add_executable(eesi ${EESI_FILES})
add_dependencies(eesi eesillvm)
target_link_libraries(eesi eesillvm)
