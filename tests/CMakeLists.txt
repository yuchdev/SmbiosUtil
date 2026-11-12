add_subdirectory(integration)
add_subdirectory(perf)
add_subdirectory(unit)
add_subdirectory(mock)
add_subdirectory(c_abi)

# Milestone 0011, Story 04.0: the GUI test tiers (gui-ci always, gui-local
# only outside CI) - see tests/gui/CMakeLists.txt. A non-GUI build adds
# no target and no test.
if(SMBIOS_BUILD_GUI)
    add_subdirectory(gui)
endif()
