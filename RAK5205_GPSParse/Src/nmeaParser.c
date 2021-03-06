/*
 * nmeaParser.c
 *
 *  Created on: Aug 20, 2018
 *      Author: uli
 */
#include "stm32l1xx_hal.h"
#include "nmeaParser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEBUG 1

uint8_t noOfGSVRecords;
nmeaGSVType *gsvData=NULL;
/* return the nect character from the sentence and checks that the following char
 * is a ','. The pointer ins advanced to the next token
 */
static nmeaStatus getUint8(uint8_t **nmeaSentencePtr, uint8_t *c) {
	uint8_t *ptr = *nmeaSentencePtr;
	*c = *ptr++;
	if ((*ptr != ',') && (*ptr != '*')){
		printf("getUint8: expected \',\' but got %c",*ptr);
		return NMEA_INVALID_SYNTAX;
	}
	ptr++;
	*nmeaSentencePtr = ptr;
	return NMEA_OK;
}

static nmeaStatus getByte(uint8_t **nmeaSentencePtr, uint8_t *byte) {
	uint8_t *ptr = *nmeaSentencePtr;
	char *endPtr;
	*byte = (uint8_t)strtol((char *)ptr,&endPtr,10);
	if ((byte == 0) && (endPtr == (char *)ptr)) {
		printf("getByte: Conversion error\r\n");
		return NMEA_INVALID_SYNTAX;
	}
//	printf("getByte: the endPtr points to %s\r\n",endPtr);
	if ((*endPtr != ',') && (*endPtr != '*')) {
		printf("getByte: next char should be \',\' or \'*\' but is %c 0x%02x\r\n",*endPtr,*endPtr);
		endPtr++;
		return NMEA_INVALID_SYNTAX;
	}
	endPtr++;
	*nmeaSentencePtr = (uint8_t *)endPtr;
	return NMEA_OK;
}

static nmeaStatus getWord(uint8_t **nmeaSentencePtr, uint16_t *word) {
	uint8_t *ptr = *nmeaSentencePtr;
	char *endPtr;
	*word = (uint16_t)strtol((char *)ptr,&endPtr,10);
	if ((word == 0) && (endPtr == (char *)ptr)) {
		printf("getWord: Conversion error\r\n");
		return NMEA_INVALID_SYNTAX;
	}
//	printf("getByte: the endPtr points to %s\r\n",endPtr);
	if ((*endPtr != ',') && (*endPtr != '*')) {
		printf("getWord: next char should be \',\' or \'*\' but is %c 0x%02x\r\n",
				*endPtr, *endPtr);
		endPtr++;
		return NMEA_INVALID_SYNTAX;
	}
	endPtr++;
	*nmeaSentencePtr = (uint8_t *)endPtr;
	return NMEA_OK;
}
/* read a double value from the sentence buffe and increase the buffer pointer
 * to the next token
 */
static nmeaStatus getDouble(uint8_t **nmeaSentencePtr, double *result) {
	uint8_t *ptr = *nmeaSentencePtr;
//	int i;
	char *endPtr;
	if (((*result = strtod((char *)ptr,&endPtr))== 0) && (endPtr == (char *)ptr)) {
		printf("getDouble: conversion error\r\n");
		return NMEA_INVALID_SYNTAX;
	}

	if ((*endPtr != ',') && (*endPtr != '*')) {
		printf("getDouble: next char should be \',\' or \'*\' but is %c 0x%02x\r\n",
				*endPtr,*endPtr);
		endPtr++;
		return NMEA_INVALID_SYNTAX;
	}
	endPtr++;
	*nmeaSentencePtr = (uint8_t *) endPtr;
	return NMEA_OK;
}

nmeaStatus nmeaInterpret(uint8_t *nmeaSentence) {
	uint8_t *nmeaSentencePtr;
	uint8_t gsvSequenceNo,tmp;
	nmeaRMCType rmcData;
	nmeaVTGType vtgData;
	nmeaGGAType ggaData;
	nmeaGSAType gsaData;
	nmeaGLLType gllData;
	nmeaGSVType *gsvDataPtr;

	char tmpBuf[12];
	char *endPtr;
	enum nmeaHdr hdr;
	int satelliteCnt;

	const char *nmeaHeaders[] = {
			"GPRMC","GNRMC","GPGGA","GPVTG","GPGSA","GNGSA","GPGSV","GLGSV", "GPGLL","GNGLL",NULL
	};
	nmeaSentencePtr = nmeaSentence;
	if (*nmeaSentencePtr++ != '$')
		return NMEA_INVALID_HDR;
	for (hdr = GPRMC; hdr < INVALID+1; hdr++) {
//	while (nmeaHeaders[hdr] != NULL) {
		if (!strncmp(nmeaHeaders[hdr],(char *)nmeaSentencePtr,5)) {
//			printf("\r\nnmeaInterpret: Found: %s header\r\n",nmeaHeaders[hdr]);
			/* skip over header */
			nmeaSentencePtr += 5;
			if (*nmeaSentencePtr++ != ',')
				return NMEA_INVALID_SYNTAX;

			switch (hdr) {
			case GPRMC:
			case GNRMC:
#ifdef DEBUG

				printf("==========================\r\n");
				if (hdr == GPRMC)
					printf("GPRMC: Recommended Minimum\r\n");
				else
					printf("GNRMC: Recommended Minimum\r\n");
				printf("==========================\r\n");
#endif
				rmcData.invalid = NMEA_INVALID_SYNTAX;

			/* convert hour */
				tmpBuf[2] = '\0';
				tmpBuf[0] = *nmeaSentencePtr++;
				tmpBuf[1] = *nmeaSentencePtr++;
				rmcData.time.hour = (uint8_t) strtol(tmpBuf,&endPtr,10);
				if ((rmcData.time.hour == 0) && (endPtr == tmpBuf)) {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				/* convert mins */
				tmpBuf[0] = *nmeaSentencePtr++;
				tmpBuf[1] = *nmeaSentencePtr++;
				rmcData.time.min = (uint8_t) strtol(tmpBuf,&endPtr,10);
				if ((rmcData.time.min == 0) && (endPtr == tmpBuf)) {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				/* convert secs */
				tmpBuf[0] = *nmeaSentencePtr++;
				tmpBuf[1] = *nmeaSentencePtr++;
				rmcData.time.sec = (uint8_t) strtol(tmpBuf,&endPtr,10);
				if ((rmcData.time.sec == 0) && (endPtr == tmpBuf)) {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}

#ifdef DEBUG
				printf("time: %02d:%02d:%02d UTC\r\n",
						rmcData.time.hour,rmcData.time.min,rmcData.time.sec);
#endif
				if (*nmeaSentencePtr++ != '.') {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				nmeaSentencePtr += 3;
				if (*nmeaSentencePtr++ != ',') {
					printf("nmeaInterpret: expected \',\'\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				if (getUint8(&nmeaSentencePtr,&rmcData.status) != NMEA_OK)
					return NMEA_INVALID_SYNTAX;
				printf("status: active (A) or void (V):  %c\r\n",rmcData.status);
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr ++;
				else {
					if (getDouble(&nmeaSentencePtr,&rmcData.latitude) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("latitude: %lf\r\n",rmcData.latitude);
#endif
				}
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr ++;
				else {
					if (getUint8(&nmeaSentencePtr,&rmcData.northOrSouth) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					printf("GPS northOrSouth: %c\r\n",rmcData.northOrSouth);
#endif
				}

				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr ++;
				else {
					if (getDouble(&nmeaSentencePtr,&rmcData.longitude) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("longitude: %lf\r\n",rmcData.longitude);
#endif
				}

				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr ++;
				else {
					if (getUint8(&nmeaSentencePtr,&rmcData.eastOrWest) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					printf("GPS eastOrWest: %c\r\n",rmcData.eastOrWest);
#endif
				}
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr ++;
				else {
					if (getDouble(&nmeaSentencePtr,&rmcData.speedOverGround) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("speedOverGround: %lf\r\n",rmcData.speedOverGround);
#endif
				}
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&rmcData.trackAngle) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("trackAngle: %lf\r\n",rmcData.trackAngle);
#endif
				}
				/* read the date */
				tmpBuf[0] = *nmeaSentencePtr++;
				tmpBuf[1] = *nmeaSentencePtr++;
				rmcData.date.day = (uint8_t) strtol(tmpBuf,&endPtr,10);
				if ((rmcData.date.day == 0) && (endPtr == tmpBuf)) {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				tmpBuf[0] = *nmeaSentencePtr++;
				tmpBuf[1] = *nmeaSentencePtr++;
				rmcData.date.month = (uint8_t) strtol(tmpBuf,&endPtr,10);
				if ((rmcData.date.month == 0) && (endPtr == tmpBuf)) {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				tmpBuf[0] = *nmeaSentencePtr++;
				tmpBuf[1] = *nmeaSentencePtr++;
				rmcData.date.year = (uint8_t) strtol(tmpBuf,&endPtr,10);
				if ((rmcData.date.year == 0) && (endPtr == tmpBuf)) {
					printf("nmeaInterpret: date conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
#ifdef DEBUG
				printf("date:%02d.%02d 20%02d\r\n",
						rmcData.date.day,rmcData.date.month,rmcData.date.year);
#endif
				if (*nmeaSentencePtr++ != ',') {
					printf("nmeaInterpret: sentence syntax error\r\n");
					return NMEA_INVALID_SYNTAX;
				}

				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&rmcData.magneticVariationDegrees) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("magnetic variation in degrees: %lf\r\n",rmcData.trackAngle);
#endif
				}
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&rmcData.magneticVariationEW) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("magnetic variation in degrees: %lf\r\n",rmcData.trackAngle);
#endif
				}

				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					rmcData.positioningMode = *nmeaSentencePtr++;

#ifdef DEBUG
					switch (rmcData.positioningMode) {
					case 'N':
						printf("positioning mode: No fix\r\n");
						break;
					case 'A':
						printf("positioning mode: Autonomous GNSS Fix\r\n");
						break;
					case 'D':
						printf("positioning mode: Differential GNSS fix\r\n");
						break;
					default:
						printf("Unknown positioning mode: %c\r\n",rmcData.positioningMode);
					}
#endif
				}
				rmcData.invalid = NMEA_OK;

				break;
			case GPGGA:
#ifdef DEBUG
				printf("==================\r\n");
				printf("Essential Fix Data\r\n");
				printf("==================\r\n");
#endif
				/* convert hour */
				tmpBuf[2] = '\0';
				tmpBuf[0] = *nmeaSentencePtr++;
				tmpBuf[1] = *nmeaSentencePtr++;
				ggaData.time.hour = (uint8_t) strtol(tmpBuf,&endPtr,10);
				if ((ggaData.time.hour == 0) && (endPtr == tmpBuf)) {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				/* convert mins */
				tmpBuf[0] = *nmeaSentencePtr++;
				tmpBuf[1] = *nmeaSentencePtr++;
				ggaData.time.min = (uint8_t) strtol(tmpBuf,&endPtr,10);
				if ((ggaData.time.min == 0) && (endPtr == tmpBuf)) {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				/* convert secs */
				tmpBuf[0] = *nmeaSentencePtr++;
				tmpBuf[1] = *nmeaSentencePtr++;
				ggaData.time.sec = (uint8_t) strtol(tmpBuf,&endPtr,10);
				if ((ggaData.time.sec == 0) && (endPtr == tmpBuf)) {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
#ifdef DEBUG
				printf("time: %02d:%02d:%02d UTC\r\n",
						ggaData.time.hour,ggaData.time.min,ggaData.time.sec);
#endif
				if (*nmeaSentencePtr++ != '.') {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				nmeaSentencePtr += 3;
				if (*nmeaSentencePtr++ != ',') {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				/*
				 * latitude
				 */
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&ggaData.latitude) != NMEA_OK){
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("latitude: %lf\r\n",ggaData.latitude);
#endif
				}
				/*
				 * North or South
				 */
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getUint8(&nmeaSentencePtr,&ggaData.northOrSouth) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					printf("GPS northOrSouth: %c\r\n",ggaData.northOrSouth);
#endif
				}
				/*
				 * longitude
				 */
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {

					if (getDouble(&nmeaSentencePtr,&ggaData.longitude) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("longitude: %lf\r\n",ggaData.longitude);
#endif
				}
				/*
				 * east or west
				 */
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {

					if (getUint8(&nmeaSentencePtr,&ggaData.eastOrWest) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					printf("GPS eastOrWest: %c\r\n",ggaData.eastOrWest);
#endif
				}

				/* fix quality */
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					ggaData.fixQuality = *nmeaSentencePtr++ - '0';
#ifdef DEBUG
					printf("fix quality: %d\r\n",ggaData.fixQuality);
#endif

					if (*nmeaSentencePtr++ != ',') {
						printf("nmeaInterpret: sentence syntax error\r\n");
						return NMEA_INVALID_SYNTAX;
					}
				}
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					tmpBuf[0] = *nmeaSentencePtr++;
					tmpBuf[1] = *nmeaSentencePtr++;
					ggaData.noOfSatellites = (uint8_t) strtol(tmpBuf,&endPtr,10);

					if ((ggaData.noOfSatellites == 0) && (endPtr == tmpBuf)) {
						printf("nmeaInterpret: conversion error for no of satellites\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("no of satellites seen: %d\r\n",ggaData.noOfSatellites);
#endif
				}

				if (*nmeaSentencePtr == ',') {
					nmeaSentencePtr++;
				}
				else {
					if (getDouble(&nmeaSentencePtr,&ggaData.horDilution) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble() horDilution\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("horizontal dilution: %lf\r\n",ggaData.horDilution);
#endif
				}

				if (*nmeaSentencePtr == ',') {
					nmeaSentencePtr++;
				}
				else {
					/* altitude */
					if (getDouble(&nmeaSentencePtr,&ggaData.altitude) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble() altitude\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("altitude: %lf\r\n",ggaData.altitude);
#endif
				}

				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {

					if (getUint8(&nmeaSentencePtr,&ggaData.altitudeUnit) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					printf("unit of altitude: %c\r\n",ggaData.altitudeUnit);
#endif
				}

				/* height of GeoID */
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&ggaData.heightOfGeoide) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("height of GeoID: %lf\r\n",ggaData.heightOfGeoide);
#endif
				}

				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getUint8(&nmeaSentencePtr,&ggaData.heightOfGeoideUnit) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					printf("units for height of GeoID: %c\r\n",ggaData.heightOfGeoideUnit);
#endif
				}
				break;
			case GPVTG:
#ifdef DEBUG
				printf("=========================\r\n");
				printf("GPVTG: Velocity made good\r\n");
				printf("=========================\r\n");
#endif
//				printf("nmeaInterpret sentence %s\r\n",nmeaSentencePtr);
				vtgData.invalid = NMEA_INVALID_SYNTAX;
				/* true track */
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&vtgData.trueTrack) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
					printf("nmeaInterpret: trueTrack: %lf\r\n",vtgData.trueTrack);
				}
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getUint8(&nmeaSentencePtr,&vtgData.trueTrackChar) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
					printf("nmeaInterpret: true track character: %c\r\n",vtgData.trueTrackChar);
				}
				/* magnetic track */
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&vtgData.trueTrack) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
					printf("nmeaInterpret: trueTrack: %lf\r\n",vtgData.trueTrack);
				}
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getUint8(&nmeaSentencePtr,&vtgData.magneticTrackChar) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
					printf("nmeaInterpret: magnetic track character: %c\r\n",vtgData.magneticTrackChar);
				}

				/* speed in knots and km/h */
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&vtgData.speedKnots) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
					printf("nmeaInterpret: speed in knots: %lf\r\n",vtgData.speedKnots);
				}
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getUint8(&nmeaSentencePtr,&vtgData.speedKnotsChar) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
					printf("nmeaInterpret: speed in knots character: %c\r\n",vtgData.speedKnotsChar);
				}
				/* speed in km/h */
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&vtgData.speedKmh) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
					printf("nmeaInterpret: speed in km/h: %lf\r\n",vtgData.speedKmh);
				}
				if (*nmeaSentencePtr == ',')
					nmeaSentencePtr++;
				else {
					if (getUint8(&nmeaSentencePtr,&vtgData.speedKmhChar) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
					printf("nmeaInterpret: speed in km/h character: %c\r\n",vtgData.speedKmhChar);
				}
				break;

			case GPGSA:
			case GNGSA:
#ifdef DEBUG
				printf("=============================\r\n");
				if (hdr == GPGSA)
					printf("GPGSA: Overall Satellite data\r\n");
				else
					printf("GNGSA: Overall Satellite data\r\n");
				printf("=============================\r\n");
#endif
//				printf("nmeaSentence: %s\r\n",nmeaSentencePtr);
				gsaData.manualAuto = *nmeaSentencePtr++;
				if (*nmeaSentencePtr++ != ',') {
					printf("nmeaInterpret: expected \',\'\r\n");
					return NMEA_INVALID_SYNTAX;
				}
#ifdef DEBUG
				printf("manual/auto: %c\r\n",gsaData.manualAuto);
#endif
				gsaData.threeDFix = *nmeaSentencePtr++ - '0';
				if (*nmeaSentencePtr++ != ',') {
					printf("nmeaInterpret: expected \',\'\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				for (int i=0 ;i<12; i++) {
					if (*nmeaSentencePtr ==',') {
						nmeaSentencePtr++;
						continue;
					}
					if (getByte(&nmeaSentencePtr,&gsaData.prns[i]) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					printf("prns[%d]: %d\r\n",i,gsaData.prns[i]);
#endif
				}
				if (*nmeaSentencePtr ==',')
					nmeaSentencePtr++;
				else {
					/* pdop */
					if (getDouble(&nmeaSentencePtr,&gsaData.pdop) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("pdop: %lf\r\n",gsaData.pdop);
#endif
				}
				/* hor dilution */
				if (*nmeaSentencePtr ==',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&gsaData.horDilution) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("horizontal dilution: %lf\r\n",gsaData.horDilution);
#endif
				}
				if (*nmeaSentencePtr ==',')
					nmeaSentencePtr++;
				else {
					/* ver dilution */
					if (getDouble(&nmeaSentencePtr,&gsaData.verDilution) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("vertical dilution: %lf\r\n",gsaData.verDilution);
#endif
				}
				gsaData.invalid = NMEA_OK;
				break;
			case GPGSV:
			case GLGSV:
#ifdef DEBUG
				printf("=========================\r\n");
				if (hdr==GPGSV)
					printf("GPGSV: satellites in view\r\n");
				else
					printf("GLGSV: satellites in view\r\n");
				printf("=========================\r\n");
#endif
				/* find no of GSV records needed to store all the information */


				if (getByte(&nmeaSentencePtr,&tmp) != NMEA_OK) {
					printf("nmeaInterpret: Cannot find the no of GSV records needed\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				if (noOfGSVRecords == 0) {
					noOfGSVRecords = tmp;
					printf("No of GSV structures is %d\r\n",noOfGSVRecords);
					gsvData = calloc(sizeof(nmeaGSVType),noOfGSVRecords);
					if (gsvData == NULL) {
						printf("nmeaInterpret: Could not allocate enough memory for the GSV records\r\n");
						return NMEA_NOMEM;
					}
					else
						printf("Memory for the GSV records successfully allocated\r\n");
				}

				if (getByte(&nmeaSentencePtr,&gsvSequenceNo) != NMEA_OK){
					printf("nmeaInterpret: Cannot find GSV sequence number\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				printf("gsv sequence number is %d\r\n",gsvSequenceNo);
				gsvDataPtr = gsvData + gsvSequenceNo*sizeof(nmeaGSVType);
				gsvDataPtr -> noOfSentences = noOfGSVRecords;
				gsvDataPtr -> sequenceNo = gsvSequenceNo;


				if (getByte(&nmeaSentencePtr,&(gsvDataPtr ->satellitesInView)) != NMEA_OK){
					printf("Cannot find number of satellites in view\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				printf("no of sentences: %d\r\n",gsvDataPtr -> noOfSentences);
				printf("sequence number: %d\r\n",gsvDataPtr -> sequenceNo);
				printf("no of satellites in view: %d\r\n",gsvDataPtr ->satellitesInView);

				if (gsvDataPtr -> sequenceNo < gsvDataPtr -> noOfSentences )
					satelliteCnt = 4;
				else
					satelliteCnt = gsvDataPtr -> satellitesInView - 4 * (gsvDataPtr -> sequenceNo-1);
//				printf("satellite count: %d\r\n",satelliteCnt);
				for (int i=0; i<satelliteCnt; i++) {
					if (*nmeaSentencePtr == '*')
						break;
					if (*nmeaSentencePtr==',')
						nmeaSentencePtr++;
					else {
						if (getByte(&nmeaSentencePtr,&(gsvDataPtr -> specs[i].prn)) != NMEA_OK){
							printf("Cannot get prn for satellite specs\r\n");
							return NMEA_INVALID_SYNTAX;
						}
#ifdef DEBUG
						printf("satellite specs[%d].prn: %d\r\n",i,gsvDataPtr -> specs[i].prn);
#endif
					}

					if (*nmeaSentencePtr == '*')
						break;
					if (*nmeaSentencePtr==',')
						nmeaSentencePtr++;
					else {
						if (getByte(&nmeaSentencePtr,&(gsvDataPtr -> specs[i].elevation)) != NMEA_OK){
							printf("Cannot get elevation for satellite specs\r\n");
							return NMEA_INVALID_SYNTAX;
						}
						printf("satellite specs[%d].elevation: %d\r\n",i,gsvDataPtr -> specs[i].elevation);
					}

					if (*nmeaSentencePtr == '*')
						break;
					if (*nmeaSentencePtr==',')
						nmeaSentencePtr++;
					else {
						if (getWord(&nmeaSentencePtr,&(gsvDataPtr -> specs[i].azimuth)) != NMEA_OK){
							printf("Cannot get azimuth for satellite specs\r\n");
							return NMEA_INVALID_SYNTAX;
						}
#ifdef DEBUG
						printf("satellite specs[%d].azimuth: %d\r\n",i,gsvDataPtr -> specs[i].azimuth);
#endif
					}

					if (*nmeaSentencePtr == '*')
						break;
					if (*nmeaSentencePtr==',')
						nmeaSentencePtr++;
					else {
						if (getByte(&nmeaSentencePtr,&(gsvDataPtr -> specs[i].snr)) != NMEA_OK){
							printf("Cannot snr in satellite specs\r\n");
							return NMEA_INVALID_SYNTAX;
						}
#ifdef DEBUG
						printf("satellite specs[%d].srn: %d\r\n",i,gsvDataPtr -> specs[i].snr);
#endif
					}
				}

				gsvDataPtr -> invalid = NMEA_OK;
				break;

			case GPGLL:
			case GNGLL:
#ifdef DEBUG
				printf("========================================\r\n");
				if (hdr==GPGLL)
					printf("GPGLL: geographic latitude and longitude\r\n");
				else
					printf("GNGLL: geographic latitude and longitude\r\n");
				printf("========================================\r\n");
#endif
				/*
				 * latitude
				 */
				if (*nmeaSentencePtr==',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&gllData.latitude) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("latitude: %lf\r\n",gllData.latitude);
#endif
				}

				if (*nmeaSentencePtr==',')
					nmeaSentencePtr++;
				else {
					if (getUint8(&nmeaSentencePtr,&gllData.northOrSouth) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					printf("GPS North or South: %c\r\n",gllData.northOrSouth);
#endif
				}
				/*
				 * longitude
				 */
				if (*nmeaSentencePtr==',')
					nmeaSentencePtr++;
				else {
					if (getDouble(&nmeaSentencePtr,&gllData.longitude) != NMEA_OK) {
						printf("nmeaInterpret: error in getDouble()\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("longitude: %lf\r\n",gllData.longitude);
#endif
				}

				if (*nmeaSentencePtr==',')
					nmeaSentencePtr++;
				else {
					if (getUint8(&nmeaSentencePtr,&gllData.eastOrWest) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					printf("nmeaInterpret: GPS East or West: %c\r\n",gllData.eastOrWest);
#endif
				}

				if (*nmeaSentencePtr==',')
					nmeaSentencePtr++;
				else {
					/* convert hour */
					tmpBuf[2] = '\0';
					tmpBuf[0] = *nmeaSentencePtr++;
					tmpBuf[1] = *nmeaSentencePtr++;
					gllData.time.hour = (uint8_t) strtol(tmpBuf,&endPtr,10);
					if ((gllData.time.hour == 0) && (endPtr == tmpBuf)) {
						printf("nmeaInterpret: time conversion error\r\n");
						return NMEA_INVALID_SYNTAX;
					}
					/* convert mins */
					tmpBuf[0] = *nmeaSentencePtr++;
					tmpBuf[1] = *nmeaSentencePtr++;
					gllData.time.min = (uint8_t) strtol(tmpBuf,&endPtr,10);
					if ((gllData.time.min == 0) && (endPtr == tmpBuf)) {
						printf("nmeaInterpret: time conversion error\r\n");
						return NMEA_INVALID_SYNTAX;
					}
					/* convert secs */
					tmpBuf[0] = *nmeaSentencePtr++;
					tmpBuf[1] = *nmeaSentencePtr++;
					gllData.time.sec = (uint8_t) strtol(tmpBuf,&endPtr,10);
					if ((gllData.time.sec == 0) && (endPtr == tmpBuf)) {
						printf("nmeaInterpret: time conversion error\r\n");
						return NMEA_INVALID_SYNTAX;
					}
#ifdef DEBUG
					printf("time: %02d:%02d:%02d UTC\r\n",
							gllData.time.hour,gllData.time.min,gllData.time.sec);
#endif
				}
				if (*nmeaSentencePtr++ != '.') {
					printf("nmeaInterpret: time conversion error not '.'\r\n");
					return NMEA_INVALID_SYNTAX;
				}
				nmeaSentencePtr +=3;
				if (*nmeaSentencePtr++ != ',') {
					printf("nmeaInterpret: time conversion error\r\n");
					return NMEA_INVALID_SYNTAX;
				}

				if (*nmeaSentencePtr==',')
					nmeaSentencePtr++;
				else {
					if (getUint8(&nmeaSentencePtr,&gllData.status) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					printf("status: active (A) or void (V): %c\r\n",gllData.status);
#endif
				}

				if (*nmeaSentencePtr==',')
					nmeaSentencePtr++;
				else {
					if (getUint8(&nmeaSentencePtr,&gllData.positioningMode) != NMEA_OK)
						return NMEA_INVALID_SYNTAX;
#ifdef DEBUG
					switch (gllData.positioningMode) {
					case 'N':
						printf("positioning mode: No fix\r\n");
						break;
					case 'A':
						printf("positioning mode: Autonomous GNSS Fix\r\n");
						break;
					case 'D':
						printf("positioning mode: Differential GNSS fix\r\n");
						break;
					default:
						printf("Unknown positioning mode: %c\r\n",rmcData.positioningMode);
					}
#endif
				}

				gllData.invalid = NMEA_OK;
				break;

			default:
				printf("Invalid sentence\r\n");
				return NMEA_INVALID_HDR;
				break;
			}
			break;
		}

	}
	return NMEA_OK;
}
#undef DEBUG

static nmeaStatus nmeaCalcChecksum(uint8_t *nmeaSentence, uint8_t *cs){
	uint8_t checksum;
	uint8_t charCount;
	uint8_t *nmeaSentencePtr;
	nmeaSentencePtr = nmeaSentence;
	checksum =charCount = 0;
	if (*nmeaSentencePtr++ != '$')
		return NMEA_INVALID_HDR;
	while (*nmeaSentencePtr != '*') {
		checksum ^= *nmeaSentencePtr++;
		charCount++;
		if (charCount > NMEA_SENTENCE_MAX_LEN)
			return NMEA_INVALID_SYNTAX;
	}
//	printf("nmeaCalcChecksum: calculated checksum: 0x%02x\r\n",checksum);
	*cs = checksum;
	return NMEA_OK;
}

static nmeaStatus nmeaVerifyChecksum(uint8_t *nmeaSentence){
	uint8_t *nmeaSentencePtr;
	uint8_t checksum;
	uint8_t charCount;
	uint8_t cs;
	long csLong;
	char *endPtr;

	nmeaStatus retCode;

	if ((retCode = nmeaCalcChecksum(nmeaSentence,&checksum)) != NMEA_OK) {
		printf("nmeaVerifyChecksum: error in checksum calculation\r\n");
		return retCode;
	}

	charCount = 0;
	nmeaSentencePtr = nmeaSentence;
	while ((*nmeaSentencePtr++ != '*') && (charCount < NMEA_SENTENCE_MAX_LEN) )
		charCount++;

	if (charCount >= NMEA_SENTENCE_MAX_LEN)
		return NMEA_INVALID_SYNTAX ;
#ifdef DEBUG
	printf("nmeaVerifyChecksum: checksum in ascii: %c%c\r\n",*nmeaSentencePtr,*(nmeaSentencePtr+1));
#endif
	csLong = strtol((char *)nmeaSentencePtr,&endPtr,16);
	if ((csLong == 0) && ((uint8_t *)endPtr == nmeaSentencePtr))
		printf("nmeaVerifyChecksum: Conversion error checksum read from NMEA sentence\r\n");
	cs = (uint8_t) csLong;
#ifdef DEBUG
	printf("checksum calculated: 0x%02x, checksum read: 0x%02x\r\n",checksum,cs);
#endif
	if (cs == checksum)
		return NMEA_OK;
	else
		return NMEA_BAD_CHECKSUM;
}

static int8_t getNmeaSentence(uint8_t *buff, uint8_t *nmeaSentence, int *charCnt) {
	uint8_t *buffPtr;
	uint8_t *nmeaSentencePtr;
	buffPtr = buff;
	nmeaSentencePtr = nmeaSentence;
	int count = *charCnt;
	while ((*buffPtr != '$') && (count > 0)) {
		count--;
		buffPtr++;
	}
	if (*buffPtr != '$' ) {
		*charCnt = 0;
		return 0;
	}
	else
		while ((*buffPtr != '\n') && (count > 0)) {
			*nmeaSentencePtr++ = *buffPtr++;
			count--;
		}
	*nmeaSentencePtr = '\n';
#ifdef DEBUG
	printf("Nmea Sentence: ");
#endif
	nmeaSentencePtr = nmeaSentence;
#ifdef DEBUG
	while (*nmeaSentencePtr != '\n')
		printf("%c",*nmeaSentencePtr++);
	printf("\n");
#endif
//	printf("count: %d\r\n",count);
	*charCnt = count;
	return NMEA_OK;
}

void nmeaParse(uint8_t *buff, int charCnt) {
	int count = charCnt,offset;
	nmeaStatus retCode;
	uint8_t nmeaSentence[NMEA_SENTENCE_MAX_LEN];
	uint8_t *buffPtr;
	buffPtr = buff;
#ifdef DEBUG
	printf("nmeaParse: charCount=%d\r\n",count);
#endif
/*
 * reset the number of gsv records
 */
	noOfGSVRecords = 0;
	while (count > 0) {
		offset = charCnt - count;
		retCode = getNmeaSentence(buffPtr+offset,nmeaSentence, &count);
		if (retCode != NMEA_OK)
			printf("nmeaParse: error in getNmeaSentence: %d\r\n",retCode);
		nmeaInterpret(nmeaSentence);
		retCode = nmeaVerifyChecksum(nmeaSentence);
#ifdef DEBUG
		printf("nmeaParse: charCount=%d after call\r\n",count);
#endif
	}

}
