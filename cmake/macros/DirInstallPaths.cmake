# Directory install paths.
# Files are installed in different directories depending
# on platform, e.g. Unix-style for Linux and most other
# Unix systems.

IF(UNIX AND NOT APPLE)
	# Unix-style install paths.
	SET(DIR_INSTALL_DATA "share/mcrecover/data")
	SET(DIR_INSTALL_EXE "bin")
	SET(DIR_INSTALL_DLL "lib")
	SET(DIR_INSTALL_LIB "lib")
ELSEIF(APPLE)
	# Mac OS X-style install paths.
	# Install should be relative to the application bundle.
	# (TODO)
ELSEIF(WIN32)
	# Win32-style install paths.
	# Files are installed relative to root, since the
	# program is run out of its own directory.
	SET(DIR_INSTALL_DATA "data")
	SET(DIR_INSTALL_EXE ".")
	SET(DIR_INSTALL_DLL ".")
	SET(DIR_INSTALL_LIB ".")
ENDIF()
