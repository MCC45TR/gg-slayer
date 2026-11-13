configure_file(kcm_slayerctl.json.in kcm_slayerctl.json @ONLY)
# KF6's convenience macro requires source-tree metadata. Generate the desktop
# entry from the configured metadata in the build tree instead.
kcmutils_add_qml_kcm(kcm_slayerctl SOURCES module.cpp module.hpp DISABLE_DESKTOP_FILE_GENERATION)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kcm_slayerctl.desktop
  COMMAND KF6::kcmdesktopfilegenerator ${CMAKE_CURRENT_BINARY_DIR}/kcm_slayerctl.json ${CMAKE_CURRENT_BINARY_DIR}/kcm_slayerctl.desktop
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kcm_slayerctl.json VERBATIM)
add_custom_target(kcm_slayerctl-kcm-desktop-gen DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kcm_slayerctl.desktop)
add_dependencies(kcm_slayerctl kcm_slayerctl-kcm-desktop-gen)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kcm_slayerctl.desktop DESTINATION ${KDE_INSTALL_APPDIR})
target_include_directories(kcm_slayerctl PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(kcm_slayerctl PRIVATE slayer-backend Qt6::Core Qt6::Quick Qt6::Qml KF6::CoreAddons KF6::KCMUtilsQuick)
target_compile_definitions(kcm_slayerctl PRIVATE SLAYER_CLI="${CMAKE_INSTALL_FULL_BINDIR}/slayerctl" SLAYER_HELPER="${CMAKE_INSTALL_FULL_LIBEXECDIR}/slayerctl-helper")
target_compile_options(kcm_slayerctl PRIVATE -Wall -Wextra -fstack-protector-strong)
target_link_options(kcm_slayerctl PRIVATE -Wl,-z,relro,-z,now)

set_property(TARGET kcm_slayerctl PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
target_link_options(kcm_slayerctl PRIVATE $<$<CONFIG:MinSizeRel>:-s>)
