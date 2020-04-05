cmake_minimum_required (VERSION 2.8.11)
project (gamerzilla)

set(GAMERZILLA_VERSION_MAJOR 0)
set(GAMERZILLA_VERSION_MINOR 1)
set(GAMERZILLA_VERSION_PATCH 0)
set(GAMERZILLA_VERSION_STRING ${GAMERZILLA_VERSION_MAJOR}.${GAMERZILLA_VERSION_MINOR}.${GAMERZILLA_VERSION_PATCH})

add_library (gamerzilla SHARED gamerzilla.c)
set_target_properties(gamerzilla PROPERTIES VERSION ${GAMERZILLA_VERSION_STRING} SOVERSION ${GAMERZILLA_VERSION_MAJOR})

target_include_directories (gamerzilla PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable (gamerzillatest gamerzillatest.c)
target_link_libraries (gamerzillatest LINK_PUBLIC gamerzilla)

INCLUDE(FindPkgConfig)

PKG_SEARCH_MODULE(CURL REQUIRED libcurl)

INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${CURL_LIBRARIES})


set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}" CACHE PATH "Installation directory for libraries")
set(INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gamerzilla.pc.cmakein
		${CMAKE_CURRENT_BINARY_DIR}/gamerzilla.pc @ONLY)

install(TARGETS ${PROJECT_NAME} DESTINATION "lib${LIB_SUFFIX}")

# Install library headers
file(GLOB HEADERS *.h)
install(FILES ${HEADERS} DESTINATION include/gamerzilla)

install(FILES gamerzilla.pc DESTINATION "lib${LIB_SUFFIX}/pkgconfig")
