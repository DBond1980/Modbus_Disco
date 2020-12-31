#ifndef __MBFRAME_H__
#define __MBFRAME_H__

/*
 *  Modbus RTU frame
 * 
 *  <--------------------- MODBUS RTU ADU (1) ----------------------------->
 *              <--------- MODBUS RTU PDU (1') -------------->
 *  +-----------+---------------+----------------------------+-------------+
 *  |  Address  | Function Code |            Data            |     CRC     |
 *  +-----------+---------------+----------------------------+-------------+
 *  |           |               |                             <------------>
 * (2)        (3/2')           (3')                                 |                                    
 *                                                                 (4)
 *                                                                 
 * (1)  ... MB_ADU_SIZE_MAX    = 256   ... MB_ADU_SIZE_MIN = 4
 * (2)  ... MB_ADU_ADDR_OFFSET = 0
 * (3)  ... MB_ADU_PDU_OFFSET  = 1
 * (4)  ... MB_ADU_SIZE_CRC    = 2
 *
 * (1') ... MB_PDU_SIZE_MAX     = 253  ... MB_PDU_SIZE_MIN = 1
 * (2') ... MB_PDU_FUNC_OFFSET  = 0
 * (3') ... MB_PDU_DATA_OFFSET  = 1
 */

#define MB_ADU_SIZE_MAX			256
#define MB_ADU_SIZE_MIN			4
#define MB_ADU_ADDR_OFFSET		0
#define MB_ADU_PDU_OFFSET		1
#define MB_ADU_SIZE_CRC			2

#define MB_PDU_SIZE_MAX			253
#define MB_PDU_SIZE_MIN			1 //Function Code
#define MB_PDU_FUNC_OFFSET		0
#define MB_PDU_DATA_OFFSET		1

#define MB_BROADCAST_ADDRESS	0
#define MB_FUNC_ERROR_MASK		0x80

#endif /* __MBFRAME_H__ */