######################################################################
#
# CMAKE build recipe for generic-rtems Board Support Package (BSP)
#
######################################################################

add_library(osal_generic-rtems_impl OBJECT
	src/bsp_start.c
	src/bsp_console.c
)

# This definition is needed for the MSG_DONTWAIT constant
# By defining this, it avoids the need to use the -std=gnu99
#  instead of the preferred -std=c99 GCC switch
target_compile_definitions(osal_generic-rtems_impl PUBLIC
   _BSD_SOURCE
)

# This BSP only works with "rtems" OS layer.
# Confirming this reduces risk of accidental misconfiguration
set(OSAL_EXPECTED_OSTYPE    "rtems" PARENT_SCOPE)
