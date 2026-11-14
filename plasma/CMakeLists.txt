qt_policy(SET QTP0001 NEW)
qt_add_qml_module(slayer-camera SHARED
  URI org.gamegaraj.slayerctl.camera
  VERSION 1.0
  OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/org/gamegaraj/slayerctl/camera
  SOURCES camera_control.cpp camera_control.hpp)
target_link_libraries(slayer-camera PRIVATE Qt6::Core Qt6::Qml Qt6::DBus PkgConfig::SLAYER_SYSTEMD)
target_compile_definitions(slayer-camera PRIVATE SLAYER_CLI="${CMAKE_INSTALL_FULL_BINDIR}/slayerctl" _FORTIFY_SOURCE=3)
target_compile_options(slayer-camera PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fstack-protector-strong)
target_link_options(slayer-camera PRIVATE -Wl,-z,relro,-z,now)
set_target_properties(slayer-camera slayer-cameraplugin PROPERTIES POSITION_INDEPENDENT_CODE ON INSTALL_RPATH "$ORIGIN")
# KDE removes the prefix from applet/KCM plugins; the QML loader expects the
# platform's shared-library prefix for QML extension plugins.
set_target_properties(slayer-cameraplugin PROPERTIES PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}")
set(camera_import ${KDE_INSTALL_QMLDIR}/org/gamegaraj/slayerctl/camera)
install(TARGETS slayer-camera slayer-cameraplugin LIBRARY DESTINATION ${camera_import})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org/gamegaraj/slayerctl/camera/qmldir
              ${CMAKE_CURRENT_BINARY_DIR}/org/gamegaraj/slayerctl/camera/slayer-camera.qmltypes
        DESTINATION ${camera_import})
install(DIRECTORY org.gamegaraj.slayerctl.camera DESTINATION ${KDE_INSTALL_DATADIR}/plasma/plasmoids)

add_executable(camera-check ../tests/camera.cpp camera_control.cpp camera_control.hpp)
target_link_libraries(camera-check PRIVATE Qt6::Core Qt6::Qml Qt6::DBus PkgConfig::SLAYER_SYSTEMD)
add_test(NAME native-camera-control COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:camera-check> ${CMAKE_SOURCE_DIR}/tests/camera_reader.py)
set_tests_properties(native-camera-control PROPERTIES TIMEOUT 15)
add_executable(camera-module-check ../tests/camera_module.cpp)
target_link_libraries(camera-module-check PRIVATE Qt6::Core Qt6::Qml)
add_test(NAME native-camera-qml-module COMMAND camera-module-check ${CMAKE_CURRENT_BINARY_DIR})
find_program(SLAYER_QML_TEST_RUNNER NAMES qmltestrunner6 qmltestrunner HINTS ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS} /usr/lib64/qt6/bin /usr/lib/qt6/bin REQUIRED)
add_test(NAME native-camera-battery-ui COMMAND ${SLAYER_QML_TEST_RUNNER} -input ${CMAKE_SOURCE_DIR}/tests/qml)
set_tests_properties(native-camera-battery-ui PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FORCE_STDERR_LOGGING=1")
