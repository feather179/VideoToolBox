add_rules("mode.debug", "mode.release")

target("VideoToolBox")
    set_kind("binary")
    set_toolchains("msvc")
    set_languages("c++20")
    add_cxxflags("/Zc:__cplusplus")
    add_cxxflags("/await")

    add_rules("qt.widgetapp")
    add_frameworks("QtOpenGL", "QtOpenGLWidgets")

    add_files("main.cpp")
    add_files("ui/*")
    add_files("vp/*.cpp")
    add_files("vr/*.cpp")
    add_files("foundation/*.cpp")
    add_files("rtsp/client/*.cpp")
    add_files("rtsp/server/*.cpp")

    add_headerfiles("ui/*.h", "vp/*.h", "vr/*.h", "foundation/*.h", "rtsp/client/*.h", "rtsp/server/*.h")

    add_includedirs(".")
    add_includedirs("E:/ffmpeg/SDL2-devel-2.28.4-VC/include")
    add_includedirs("D:/msys64/usr/local/include")
    add_includedirs("E:/ffmpeg/libyuv/out/install/x64-Release/include")

    add_linkdirs("E:/ffmpeg/SDL2-devel-2.28.4-VC/lib/x64")
    add_linkdirs("D:/msys64/usr/local/bin")
    add_linkdirs("E:/ffmpeg/libyuv/out/install/x64-Release/lib")

    add_links("avdevice", "avutil", "avcodec", "avformat", "swresample", "swscale")
    add_links("SDL2")
    add_links("OleAut32")
    add_links("d3d11", "d3dcompiler")
    add_links("yuv")