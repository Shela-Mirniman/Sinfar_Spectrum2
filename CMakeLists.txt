cmake_minimum_required(VERSION 2.8.11)

  
FILE(GLOB SRC *.c *.cpp)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=1 ")

ADD_EXECUTABLE(spectrum2_sinfar_backend ${SRC})
#
#if (CMAKE_COMPILER_IS_GNUCXX)
#if (NOT WIN32)
#target_link_libraries(spectrum2_template_backend transport pthread ${Boost_LIBRARIES} ${SWIFTEN_LIBRARY} ${LOG4CXX_LIBRARIES} Qt5::Widgets)
target_link_libraries(spectrum2_sinfar_backend pthread curl transport curlpp stdc++fs ${Boost_LIBRARIES} ${SWIFTEN_LIBRARY} ${LOG4CXX_LIBRARIES})


#else()
#target_link_libraries(spectrum2_template_backend transport ${Boost_LIBRARIES} ${SWIFTEN_LIBRARY} ${LOG4CXX_LIBRARIES})
#endif()
#else()
#target_link_libraries(spectrum2_template_backend transport ${PROTOBUF_LIBRARY} ${Boost_LIBRARIES} ${SWIFTEN_LIBRARY} ${LOG4CXX_LIBRARIES})
#endif()

INSTALL(TARGETS spectrum2_sinfar_backend RUNTIME DESTINATION bin)

