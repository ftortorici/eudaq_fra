include_directories(include)
#include_directories(/usr/include/libusb-1.0)
include_directories(/usr/local/include/libusb-1.0)
aux_source_directory(src USER_HARDWARE_SRC)

#link_directories(/usr/lib/x86_64-linux-gnu)


if(USER_FERS_BUILD)
  add_library(libusb SHARED IMPORTED)
  add_library(${USER_HARDWARE_LIBRARY} STATIC ${USER_HARDWARE_SRC})
  #set_target_properties( libusb PROPERTIES IMPORTED_LOCATION /usr/lib/x86_64-linux-gnu/libusb-1.0.so)
  set_target_properties( libusb PROPERTIES IMPORTED_LOCATION /usr//local/lib/libusb-1.0.dylib)
  target_link_libraries(${USER_HARDWARE_LIBRARY} ${EUDAQ_CORE_LIBRARY} ${USER_HARDWARE_DEP_LIB} libusb)
  #list(APPEND INSTALL_TARGETS ${USER_HARDWARE_LIBRARY})
endif()


if(USER_FERS_BUILD)
  list(APPEND USER_HARDWARE_SRC
    src/FERSutils.cc
    src/FERS_LLeth.cc
    src/FERS_LLtdl.cc
    src/FERS_LLusb.cpp
    src/FERS_readout.cc
    src/console.cc
    src/MultiPlatform.cc
    src/FERSlib.cc)
endif()

install(TARGETS ${USER_HARDWARE_LIBRARY}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
