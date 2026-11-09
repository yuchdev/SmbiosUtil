set(TARGET smbios)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS program_options REQUIRED)

# include/smbios/command_line_parser.hpppp is this executable's own private
# header - it lives under this target's own include/smbios/ segment
# (renamed from include/smbios_util/), distinct from libsmbios's own
# public headers at libsmbios/include/libsmbios/.
file(GLOB SOURCES *.cpp ../include/smbios/*.h)

include_directories(${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/libsmbios/include
)

add_executable(${TARGET} ${SOURCES})

target_compile_options(${TARGET} PRIVATE ${SMBIOS_COMPILE_OPTIONS} ${SMBIOS_WARNING_OPTIONS})
target_link_options(${TARGET} PRIVATE ${SMBIOS_LINK_OPTIONS})

# Force every object file in libsmbios's static archive into this
# executable. Each concrete entry class (BiosInformationEntry,
# SystemSlotsEntry, ...) registers itself with SMBiosEntryFactory only via
# REGISTER_SMBIOS_ENTRY's static initializer in its own .cpp, and nothing in
# this CLI names any concrete entry class - so without this, the linker
# never extracts those objects from libsmbios.a, their initializers never
# run, and SMBiosEntryFactory::create() returns nullptr for every structure
# type. Scoped to this target only: libsmbios itself is just an archive, and
# the tests/* targets already pull the objects in by constructing the
# concrete classes directly.
#
# Branches on the *linker* family rather than the compiler, since the root
# CMakeLists.txt's "Clang" branch also covers Clang on Linux, which uses a
# GNU-compatible linker (ld/gold/lld), not Apple's ld64. Each form is
# position-independent, so passing it as a link option (emitted before the
# objects and libraries) is safe: whole-archive members are loaded
# unconditionally, and their own dependencies (SMBIOS_LEGACY_LIB /
# SMBIOS_PLATFORM_LIB) still resolve from the libraries listed afterwards.
if(APPLE)
    target_link_options(${TARGET} PRIVATE "LINKER:-force_load,$<TARGET_FILE:libsmbios>")
elseif(MSVC)
    target_link_options(${TARGET} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:libsmbios>")
else()
    target_link_options(${TARGET} PRIVATE
        "LINKER:--whole-archive,$<TARGET_FILE:libsmbios>,--no-whole-archive")
endif()

target_link_libraries(${TARGET}
    ${Boost_LIBRARIES}
    libsmbios)

add_dependencies(${TARGET} libsmbios)
