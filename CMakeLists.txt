cmake_minimum_required(VERSION 3.8)
project(webserver)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
    badrequest.html
    brum.html
    client
    client.c
    end.html
    get_listen_socket.c
    get_listen_socket.h
    get_listen_socket.o
    index.html
    main.c
    main.o
    make_printable_address.c
    make_printable_address.h
    make_printable_address.o
    Makefile
    multi_thread_server
    notfound.html
    profile.html
    random.html
    README
    service_client_socket.c
    service_client_socket.h
    service_client_socket.o
    service_listen_socket.c
    service_listen_socket.h
    service_listen_socket.o
    service_listen_socket_multithread.c
    service_listen_socket_multithread.h
    service_listen_socket_multithread.o
    single_thread_server
    TODO
    uni.html
    upload.html)

add_executable(webserver ${SOURCE_FILES})