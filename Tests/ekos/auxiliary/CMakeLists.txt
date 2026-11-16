add_subdirectory(darkprocessor)
add_subdirectory(fieldderotation)

# Failure diagnosis types and runner
add_subdirectory(diagnosis)

# Module settings and metadata per optical train
add_subdirectory(opticaltrain)

# Plate solving profiles and solver defaults
if (StellarSolver_FOUND AND CFITSIO_FOUND)
    add_subdirectory(solver)
endif ()

# The star shapes of a frame
if (CFITSIO_FOUND)
    add_subdirectory(imageanalysis)
endif ()
