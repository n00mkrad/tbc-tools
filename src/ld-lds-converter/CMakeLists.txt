set(_ld_lds_converter_flac_target "")

find_package(FLAC 1.5 QUIET CONFIG)
if(TARGET FLAC::FLAC)
    set(_ld_lds_converter_flac_target FLAC::FLAC)
elseif(TARGET FLAC::libFLAC)
    set(_ld_lds_converter_flac_target FLAC::libFLAC)
endif()

if(NOT _ld_lds_converter_flac_target)
    pkg_check_modules(FLAC_PC REQUIRED IMPORTED_TARGET "flac>=1.5.0")
    set(_ld_lds_converter_flac_target PkgConfig::FLAC_PC)
endif()
add_executable(ld-lds-converter
    converterdialog.cpp
    converterdialog.ui
    dataconverter.cpp
    main.cpp
)
target_link_libraries(ld-lds-converter PRIVATE
    Qt::Core Qt::Gui Qt::Widgets
    lddecode-library
    ${_ld_lds_converter_flac_target}
)

install(TARGETS ld-lds-converter)
