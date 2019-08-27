cmake_minimum_required(VERSION 3.1)

project(signatue)
set(CMAKE_CXX_STANDARD 14)

FIND_PACKAGE(Boost COMPONENTS
    program_options
    thread
    REQUIRED
)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})

add_executable(${PROJECT_NAME}
    main.cpp
    utils.cpp
    signature.cpp
    md5.cpp
)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} LINK_PUBLIC ${Boost_LIBRARIES})
