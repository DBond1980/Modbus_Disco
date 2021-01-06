#ifndef __MB_H__
#define __MB_H__

//typedef enum
//{
//	MB_REG_READ, /*!< Read register values and pass to protocol stack. */
//	MB_REG_WRITE                /*!< Update register values. */
//} MB_RegisterMode;

typedef enum
{
	MB_REG_INPUT,
	MB_REG_HOLDING,
} MB_RegType;

typedef enum
{
	MB_OK,
	MB_ERR_ADDR,
	MB_ERR_ARG,
	MB_ERR_HAL,
	MB_ERR_MEM,
} MB_ErrorRet;

typedef enum
{
	MB_EX_NONE = 0x00,
	MB_EX_ILLEGAL_FUNCTION = 0x01,
	MB_EX_ILLEGAL_DATA_ADDRESS = 0x02,
	MB_EX_ILLEGAL_DATA_VALUE = 0x03,
	MB_EX_SLAVE_DEVICE_FAILURE = 0x04,
	MB_EX_ACKNOWLEDGE = 0x05,
	MB_EX_SLAVE_BUSY = 0x06,
} MB_Exception;

#define MB_PDU_FUNC_OFFSET 0

#endif  /* __MB_H__ */
