add_subdirectory(darkprocessor)

# Plate solving profiles and solver defaults
if (StellarSolver_FOUND AND CFITSIO_FOUND)
    add_subdirectory(solver)
endif ()
