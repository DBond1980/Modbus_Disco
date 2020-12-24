#ifndef __MB_H__
#define __MB_H__

typedef enum
{
	MB_REG_READ, /*!< Read register values and pass to protocol stack. */
	MB_REG_WRITE                /*!< Update register values. */
} MB_RegisterMode;

/*! \ingroup modbus
 * \brief Errorcodes used by all function in the protocol stack.
 */
typedef enum
{
	MB_OK, /*!< no error. */
	MB_ERR_ADDRESS, /*!< illegal register address. */
	MB_ERR_ARG, /*!< illegal argument. */
	MB_ERR_HAL
//	MB_EPORTERR, /*!< porting layer error. */
//	MB_ENORES, /*!< insufficient resources. */
//	MB_EIO, /*!< I/O error. */
//	MB_EILLSTATE, /*!< protocol stack in illegal state. */
//	MB_ETIMEDOUT                /*!< timeout error occurred. */
} MB_ErrorRet;

#endif  /* __MB_H__ */
