cmake_minimum_required(VERSION 2.8.3)
project(associative_memory_ros)

add_definitions(-std=c++14)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  playground_builder
  roscpp
)

## System dependencies are found with CMake's conventions

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SDL REQUIRED)
find_package(SDL_image REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)

pkg_search_module(FTGL REQUIRED ftgl)
pkg_search_module(JSONCPP REQUIRED jsoncpp)
pkg_search_module(AssociativeMemory REQUIRED associative-memory)

link_directories(${AssociativeMemory_LIBRARY_DIRS})


###################################
## catkin specific configuration ##
###################################
## The catkin_package macro generates cmake config files for your package
## Declare things to be passed to dependent projects
## INCLUDE_DIRS: uncomment this if you package contains header files
## LIBRARIES: libraries you create in this project that dependent projects also need
## CATKIN_DEPENDS: catkin_packages dependent projects also need
## DEPENDS: system dependencies of this project that dependent projects also need
catkin_package(
#  INCLUDE_DIRS include
#  LIBRARIES associative_memory_ros
#  CATKIN_DEPENDS playground_builder roscpp
#  DEPENDS AssociativeMemory
)

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
# include_directories(include)
# TODO: Check names of system library include directories (AssociativeMemory)
message(STATUS "includedir: ${AssociativeMemory_INCLUDE_DIRS}")
include_directories(
    ${catkin_INCLUDE_DIRS}
    ${AssociativeMemory_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    ${SDL_INCLUDE_DIR}
    ${SDL_IMAGE_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${FTGL_INCLUDE_DIRS} 
    ${JSONCPP_INCLUDE_DIRS})

file(GLOB_RECURSE SRC src/*.cpp)

## Declare a C++ library
# add_library(associative_memory_ros
#   src/${PROJECT_NAME}/associative_memory_ros.cpp
# )

## Add cmake target dependencies of the library
## as an example, code may need to be generated before libraries
## either from message generation or dynamic reconfigure
# add_dependencies(associative_memory_ros ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})

## Declare a C++ executable
add_executable(associative_memory_ros_node ${SRC})

## Add cmake target dependencies of the executable
## same as for the library above
add_dependencies(associative_memory_ros_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})


## Specify libraries to link a library or executable target against
target_link_libraries(associative_memory_ros_node
   ${catkin_LIBRARIES}
   ${AssociativeMemory_LIBRARIES}
   ${OPENGL_LIBRARIES} 
   ${SDL_LIBRARY} 
   ${SDL_IMAGE_LIBRARIES} 
   ${Boost_LIBRARIES} 
   ${FTGL_LIBRARIES}
   ${JSONCPP_LIBRARIES}
)

#############
## Install ##
#############

# all install targets should use catkin DESTINATION variables
# See http://ros.org/doc/api/catkin/html/adv_user_guide/variables.html

## Mark executable scripts (Python etc.) for installation
## in contrast to setup.py, you can choose the destination
# install(PROGRAMS
#   scripts/my_python_script
#   DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
# )

# Mark executables and/or libraries for installation
install(TARGETS associative_memory_ros_node
ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

## Mark cpp header files for installation
# install(DIRECTORY include/${PROJECT_NAME}/
#   DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
#   FILES_MATCHING PATTERN "*.h"
#   PATTERN ".svn" EXCLUDE
# )

## Mark other files for installation (e.g. launch and bag files, etc.)
# install(FILES
#   # myfile1
#   # myfile2
#   DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
# )

#############
## Testing ##
#############

## Add gtest based cpp test target and link libraries
# catkin_add_gtest(${PROJECT_NAME}-test test/test_associative_memory_ros.cpp)
# if(TARGET ${PROJECT_NAME}-test)
#   target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})
# endif()

## Add folders to be run by python nosetests
# catkin_add_nosetests(test)
