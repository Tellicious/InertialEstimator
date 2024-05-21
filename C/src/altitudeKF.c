/* BEGIN Header */
/**
 ******************************************************************************
 * \file            altitudeKF.c
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

/* Includes ------------------------------------------------------------------*/

#include "altitudeKF.h"
#include "basicMath.h"
#include "string.h"

#if defined(configALTITUDE_KF_ACC_HP_FILTER) || (configUSE_ALT_TOF != configTOF_DISABLE)
#include "IIRFilters.h"
#endif

/* MATLAB Code ---------------------------------------------------------------*/

/* MATLAB CODE for KF
 * T=1/200;
 * A=[1 T T^2/2 -T^2/2; 0 1 T -T; 0 0 1 0; 0 0 0 1];
 * B=[0 0;0 0; 1 0; 0 1];
 * C=[1 0 0 0; 0 0 1 0];
 * Q=[0.36 0; 0 0.05];
 * R=[100 0; 0 70]; //R=[15^2 0; 0 36];
 * Plant=ss(A,B,C,0,T,'inputname',{'v_acc_noise', 'v_acc_bias_noise'},'outputname',{'h','vAcc'},'statename',{'h','RoC','vAcc','v_acc_bias'});
 * [kalmf,L,P,M] = kalman(Plant,Q,R);
 * M
 */

/* MATLAB CODE for KF with LIDAR
 * T=1/200;
 * A=[1 T T^2/2 -T^2/2; 0 1 T -T; 0 0 1 0; 0 0 0 1];
 * B=[0 0;0 0; 1 0; 0 1];
 * C=[1 0 0 0; 0 1 0 0; 0 0 1 0];
 * Q=[0.36 0; 0 0.05];
 * R=[100 0 0; 0 80 0; 0 0 70]; //R=[15^2 0 0; 0 80 0; 0 0 36];
 * Plant=ss(A,B,C,0,T,'inputname',{'v_acc_noise', 'v_acc_bias_noise'},'outputname',{'h','RoC','vAcc'},'statename',{'h','RoC','vAcc','v_acc_bias'});
 * [kalmf,L,P,M] = kalman(Plant,Q,R);
 * M
 */

/* MATLAB CODE for KF with LIDAR and Velocity Down correction
 * T=1/200;
 * A=[1 T T^2/2 -T^2/2; 0 1 T -T; 0 0 1 0; 0 0 0 1];
 * B=[0 0;0 0; 1 0; 0 1];
 * C=[1 0 0 0; 0 1 0 0; 0 1 0 0; 0 0 1 0];
 * Q=[0.36 0; 0 0.05];
 * R=[100 0 0 0; 0 80 0 0; 0 0 80 0; 0 0 0 70];
 * Plant=ss(A,B,C,0,T,'inputname',{'v_acc_noise', 'v_acc_bias_noise'},'outputname',{'h','RoC LIDAR','RoC EKF', 'vAcc'},'statename',{'h','RoC','vAcc','v_acc_bias'});
 * [kalmf,L,P,M] = kalman(Plant,Q,R);
 * M
 */

/* Private variables ---------------------------------------------------------*/
#ifdef configALTITUDE_KF_USE_APPROX_ALTITUDE
static float _alt0; /* Ground altitude ISA */
#else
static float _inv_pressZeroLevel; /* Ground pressure */
static float _alt_k1, _alt_kpow;  /* Altitude calculation coefficients */
#endif /* configALTITUDE_KF_USE_APPROX_ALTITUDE */

#ifdef configALTITUDE_KF_ACC_HP_FILTER
IIRFilterGeneric_t HPFilt_accD;
#endif

#if (configUSE_ALT_TOF != configTOF_DISABLE)
IIRFilterDerivative_t LIDAR_diff;
#endif

/* Private functions ---------------------------------------------------------*/
/* Calculate altitude in m from barometric pressure in hPa */
static float altitudeCalculation(float pressure) {

#ifdef configALTITUDE_KF_USE_APPROX_ALTITUDE
    /* Approximated formula with ISA parameters (t_SL = 15°C, QNH = 101325 Pa) */
    int32_t RP, h0, hs0, HP1, HP2, RH;
    int16_t hs1, dP0;
    int8_t P0;

    RP = pressure * 800.f;

    if (RP >= 824000) {
        P0 = 103;
        h0 = -138507;
        hs0 = -5252;
        hs1 = 311;
    } else if (RP >= 784000) {
        P0 = 98;
        h0 = 280531;
        hs0 = -5468;
        hs1 = 338;
    } else if (RP >= 744000) {
        P0 = 93;
        h0 = 717253;
        hs0 = -5704;
        hs1 = 370;
    } else if (RP >= 704000) {
        P0 = 88;
        h0 = 1173421;
        hs0 = -5964;
        hs1 = 407;
    } else if (RP >= 664000) {
        P0 = 83;
        h0 = 1651084;
        hs0 = -6252;
        hs1 = 450;
    } else if (RP >= 624000) {
        P0 = 78;
        h0 = 2152645;
        hs0 = -6573;
        hs1 = 501;
    } else if (RP >= 584000) {
        P0 = 73;
        h0 = 2680954;
        hs0 = -6934;
        hs1 = 560;
    } else if (RP >= 544000) {
        P0 = 68;
        h0 = 3239426;
        hs0 = -7342;
        hs1 = 632;
    } else if (RP >= 504000) {
        P0 = 63;
        h0 = 3832204;
        hs0 = -7808;
        hs1 = 719;
    } else if (RP >= 464000) {
        P0 = 58;
        h0 = 4464387;
        hs0 = -8345;
        hs1 = 826;
    } else if (RP >= 424000) {
        P0 = 53;
        h0 = 5142359;
        hs0 = -8972;
        hs1 = 960;
    } else if (RP >= 384000) {
        P0 = 48;
        h0 = 5874268;
        hs0 = -9714;
        hs1 = 1131;
    } else if (RP >= 344000) {
        P0 = 43;
        h0 = 6670762;
        hs0 = -10609;
        hs1 = 1354;
    } else if (RP >= 304000) {
        P0 = 38;
        h0 = 7546157;
        hs0 = -11711;
        hs1 = 1654;
    } else if (RP >= 264000) {
        P0 = 33;
        h0 = 8520395;
        hs0 = -13103;
        hs1 = 2072;
    } else {
        P0 = 28;
        h0 = 9622536;
        hs0 = -14926;
        hs1 = 2682;
    }

    dP0 = RP - P0 * 8000;
    HP1 = (hs0 * dP0) >> 1;
    HP2 = (((hs1 * dP0) >> 14) * dP0) >> 4;
    RH = ((HP1 + HP2) >> 8) + h0;

    return (float)RH * 1e-3f - _alt0;
#else
    /* Correct formula */
    return (_alt_k1 * (1 - powf((pressure * _inv_pressZeroLevel), _alt_kpow)));
#endif /* configALTITUDE_KF_USE_APPROX_ALTITUDE */
}

static float altitudeKFAccelDownCalc(axis3f_t accel, float b_az, axis3f_t angles) {
    float accD;
#if defined(configALTITUDE_KF_USE_ACC_D)
    //transform accel_z into accel_D
    accD = (constG - accel.x * SIN(angles.y) + accel.y * COS(angles.y) * SIN(angles.x)
            + (accel.z - b_az) * COS(angles.y) * COS(angles.x));
#else
    accD = (constG + accel.z - b_az);
#endif

#ifdef configALTITUDE_KF_ACC_HP_FILTER
    accD = IIRFilterProcess(&HPFilt_accD, accD);
#endif

    return DEADBAND(accD, configALTITUDE_KF_ACCEL_D_DEADBAND);
}

#ifdef configALTITUDE_KF_USE_VELD_CORRECTION
static float altitudeKFVelDownCalc(axis3f_t vel, axis3f_t angles) {
    float velD;
    //transform vel into vel_D
    velD = -vel.x * SIN(angles.y) + vel.y * COS(angles.y) * SIN(angles.x) + vel.z * COS(angles.y) * COS(angles.x);

    return velD;
}
#endif

/* Functions -----------------------------------------------------------------*/

void altitudeKF_init(altitudeState_t* altState, float pressGround, float tempGround) {

/* Initialize support variables for altitude calculation */
#ifdef configALTITUDE_KF_USE_APPROX_ALTITUDE
    _alt0 = altitudeCalculation(pressGround);
#else
    _inv_pressZeroLevel = 1.f / pressGround;
    _alt_k1 = tempGround / configALTITUDE_KF_CONST_TEMP_RATE;
    _alt_kpow = (configALTITUDE_KF_CONST_R * configALTITUDE_KF_CONST_TEMP_RATE) / (configALTITUDE_KF_CONST_M * constG);
#endif /* configALTITUDE_KF_USE_APPROX_ALTITUDE */

    /* Initialize state */
    altState->RoC = 0;
    altState->alt = 0;
    altState->vAcc = 0;
    altState->b_vAcc = 0;

    /* Initialize downward acceleration high-pass filter */
#ifdef configALTITUDE_KF_ACC_HP_FILTER
    IIRFilterInitHP(&HPFilt_accD, configALTITUDE_KF_ACCEL_HP_FREQ, configALTITUDE_KF_LOOP_TIME_S * 1e3f);
#endif

    /* Initialize derivative calculation for vertical speed estimation */
#if (configUSE_ALT_TOF != configTOF_DISABLE)
    IIRFilterDerivativeInit(&LIDAR_diff, configALTITUDE_KF_LIDAR_DIFF_ND, configALTITUDE_KF_LIDAR_UPDATE_TIME_S * 1e3f);
#endif
    return;
}

void altitudeKF_prediction(altitudeState_t* altState) {
    /* Predict state */
    altState->alt +=
        configALTITUDE_KF_LOOP_TIME_S * altState->RoC
        + 0.5 * configALTITUDE_KF_LOOP_TIME_S * configALTITUDE_KF_LOOP_TIME_S * (altState->vAcc - altState->b_vAcc);

    altState->RoC += configALTITUDE_KF_LOOP_TIME_S * (altState->vAcc - altState->b_vAcc);

    return;
}

void altitudeKF_updateBaroAccel(altitudeState_t* altState, float press, axis3f_t accel, float b_az, axis3f_t angles) {
    /* Calculate delta measures */
    float delta_baroAltitude = altitudeCalculation(press) - altState->alt;
    float delta_accelDown = altitudeKFAccelDownCalc(accel, b_az, angles) + altState->vAcc;

    /* Correct with accelerometer only if measured value is within allowed range */
    if (fabsf(delta_accelDown) > configALTITUDE_KF_MAX_ACCEL_DOWN) {
        delta_accelDown = 0;
    }

    /* Apply correction */
    altState->alt += configALTITUDE_KF_M_HH * delta_baroAltitude - configALTITUDE_KF_M_HA * delta_accelDown;
    altState->RoC += configALTITUDE_KF_M_VH * delta_baroAltitude - configALTITUDE_KF_M_VA * delta_accelDown;
    altState->vAcc += configALTITUDE_KF_M_AH * delta_baroAltitude - configALTITUDE_KF_M_AA * delta_accelDown;
    altState->b_vAcc += configALTITUDE_KF_M_BH * delta_baroAltitude - configALTITUDE_KF_M_BA * delta_accelDown;

    return;
}

#if (configUSE_ALT_TOF != configTOF_DISABLE)
void altitudeKF_updateLIDAR(altitudeState_t* altState, float ToFAlt, axis3f_t angles) {
    /* Differentiate LIDAR reading to obtain vertical speed */
    IIRFilterDerivativeProcess(&LIDAR_diff, (ToFAlt * COS(angles.y) * COS(angles.x)));

    /* Correct with LIDAR only if measured altitude and current attitude are within allowed range */
    if ((LIDAR_diff.output < configALTITUDE_KF_MAX_LIDAR_ROC)
        && (fabsf(angles.x) <= configALTITUDE_KF_MAX_LIDAR_ROLL_PITCH)
        && (fabsf(angles.y) <= configALTITUDE_KF_MAX_LIDAR_ROLL_PITCH)) {
        float delta_LIDARRoC =
            (LIDAR_diff.output - altState->RoC) * configALTITUDE_KF_LIDAR_UPDATE_TIME_S / configALTITUDE_KF_LOOP_TIME_S;
        altState->alt += configALTITUDE_KF_M_HL * delta_LIDARRoC;
        altState->RoC += configALTITUDE_KF_M_VL * delta_LIDARRoC;
        altState->vAcc += configALTITUDE_KF_M_AL * delta_LIDARRoC;
        altState->b_vAcc += configALTITUDE_KF_M_BL * delta_LIDARRoC;
    }
}
#endif

#ifdef configALTITUDE_KF_USE_VELD_CORRECTION
void altitudeKF_updateVelD(altitudeState_t* altState, axis3f_t velocities, axis3f_t angles) {
    float delta_velD = (altitudeKFVelDownCalc(velocities, angles) + altState->RoC);
    altState->alt -= configALTITUDE_KF_M_HV * delta_velD;
    altState->RoC -= configALTITUDE_KF_M_VV * delta_velD;
    altState->vAcc -= configALTITUDE_KF_M_AV * delta_velD;
    altState->b_vAcc -= configALTITUDE_KF_M_BV * delta_velD;
}
#endif

void altitudeKF_reset(altitudeState_t* altState, float pressGround) {
#ifdef configALTITUDE_KF_USE_APPROX_ALTITUDE
    _alt0 = altitudeCalculation(pressGround);
#else
    _inv_pressZeroLevel = 1.f / pressGround;
#endif /* configALTITUDE_KF_USE_APPROX_ALTITUDE */

    /* Initialize state */
    altState->RoC = 0;
    altState->alt = 0;
    altState->vAcc = 0;
    altState->b_vAcc = 0;

    /* Initialize accelerometer HP filter */
#ifdef configALTITUDE_KF_ACC_HP_FILTER
    IIRFilterReset(&HPFilt_accD);
#endif

    /* Initialize LIDAR derivative filter */
#if (configUSE_ALT_TOF != configTOF_DISABLE)
    IIRFilterDerivativeReset(&LIDAR_diff);
#endif
}
