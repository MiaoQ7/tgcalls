include(cmake/external_ffmpeg.cmake)
include(cmake/lib_tgcalls.cmake)

configure_file("${src_loc}/config.h.in" "${src_loc}/config.h")

list(APPEND SOURCES
    ${src_loc}/config.h
    ${src_loc}/tgcalls.cpp
    ${src_loc}/WrappedAudioDeviceModuleImpl.h
    ${src_loc}/WrappedAudioDeviceModuleImpl.cpp
    ${src_loc}/FileAudioDevice.cpp
    ${src_loc}/FileAudioDevice.h
    ${src_loc}/P2PFileAudioDevice.h
    ${src_loc}/P2PFileAudioDevice.cpp
    ${src_loc}/P2PFileAudioDeviceDescriptor.h
    ${src_loc}/RawAudioDevice.h
    ${src_loc}/RawAudioDevice.cpp
    ${src_loc}/FileAudioDeviceDescriptor.h
    ${src_loc}/RawAudioDeviceDescriptor.h
    ${src_loc}/RawAudioDeviceDescriptor.cpp
    ${src_loc}/P2PRawAudioDevice.h
    ${src_loc}/P2PRawAudioDevice.cpp
    ${src_loc}/P2PRawAudioDeviceDescriptor.h
    ${src_loc}/P2PRawAudioDeviceDescriptor.cpp
    ${src_loc}/NativeInstance.h
    ${src_loc}/NativeInstance.cpp
    ${src_loc}/RtcServer.h
    ${src_loc}/RtcServer.cpp
    ${src_loc}/InstanceHolder.h
    ${src_loc}/video/PythonVideoTrackSource.h
    ${src_loc}/video/PythonVideoTrackSource.cpp
    ${src_loc}/video/PythonSource.h
    ${src_loc}/video/PythonSource.cpp
    ${src_loc}/video/PythonRecord.h
    ${src_loc}/video/PythonRecord.cpp
    ${src_loc}/video/PythonSourceYUV.h
    ${src_loc}/video/PythonSourceYUV.cpp
    ${src_loc}/video/PythonPureSource.h
    ${src_loc}/video/PythonPureSource.cpp
)

pybind11_add_module(tgcalls ${SOURCES})

if (WIN32)
  target_compile_features(tgcalls
  PUBLIC
      cxx_std_20
  )
endif()

target_link_libraries(tgcalls
PRIVATE
    lib_tgcalls
    external_ffmpeg
    external_webrtc
)
