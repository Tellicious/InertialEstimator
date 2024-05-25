/* BEGIN Header */
/**
 ******************************************************************************
 * \file            altitudeKF.h
 * \author          Andrea Vivani
 * \brief           Kalman filter for altitude estimation
 ******************************************************************************
 * \copyright
 *
 * Copyright 2023 Andrea Vivani
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 ******************************************************************************
 */
/* END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ALTITUDEKF_H__
#define __ALTITUDEKF_H__

#ifdef __cplusplus
extern "C" {
#endif
/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include "commonTypes.h"
#include "main.h"

/* Configuration -------------------------------------------------------------*/

/* Choose whether to use accurate or approximate altitude calculation based on ISA conditions */
// #define configALTITUDE_KF_USE_APPROX_ALTITUDE

/* Choose whether to use accelerometer high-pass filtering */
// #define configALTITUDE_KF_ACC_HP_FILTER

/* Choose whether to use ACC_D for altitude correction (calculated according to attitude) or ACC_Z raw */
// #define configALTITUDE_KF_USE_ACC_D

/* Choose whether to use IMU EKF velocity estimation to correct altitude */
// #define configALTITUDE_KF_USE_VELD_CORRECTION

/* Temperature variation rate with altitude, in K/m */
#define configALTITUDE_KF_CONST_TEMP_RATE 0.0065f

/* Mean molar mass of atmospheric gases, in kg/mol */
#define configALTITUDE_KF_CONST_M         0.02896f

/* Universal gas constant, in J/mol/K */
#define configALTITUDE_KF_CONST_R         8.314f

/* Loop time, in s */
#ifndef configALTITUDE_KF_LOOP_TIME_S
#error configALTITUDE_KF_LOOP_TIME_S must be defined
#endif

/* Maximum acceleration to use accelerometer for correction, in m/s^2 */
#ifndef configALTITUDE_KF_MAX_ACCEL_DOWN
#define configALTITUDE_KF_MAX_ACCEL_DOWN 14.7f
#endif

/* Deadband of ACC_D, in m/s^2 */
#ifndef configALTITUDE_KF_ACCEL_D_DEADBAND
#define configALTITUDE_KF_ACCEL_D_DEADBAND 0.03f
#endif

/* LIDAR sample time, in s */
#ifndef configALTITUDE_KF_LIDAR_UPDATE_TIME_S
#define configALTITUDE_KF_LIDAR_UPDATE_TIME_S 0.025f
#endif

/* Maximum altitude to use LIDAR for correction, in mm */
#ifndef configALTITUDE_KF_MAX_LIDAR_ALT
#define configALTITUDE_KF_MAX_LIDAR_ALT 3500
#endif

/* Maximum RoC to use LIDAR for correction, in m/s */
#ifndef configALTITUDE_KF_MAX_LIDAR_ROC
#define configALTITUDE_KF_MAX_LIDAR_ROC 1.f
#endif

/* Maximum Roll and Pitch to use LIDAR for correction, in rad */
#ifndef configALTITUDE_KF_MAX_LIDAR_ROLL_PITCH
#define configALTITUDE_KF_MAX_LIDAR_ROLL_PITCH 0.4f
#endif

/* LIDAR derivative filter constant */
#ifndef configALTITUDE_KF_LIDAR_DIFF_ND
#define configALTITUDE_KF_LIDAR_DIFF_ND 25.f
#endif

/* Accelerometer high-pass filter frequency, in Hz*/
#ifndef configALTITUDE_KF_ACCEL_HP_FREQ
#define configALTITUDE_KF_ACCEL_HP_FREQ 0.15f
#endif

/* Kalman filter gains */
#if (configUSE_ALT_TOF != configTOF_DISABLE) && defined(configALTITUDE_KF_USE_VELD_CORRECTION)
#define configALTITUDE_KF_M_HH 3.159691724039985e-03f
#define configALTITUDE_KF_M_VH 1.902561445841514e-03f
#define configALTITUDE_KF_M_AH 7.531097964645326e-07f
#define configALTITUDE_KF_M_BH -8.625898582504006e-04f
#define configALTITUDE_KF_M_HL 2.421536864478208e-03f
#define configALTITUDE_KF_M_VL 9.797525441182712e-03f
#define configALTITUDE_KF_M_AL 3.081559239856321e-03f
#define configALTITUDE_KF_M_BL -1.745255910005277e-02f
#define configALTITUDE_KF_M_HV 2.421536864478211e-03f
#define configALTITUDE_KF_M_VV 9.797525441182722e-03f
#define configALTITUDE_KF_M_AV 3.081559239856331e-03f
#define configALTITUDE_KF_M_BV -1.745255910005278e-02f
#define configALTITUDE_KF_M_HA 1.952486423860921e-05f
#define configALTITUDE_KF_M_VA 3.857815251892414e-03f
#define configALTITUDE_KF_M_AA 6.902251810411432e-02f
#define configALTITUDE_KF_M_BA 1.815865407043496e-03f
#elif (configUSE_ALT_TOF != configTOF_DISABLE) || defined(configALTITUDE_KF_USE_VELD_CORRECTION)
#define configALTITUDE_KF_M_HH 4.453061957891071e-03f
#define configALTITUDE_KF_M_VH 3.605278017532177e-03f
#define configALTITUDE_KF_M_AH 9.899305752637477e-06f
#define configALTITUDE_KF_M_BH -2.010776171599746e-03f
#define configALTITUDE_KF_M_HL 4.573132243008134e-03f
#define configALTITUDE_KF_M_VL 1.590254612313570e-02f
#define configALTITUDE_KF_M_AL 3.240372918253922e-03f
#define configALTITUDE_KF_M_BL -2.466265026688891e-02f
#define configALTITUDE_KF_M_HV 4.573132243008134e-03f  // same as LIDAR
#define configALTITUDE_KF_M_VV 1.590254612313570e-02f  // same as LIDAR
#define configALTITUDE_KF_M_AV 3.240372918253922e-03f  // same as LIDAR
#define configALTITUDE_KF_M_BV -2.466265026688891e-02f // same as LIDAR
#define configALTITUDE_KF_M_HA 3.350519914362559e-05f
#define configALTITUDE_KF_M_VA 4.042050177943409e-03f
#define configALTITUDE_KF_M_AA 6.909705338644491e-02f
#define configALTITUDE_KF_M_BA 1.343684827267039e-03f
#else
#define configALTITUDE_KF_M_HH 1.660131767767979e-02f
#define configALTITUDE_KF_M_VH 2.744933752564912e-02f
#define configALTITUDE_KF_M_AH 1.788970980633465e-04f
#define configALTITUDE_KF_M_BH -2.217573853932911e-02f
#define configALTITUDE_KF_M_HA 2.791600132999355e-04f
#define configALTITUDE_KF_M_VA 4.891307927430644e-03f
#define configALTITUDE_KF_M_AA 6.918802688106758e-02f
#define configALTITUDE_KF_M_BA 8.328408234164664e-05f

/* Typedefs ------------------------------------------------------------------*/

/*
 * Altitude state
 * Altitude in m, rate-of-climb in m/s, vertical acceleration in m/s^2, bias of vertical acceleration
 */
typedef struct {
    float alt, RoC, vAcc, b_vAcc;
    /* Private variables, result of prediction step */
    float _altPred, _RoCPred, _vAccPred;
} altitudeState_t;

/* Function prototypes -------------------------------------------------------*/

/**
 * \brief           Altitude Kalman filter initialization
 *
 * \param[out]      altState: altitude state object
 * \param[in]       pressGround: ground pressure in hPa
 * \param[in]       tempGround: ground temperature in K
 */
void altitudeKF_init(altitudeState_t* altState, float pressGround, float tempGround);

/**
 * \brief           Altitude Kalman filter prediction
 * 
 * \param[out]      altState: altitude state object
 */
void altitudeKF_prediction(altitudeState_t* altState);

/**
 * \brief           Altitude Kalman filter update with barometer and accelerometer measurements
 *
 * \param[out]      altState: altitude state object
 * \param[in]       press: measured pressure, in hPa
 * \param[in]       accel: accelerometer measurements vector, in m/s^2
 * \param[in]       b_az: bias of accelerometer measurement along local z axis (if known)
 * \param[in]       angles: current attitude in Euler angles
 */
void altitudeKF_updateBaroAccel(altitudeState_t* altState, float press, axis3f_t accel, float b_az, axis3f_t angles);

#if (configUSE_ALT_TOF != configTOF_DISABLE)
/**
 * \brief           Altitude Kalman filter update with LIDAR / ToF sensor measurements
 *
 * \param[out]      altState: altitude state object
 * \param[in]       ToFAlt: LIDAR / ToF measured altitude in m
 * \param[in]       angles: current attitude in Euler angles
 */
void altitudeKF_updateLIDAR(altitudeState_t* altState, float ToFAlt, axis3f_t angles);
#endif

#ifdef configALTITUDE_KF_USE_VELD_CORRECTION
/**
 * \brief           Altitude Kalman filter update with downward velocity
 *
 * \param[out]      altState: altitude state object
 * \param[in]       velocities: velocities vector along local axes
 * \param[in]       angles: current attitude in Euler angles
 */
void altitudeKF_updateVelD(altitudeState_t* altState, axis3f_t velocities, axis3f_t angles);
#endif

/**
 * \brief           Altitude Kalman filter reset
 *
 * \param[out]      altState: altitude state object
 * \param[in]       pressGround: ground pressure in Pa
 */
void altitudeKF_reset(altitudeState_t* altState, float pressGround);

#ifdef __cplusplus
}
#endif

#endif /* __ALTITUDEKF_H__ */
