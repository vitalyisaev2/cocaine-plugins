LOCATE_LIBRARY(LIBCURL "curl/curl.h" "curl")

INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/node/include
    ${LIBCURL_INCLUDE_DIRS})

LINK_DIRECTORIES(
    ${LIBCURL_LIBRARY_DIRS})

ADD_LIBRARY(docker MODULE
    http
    docker_client
    module
    isolate)

TARGET_LINK_LIBRARIES(docker
    cocaine-core
    ${Boost_LIBRARIES}
    curl)

SET_TARGET_PROPERTIES(docker PROPERTIES
    PREFIX ""
    SUFFIX "${COCAINE_PLUGIN_SUFFIX}"
    COMPILE_FLAGS "-std=c++0x")

INSTALL(
    TARGETS docker
    LIBRARY DESTINATION lib/cocaine
    COMPONENT runtime)
