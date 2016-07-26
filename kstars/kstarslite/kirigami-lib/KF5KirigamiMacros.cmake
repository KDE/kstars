
include(CMakeParseArguments)
include(ExternalProject)
find_package(Git)


function(kirigami_package_breeze_icons)
    set(_multiValueArgs ICONS)
    cmake_parse_arguments(ARG "" "" "${_multiValueArgs}" ${ARGN} )

    if(NOT ARG_ICONS)
        message(FATAL_ERROR "No ICONS argument given to kirigami_package_breeze_icons")
    endif()

    #include icons used by Kirigami components themselves
    set(ARG_ICONS ${ARG_ICONS} go-next go-previous handle-left handle-right)

    function(_find_breeze_icon icon varName)
        #HACKY
        SET(path "")
        file(GLOB_RECURSE path ${_BREEZEICONS_DIR}/icons/*/48/${icon}.svg )

        #seach in other sizes as well
        if (NOT EXISTS ${path})
             file(GLOB_RECURSE path ${_BREEZEICONS_DIR}/icons/*/32/${icon}.svg )
            if (NOT EXISTS ${path})
                file(GLOB_RECURSE path ${_BREEZEICONS_DIR}/icons/*/24/${icon}.svg )
                if (NOT EXISTS ${path})
                    file(GLOB_RECURSE path ${_BREEZEICONS_DIR}/icons/*/22/${icon}.svg )
                endif()
            endif()
        endif()
        if (NOT EXISTS ${path})
            return()
        endif()

        get_filename_component(path "${path}" REALPATH)

        SET(${varName} ${path} PARENT_SCOPE)
    endfunction()

    if (BREEZEICONS_DIR AND NOT EXISTS ${BREEZEICONS_DIR})
        message(FATAL_ERROR "BREEZEICONS_DIR variable does not point to existing dir: \"${BREEZEICONS_DIR}\"")
    endif()

    set(_BREEZEICONS_DIR "${BREEZEICONS_DIR}")

    #FIXME: this is a terrible hack
    if(NOT _BREEZEICONS_DIR)
        set(_BREEZEICONS_DIR "${CMAKE_BINARY_DIR}/breeze-icons/src/breeze-icons")

        # replacement for ExternalProject_Add not yet working
        # first time config?
        if (NOT EXISTS ${_BREEZEICONS_DIR})
            execute_process(COMMAND ${GIT_EXECUTABLE} clone --depth 1 git://anongit.kde.org/breeze-icons.git ${_BREEZEICONS_DIR})
        endif()

        # external projects are only pulled at make time, not configure time
        # so this is too late to work with the _find_breeze_icon() method
        # _find_breeze_icon() would need to be turned into a target/command
        if (FALSE)
        ExternalProject_Add(
            breeze-icons
            PREFIX breeze-icons
            GIT_REPOSITORY git://anongit.kde.org/breeze-icons.git
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            LOG_DOWNLOAD ON
        )
        endif()
    endif()

    message (STATUS "Found external breeze icons:")
    foreach(_iconName ${ARG_ICONS})
        set(_iconPath "")
        _find_breeze_icon(${_iconName} _iconPath)
        message (STATUS ${_iconPath})
        if (EXISTS ${_iconPath})
            install(FILES ${_iconPath} DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kirigami/icons/ RENAME ${_iconName}.svg)
        endif()

    endforeach()
endfunction()

