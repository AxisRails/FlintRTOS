/*
 * FlintRTOS - kernel status / error codes (K7.3).
 * TCB code (MISRA C:2023).
 *
 * Every kernel operation returns one of these. On any error the operation has
 * no observable side effects (K1.3 atomicity, fail-closed).
 */
#ifndef FLINT_STATUS_H
#define FLINT_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum flint_status
{
    FLINT_OK                = 0,  /* success                                    */
    FLINT_ERR_CAP_INVALID   = 1,  /* cptr out of range or names an empty slot   */
    FLINT_ERR_CAP_TYPE      = 2,  /* cap is the wrong object type for the op     */
    FLINT_ERR_CAP_RIGHTS    = 3,  /* cap lacks the required rights (K3.3)        */
    FLINT_ERR_MSG_TOO_LONG  = 4,  /* message length/caps exceed configured max   */
    FLINT_ERR_WOULD_BLOCK   = 5,  /* non-blocking op would have blocked          */
    FLINT_ERR_NO_MEMORY     = 6,  /* Untyped cannot satisfy a Retype (K2.1)      */
    FLINT_ERR_RANGE         = 7,  /* an argument is out of its valid range       */
    FLINT_ERR_STATE         = 8,  /* operation illegal in the current state      */
    FLINT_ERR_EXISTS        = 9,  /* destination slot is already occupied        */
    FLINT_ERR_EMPTY         = 10  /* source slot is empty                        */
} flint_status_t;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FLINT_STATUS_H */
