cmake_minimum_required(VERSION 3.5)

project(AndroidAutoQt LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Qt5 COMPONENTS Core Quick Multimedia MultimediaWidgets REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# Set up aasdk dependencies
find_package(Boost REQUIRED COMPONENTS system log)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)

# Show debugging info for include paths
message(STATUS "LIBUSB_INCLUDE_DIRS: ${LIBUSB_INCLUDE_DIRS}")
message(STATUS "BOOST_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "PROTOBUF_INCLUDE_DIRS: ${PROTOBUF_INCLUDE_DIRS}")

# Add aasdk paths
set(AASDK_INCLUDE_DIRS "/usr/local/include")
set(AASDK_LIBRARIES "/usr/local/lib/libaasdk.so")
set(AASDK_PROTO_LIBRARIES "/usr/local/lib/libaasdk_proto.so")

# Check for aasdk protobuf message files - add more potential locations
file(GLOB_RECURSE AASDK_PROTO_FILES 
    "/usr/local/include/aasdk/Proto/*.pb.h"
    "/usr/local/include/aasdk/Proto/*/*.pb.h"
    "/usr/local/include/aasdk/proto/*.pb.h"
    "/usr/local/include/aasdk/proto/*/*.pb.h"
    "/usr/local/include/aasdk_proto/*.pb.h"
    "/usr/local/include/aasdk_proto/*/*.pb.h"
    # Add additional search paths
    "/usr/include/aasdk/Proto/*.pb.h"
    "/usr/include/aasdk/proto/*.pb.h"
    "/usr/include/aasdk_proto/*.pb.h"
)

if(AASDK_PROTO_FILES)
    list(GET AASDK_PROTO_FILES 0 SAMPLE_PROTO_FILE)
    get_filename_component(PROTO_DIR ${SAMPLE_PROTO_FILE} DIRECTORY)
    message(STATUS "Found proto files in: ${PROTO_DIR}")
    get_filename_component(PROTO_PARENT_DIR ${PROTO_DIR} DIRECTORY)
    message(STATUS "Proto parent dir: ${PROTO_PARENT_DIR}")
else()
    message(WARNING "No protobuf message files found in aasdk include directories!")
    # Add fallback paths to check
    message(STATUS "Checking for aasdk installation...")
    execute_process(
        COMMAND find /usr -name "aasdk_proto" -type d
        OUTPUT_VARIABLE AASDK_PROTO_SEARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Additional proto locations: ${AASDK_PROTO_SEARCH}")
endif()

# Check if aasdk libraries exist
if(NOT EXISTS "${AASDK_LIBRARIES}")
    message(FATAL_ERROR "libaasdk.so not found at ${AASDK_LIBRARIES}. Please build and install aasdk first.")
endif()

if(NOT EXISTS "${AASDK_PROTO_LIBRARIES}")
    message(FATAL_ERROR "libaasdk_proto.so not found at ${AASDK_PROTO_LIBRARIES}. Please build and install aasdk first.")
endif()

# Create QML resource file
set(QML_RESOURCES
    qml.qrc
)

# List all source files
set(PROJECT_SOURCES
    main.cpp
    src/androidauto.cpp
    src/androidauto.h
    src/usbdetector.cpp
    src/usbdetector.h
    ${QML_RESOURCES}
)

# Create executable
add_executable(AndroidAutoQt ${PROJECT_SOURCES})

# Find where libusb.h actually is on this system - expand search paths
find_path(LIBUSB_HEADER_DIR libusb.h
    PATHS 
    /usr/include 
    /usr/local/include 
    /usr/include/libusb-1.0 
    /usr/local/include/libusb-1.0
    ${LIBUSB_INCLUDE_DIRS}
    DOC "Directory containing libusb.h")

if(NOT LIBUSB_HEADER_DIR)
    message(FATAL_ERROR "libusb.h not found. Please install libusb development package.")
endif()

message(STATUS "Found libusb.h in: ${LIBUSB_HEADER_DIR}")

# Include directories - add all possible locations for aasdk includes
target_include_directories(AndroidAutoQt
  PRIVATE
    ${LIBUSB_INCLUDE_DIRS}
    ${LIBUSB_HEADER_DIR}
    ${Boost_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${AASDK_INCLUDE_DIRS}
    /usr/local/include
    /usr/local/include/aasdk
    /usr/local/include/aasdk_proto
    /usr/include/aasdk
    /usr/include/aasdk_proto
    ${PROTO_PARENT_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries - order matters for resolving dependencies
target_link_libraries(AndroidAutoQt
  PRIVATE
    Qt5::Core
    Qt5::Quick
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    ${LIBUSB_LIBRARIES}
    ${Boost_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${AASDK_PROTO_LIBRARIES}
    ${AASDK_LIBRARIES}
    pthread  # Explicitly add pthread for thread support
)

# Define preprocessor macros
target_compile_definitions(AndroidAutoQt
  PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>
    USE_AASDK_DIRECT
)

# Install
install(TARGETS AndroidAutoQt
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
