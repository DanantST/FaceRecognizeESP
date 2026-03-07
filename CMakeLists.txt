cmake_minimum_required(VERSION 3.20)
project(FaceRecognizeESP VERSION 1.0.0 LANGUAGES CXX)

option(USE_ESPIDF "Build with ESP-IDF specific defines" OFF)
option(USE_ARDUINO "Build with Arduino specific defines" OFF)
option(BUILD_TESTING "Build unit and integration tests" ON)

set(FACERECOGNIZE_SOURCES
  src/FaceRecognizeESP.cpp
  src/FeatureExtractor.cpp
  src/FaceDatabase.cpp
  src/RecognitionEngine.cpp
  src/Logger.cpp
  src/ImageUtils.cpp
  src/Utils.cpp
  adapters/StorageAdapter.cpp
)

add_library(FaceRecognizeESP STATIC ${FACERECOGNIZE_SOURCES})
add_library(FaceRecognizeESP::FaceRecognizeESP ALIAS FaceRecognizeESP)

target_compile_features(FaceRecognizeESP PUBLIC cxx_std_17)

target_include_directories(FaceRecognizeESP
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters
)

if(USE_ESPIDF)
  target_compile_definitions(FaceRecognizeESP PUBLIC USE_ESPIDF=1)
endif()

if(USE_ARDUINO)
  target_compile_definitions(FaceRecognizeESP PUBLIC USE_ARDUINO=1)
endif()

if(MSVC)
  target_compile_options(FaceRecognizeESP PRIVATE /W4)
else()
  target_compile_options(FaceRecognizeESP PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)
target_link_libraries(FaceRecognizeESP PUBLIC Threads::Threads)

if(BUILD_TESTING)
  include(CTest)
  enable_testing()

  add_executable(unit_lbp tests/unit/test_lbp.cpp)
  target_link_libraries(unit_lbp PRIVATE FaceRecognizeESP)
  add_test(NAME unit_lbp COMMAND unit_lbp)

  add_executable(unit_pca tests/unit/test_pca_projection.cpp)
  target_link_libraries(unit_pca PRIVATE FaceRecognizeESP)
  add_test(NAME unit_pca COMMAND unit_pca)

  add_executable(unit_db tests/unit/test_db_io.cpp)
  target_link_libraries(unit_db PRIVATE FaceRecognizeESP)
  add_test(NAME unit_db COMMAND unit_db)

  add_executable(integration_cycle tests/integration/test_enroll_recognize_cycle.cpp)
  target_link_libraries(integration_cycle PRIVATE FaceRecognizeESP)
  add_test(NAME integration_cycle COMMAND integration_cycle)
endif()