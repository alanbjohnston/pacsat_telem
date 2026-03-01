/*
 * pacsat_telem.h
 *
 *  Created on: Jan 6, 2026
 *      Author: g0kla
 */

#ifndef PACSAT_TELEM_H_
#define PACSAT_TELEM_H_

#define WOD_PATH "wod"
#define PERIOD_TO_SAMPLE_TELEM_IN_SECONDS 10
#define PERIOD_TO_STORE_WOD_IN_SECONDS 60
#define MAX_WOD_FILE_SIZE_IN_KB 10

#define MAX_NUMBER_FILE_IO_ERRORS 5

#define BROADCAST_CALLSIGN "AMSAT-11"
#define TIME_CALL "TIME-1"
#define TELEM_TYPE_1_CALL "TLMP1"
#define TELEM_TYPE_2_CALL "TLMP2"
#define PID_COMMAND 0xBC
#define PID_FILE 0xBB
#define PID_NO_PROTOCOL 0xF0



#endif /* PACSAT_TELEM_H_ */
