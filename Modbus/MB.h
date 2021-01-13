#ifndef __MB_H__
#define __MB_H__

typedef enum
{
	MB_RF_LITTLE_ENDIAN,	//16-ти битные регистры little-endian или 32-ти битные регистры little-endian
	MB_RF_BIG_ENDIAN,		//16-ти битные регистры big-endian
	MB_RF_BIG_ENDIAN_32,	//32-ти битные регистры big-endian
	MB_RF_DATA				//блок данных
} MB_RegFormat_t;

typedef enum
{
	MB_REG_INPUT,
	MB_REG_HOLDING,
} MB_RegType_t;

typedef enum
{
	MB_OK,
	MB_ERR_ADDR,
	MB_ERR_ARG,
	MB_ERR_HAL,
	MB_ERR_MEM,
} MB_ErrorRet_t;

typedef enum
{
	//Modbus standard exceptions
	MB_EX_NONE = 0x00,
	MB_EX_ILLEGAL_FUNCTION = 0x01,
	MB_EX_ILLEGAL_DATA_ADDRESS = 0x02,
	MB_EX_ILLEGAL_DATA_VALUE = 0x03,
	MB_EX_SLAVE_DEVICE_FAILURE = 0x04,
	MB_EX_ACKNOWLEDGE = 0x05,
	MB_EX_SLAVE_BUSY = 0x06,

	//Modbus library exceptions
	MB_EX_SEND_ERROR = 0x81,
	MB_EX_RECEIVE_ERROR = 0x82,
} MB_Exception_t;


#endif  /* __MB_H__ */
