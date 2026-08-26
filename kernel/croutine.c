/*
 * FlintRTOS - Co-routines (manifest 2, legacy extension). MISRA C:2023.
 * An older, optional feature for extremely RAM-constrained parts: cooperative
 * co-routines sharing one stack. Off by default (configUSE_CO_ROUTINES == 0);
 * modern designs use tasks. Compiles to nothing unless enabled.
 */
#include "FlintRTOS.h"

#if (configUSE_CO_ROUTINES == 1)

#include "list.h"

/* TODO: co-routine control blocks + ready/delayed co-routine lists.
   Retained for API completeness; new code should prefer tasks. */

#endif /* configUSE_CO_ROUTINES */
