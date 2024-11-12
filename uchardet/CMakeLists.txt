######## Project settings
cmake_minimum_required(VERSION 3.1)
include(CheckCCompilerFlag)
set (PACKAGE_NAME uchardet)
project (${PACKAGE_NAME} CXX C)

######## Subdirectories


add_subdirectory(src)
#add_subdirectory(doc)
#add_subdirectory(test)

######## Exported targets


