idf_component_register(
  SRCS "main.cpp"
  INCLUDE_DIRS "."
)

target_include_directories(${COMPONENT_LIB} PRIVATE
  ${CMAKE_SOURCE_DIR}/../../include
  ${CMAKE_SOURCE_DIR}/../../src
  ${CMAKE_SOURCE_DIR}/../../adapters
)

target_sources(${COMPONENT_LIB} PRIVATE
  ${CMAKE_SOURCE_DIR}/../../src/FaceRecognizeESP.cpp
  ${CMAKE_SOURCE_DIR}/../../src/FeatureExtractor.cpp
  ${CMAKE_SOURCE_DIR}/../../src/FaceDatabase.cpp
  ${CMAKE_SOURCE_DIR}/../../src/RecognitionEngine.cpp
  ${CMAKE_SOURCE_DIR}/../../src/Logger.cpp
  ${CMAKE_SOURCE_DIR}/../../src/ImageUtils.cpp
  ${CMAKE_SOURCE_DIR}/../../src/Utils.cpp
  ${CMAKE_SOURCE_DIR}/../../adapters/StorageAdapter.cpp
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE USE_ESPIDF=1)