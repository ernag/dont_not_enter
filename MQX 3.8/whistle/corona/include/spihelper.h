/*
 * spihelper.h
 *
 *  Created on: Feb 27, 2013
 *      Author: Chris
 */

#ifndef SPIHELPER_H_
#define SPIHELPER_H_

MQX_FILE_PTR spiInit(uint_32 baud, uint_32 clock_mode, uint_32 endian, uint_32 transfer_mode);
int_32 spiChangeBaud( MQX_FILE_PTR spifd, uint_32 baud );
int_32 spiWrite(uint8_t *pTXBuf, uint_32 txBytes, boolean flush, boolean deassertAfterFlush, MQX_FILE_PTR fd);
int_32 spiRead(uint8_t *pRXBuf, uint_32 rxBytes, MQX_FILE_PTR fd);
int_32 spiManualDeassertCS(MQX_FILE_PTR fd);
#endif /* SPIHELPER_H_ */
