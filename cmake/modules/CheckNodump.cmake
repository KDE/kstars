# Check if the linker supports -Wl,z,nodump

function(check_nodump result)
  set(NODUMP_FLAGS "-Wl,-z,nodump")
  cmake_policy(SET CMP0056 NEW)
  set(CMAKE_EXE_LINKER_FLAGS "${NODUMP_FLAGS}")
  set(TEST_PROGRAM "int main() { return 0 ; }")
  set(TEST_FILE "${CMAKE_CURRENT_BINARY_DIR}/test_nodump.cc")
  file(WRITE "${TEST_FILE}" "${TEST_PROGRAM}")

  message(STATUS "Checking whether the linker supports ${NODUMP_FLAGS} ...")
  try_compile(SUPPORT_NODUMP
              "${CMAKE_CURRENT_BINARY_DIR}/try_has_nodump"
              "${TEST_FILE}"
  )
  message(STATUS "  supports ${NODUMP_FLAGS}: ${SUPPORT_NODUMP}")

  if(SUPPORT_NODUMP)
    set(${result} "${NODUMP_FLAGS}")
  else()
    set(${result} "")
  endif()
endfunction()
