# unlike CMakeLists.txt this file is include at cpack time, once per generator after CPack has set CPACK_GENERATOR
# to the actual generator being used. It allows per-generator setting of CPACK_* variables at cpack time.
set(GIT_DESCRIBE "${CPACK_GIT_DESCRIBE}")
set(GIT_COMMIT_DATE ${CPACK_GIT_COMMIT_DATE})
set(CMAKE_CURRENT_SOURCE_DIR "${CPACK_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(GitInfo)

if(NOT GIT_DESCRIBE)
  set(PACKAGE_VERSION "${CPACK_MIXXX_VERSION}-unknown")
else()
  set(PACKAGE_VERSION "${GIT_DESCRIBE}")
endif()
# Cap the package file name length. The git describe string (embedded in
# PACKAGE_VERSION) can grow very long on branches with many merge commits
# (--first-parent accumulates a -N-g<sha> segment per merge). CPack embeds
# this name in the WIX staging directory, and wixnative.exe does not honor
# LongPathsEnabled, so an over-long name pushes the deepest Qt file path
# past MAX_PATH (260) and the installer build fails (see KNOWN_ISSUES.md).
# Truncate the version used in the file name while keeping the leading tag
# and the trailing short sha, so the name stays informative but bounded.
set(CPACK_PACKAGE_FILE_NAME_MAX_LEN 60)
string(LENGTH "${PACKAGE_VERSION}" _pkg_ver_len)
if(_pkg_ver_len GREATER CPACK_PACKAGE_FILE_NAME_MAX_LEN)
  # Keep the first 40 chars (the version tag prefix) and the last 16 chars
  # (the short commit sha), joined by a marker.
  string(SUBSTRING "${PACKAGE_VERSION}" 0 40 _pkg_ver_head)
  math(EXPR _pkg_ver_tail_start "${_pkg_ver_len} - 16")
  string(SUBSTRING "${PACKAGE_VERSION}" ${_pkg_ver_tail_start} 16 _pkg_ver_tail)
  set(PACKAGE_VERSION "${_pkg_ver_head}...${_pkg_ver_tail}")
endif()
set(
  CPACK_PACKAGE_FILE_NAME
  "mixxx-${PACKAGE_VERSION}-${CPACK_SYSTEM_PROCESSOR}"
)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-source")

# The upstream version must not contain hyphen
# . for normal versioning + for advance and ~ for decline the version
# dpkg --compare-versions 2.3~alpha~1234~g8163 lt 2.3~beta~1234~g8163 && echo true
# dpkg --compare-versions 2.3~beta~1234~g8163 lt 2.3.0 && echo true
# dpkg --compare-versions 2.3.0 lt 2.3.0+2345+g163  && echo true
if(PACKAGE_VERSION MATCHES "^[0-9]+\\.[0-9]+[A-Za-z0-9.+~-]*$")
  if(PACKAGE_VERSION MATCHES "(alpha|beta)")
    string(REPLACE "-" "~" CPACK_DEBIAN_PACKAGE_VERSION "${PACKAGE_VERSION}")
  else()
    string(REPLACE "-g" "+g" CPACK_DEBIAN_PACKAGE_VERSION "${PACKAGE_VERSION}")
    string(
      REPLACE
      "-"
      "+r"
      CPACK_DEBIAN_PACKAGE_VERSION
      "${CPACK_DEBIAN_PACKAGE_VERSION}"
    )
  endif()
else()
  string(REPLACE "-" "~" CPACK_DEBIAN_PACKAGE_VERSION "${CPACK_MIXXX_VERSION}")
endif()

if(CPACK_GENERATOR STREQUAL "DEB")
  set(CPACK_INSTALL_SCRIPT ${CPACK_DEBIAN_INSTALL_SCRIPT})
endif()

if(CPACK_GENERATOR STREQUAL "External")
  if(DEB_SOURCEPKG OR DEB_UPLOAD_PPA OR DEB_BUILD)
    set(CPACK_EXTERNAL_ENABLE_STAGING true)
    set(CPACK_INSTALLED_DIRECTORIES "${CPACK_DEBIAN_SOURCE_DIR};/")
    set(CPACK_IGNORE_FILES "${CPACK_SOURCE_IGNORE_FILES}")
    set(CPACK_INSTALL_CMAKE_PROJECTS "")
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CPACK_DEBIAN_UPLOAD_PPA_SCRIPT}")
  endif()
endif()
