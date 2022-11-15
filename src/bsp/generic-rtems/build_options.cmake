##########################################################################
#
# Build options for "generic-rtems" BSP
#
##########################################################################
# The "-u" switch is required to ensure that the linker pulls in the OS_BSPMain entry point
target_link_libraries(osal_bsp -uOS_BSPMain)

