if(TARGET mfem)
  message(STATUS "MFEM target already present; skipping bundled extern/mfem")
else()
  message(STATUS "MFEM target not found; configuring bundled extern/mfem")

  # These only matter when building bundled MFEM.
  # Don't FORCE unless you truly want to override user choices.
  if(NOT DEFINED MFEM_USE_METIS_5)
    set(MFEM_USE_METIS_5 OFF CACHE BOOL "Enable METIS 5")
  endif()

  if(NOT DEFINED MFEM_USE_SUITESPARSE)
    set(MFEM_USE_SUITESPARSE OFF CACHE BOOL "Use SuiteSparse")
  endif()

  if(NOT DEFINED MFEM_USE_MPI)
    set(MFEM_USE_MPI OFF CACHE BOOL "Enable MPI")
  endif()

  message(STATUS
    "Bundled MFEM configuration: "
    "MFEM_USE_MPI=${MFEM_USE_MPI}, "
    "MFEM_USE_SUITESPARSE=${MFEM_USE_SUITESPARSE}, "
    "MFEM_USE_METIS_5=${MFEM_USE_METIS_5}"
  )

  add_subdirectory(mfem)
endif()