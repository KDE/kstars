# Tests/ekos — Ekos subsystem tests
#
# This directory mirrors kstars/ekos/ — each subdirectory tests one module.
# Conditions below mirror those in Tests/CMakeLists.txt.

# ── Always built ────────────────────────────────────────────────────────────
# These subdirectories guard themselves internally with IF(INDI_FOUND) etc.
add_subdirectory(auxiliary)
add_subdirectory(observatory)
add_subdirectory(analyze)
add_subdirectory(mount)
add_subdirectory(capture)
add_subdirectory(mcp)

# ── Require CFITSIO + INDI ───────────────────────────────────────────────────
IF (CFITSIO_FOUND AND INDI_FOUND)
    add_subdirectory(scheduler)
    add_subdirectory(focus)
    add_subdirectory(align)

    # FIXME: guide tests crash on EGLFS / Windows — disable until root-cause found
    IF (NOT WIN32)
        add_subdirectory(guide)
    ENDIF()
ENDIF()
