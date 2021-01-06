#ifndef __MB_SLAVE_H__
#define __MB_SLAVE_H__

#include "mb_port.h"
#include "mb.h"
#include "mb_slave_reg.h"

MB_ErrorRet MB_Slave_Init_RTU(uint8_t slave_address, uint8_t num, ...);
MB_ErrorRet MB_Slave_Init_Registers(MB_SlaveReg *regs);

#endif /* __MB_SLAVE_H__ */

