
macro(hdospray_sign_target name)
  if (HDOSPRAY_SIGN_FILE)
    if (APPLE)
      # on OSX we strip manually before signing instead of setting CPACK_STRIP_FILES
      add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_STRIP} -x $<TARGET_FILE:${name}>
        COMMAND ${HDOSPRAY_SIGN_FILE} ${HDOSPRAY_SIGN_FILE_ARGS} $<TARGET_FILE:${name}>
        COMMENT "Stripping and signing target"
        VERBATIM
      )
    else()
      add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${HDOSPRAY_SIGN_FILE} ${HDOSPRAY_SIGN_FILE_ARGS} $<TARGET_FILE:${name}>
        COMMENT "Signing target"
        VERBATIM
      )
    endif()
  endif()
endmacro()