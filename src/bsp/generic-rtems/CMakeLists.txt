######################################################################
#
# CMAKE build recipe for generic-rtems Board Support Package (BSP)
#
######################################################################

add_library(osal_generic-rtems_impl OBJECT
	src/bsp_start.c
	src/bsp_console.c
)

# This BSP only works with "rtems" OS layer.
# Confirming this reduces risk of accidental misconfiguration
set(OSAL_EXPECTED_OSTYPE    "rtems" PARENT_SCOPE)
