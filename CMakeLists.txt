cmake_minimum_required (VERSION 3.0.0)
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
add_executable (gamerzillaserver gamerzillaserver.c)
target_link_libraries (gamerzillaserver LINK_PUBLIC gamerzilla)

INCLUDE(FindPkgConfig)

PKG_SEARCH_MODULE(CURL REQUIRED libcurl)
PKG_SEARCH_MODULE(JANSSON REQUIRED jansson)

INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIRS} ${JANSSON_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})
if(WIN32)
	TARGET_LINK_LIBRARIES(${PROJECT_NAME} wsock32 ws2_32)
endif()

set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}" CACHE PATH "Installation directory for libraries")
set(INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gamerzilla.pc.cmakein
	${CMAKE_CURRENT_BINARY_DIR}/gamerzilla.pc @ONLY)

install(TARGETS ${PROJECT_NAME} DESTINATION "lib${LIB_SUFFIX}")

# Install library headers
file(GLOB HEADERS *.h)
install(FILES ${HEADERS} DESTINATION include/gamerzilla)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gamerzilla.pc DESTINATION "lib${LIB_SUFFIX}/pkgconfig")
if(NOT WIN32)
	install(FILES gamerzilla.vapi gamerzilla.deps DESTINATION share/vala/vapi)

	install(TARGETS gamerzillaserver DESTINATION "bin")
endif()
