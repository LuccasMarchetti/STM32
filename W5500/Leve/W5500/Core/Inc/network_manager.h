/*
 * network_manager.h
 *
 *  Created on: Feb 21, 2026
 *      Author: Luccas
 */

#ifndef INC_NETWORK_MANAGER_H_
#define INC_NETWORK_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "w5500.h"
void W5500_NetworkManagerTask(W5500_Driver_t *drv);
#ifdef __cplusplus
}
#endif

#endif /* INC_NETWORK_MANAGER_H_ */
