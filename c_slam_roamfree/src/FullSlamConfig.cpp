/*
 * c_slam_roamfree,
 *
 *
 * Copyright (C) 2014 Davide Tateo
 * Versione 1.0
 *
 * This file is part of c_slam_roamfree.
 *
 * c_slam_roamfree is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * c_slam_roamfree is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with c_slam_roamfree.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "FullSlamConfig.h"

#include <vector>

#include "NoiseGeneration.h"

using namespace std;

namespace roamfree_c_slam {

FullSlamConfig::FullSlamConfig() {
  ros::NodeHandle n("~");

  try {
    initROS(n);

    initPose(n);

    initIMU(n);

    initCamera(n);

    initRoamfree(n);
  } catch (runtime_error& e) {
    ROS_FATAL(e.what());
    throw e;
  }

}

void FullSlamConfig::initROS(const ros::NodeHandle& n) {
  n.param<string>("trackedFrame", trackedFrame, "camera_link");
  if (!n.getParam("imuTopic", imuTopic)) {
    throw runtime_error("IMU topic not set");
  }
}

void FullSlamConfig::initPose(const ros::NodeHandle& n) {

  //Init initial pose
  vector<double> x0_std(7);
  if (!n.getParam("initialPose", x0_std) || x0_std.size() != 7) {
    throw runtime_error("Incorrect initial pose or no initial pose specified");
  }

  x0.resize(7);
  x0 << x0_std[0], x0_std[1], x0_std[2], x0_std[3], x0_std[4], x0_std[5], x0_std[6];

  //Init imu pose wrt odometric center
  vector<double> T_O_IMU_std(7);
  if (!n.getParam("T_O_IMU", T_O_IMU_std) || T_O_IMU_std.size() != 7) {
    throw runtime_error("Incorrect imu pose or no imu pose specified");
  }

  T_O_IMU.resize(7);
  T_O_IMU << T_O_IMU_std[0], T_O_IMU_std[1], T_O_IMU_std[2], T_O_IMU_std[3], T_O_IMU_std[4], T_O_IMU_std[5], T_O_IMU_std[6];

  //Init camera pose wrt odometric center
  vector<double> T_O_CAMERA_std(7);
  if (!n.getParam("T_O_CAMERA", T_O_CAMERA_std) || T_O_CAMERA_std.size() != 7) {
    throw runtime_error("Incorrect camera pose or no camera pose specified");
  }

  T_O_CAMERA.resize(7);
  T_O_CAMERA << T_O_CAMERA_std[0], T_O_CAMERA_std[1], T_O_CAMERA_std[2], T_O_CAMERA_std[3], T_O_CAMERA_std[4], T_O_CAMERA_std[5], T_O_CAMERA_std[6];
}

void FullSlamConfig::initIMU(const ros::NodeHandle& n) {
  //Setup accelerometer
  n.param<bool>("isAccBiasFixed", isAccBiasFixed, true);
  ROS_INFO_STREAM(
      "Accelerometer bias" << (isAccBiasFixed ? "" : " not") << " fixed");

  vector<double> accBias_std(3);
  if (n.getParam("accBias", accBias_std) && accBias_std.size() != 3) {
    accBias.resize(3);
    accBias << accBias_std[0], accBias_std[1], accBias_std[2];
  } else {
    accBias.setZero();

    ROS_INFO("No default accelerometer bias");
  }

  if (n.getParam("accStd", acc_stdev)) {
    ROS_INFO_STREAM("Adding noise on accelerometer with std " << acc_stdev);
  } else {
    acc_stdev = 0.0;

    ROS_INFO("No added noise on acceleration");
  }

  double gen_acc_bias_stdev;
  acc_generated_bias.resize(3);
  acc_generated_bias.setZero();

  if (n.getParam("genAccBiasStd", gen_acc_bias_stdev)) {

    add_gaussian_noise(acc_generated_bias.data(), 3, 0.0, gen_acc_bias_stdev);

    ROS_INFO_STREAM(
        "Generating random bias on accelerometer with std: " << acc_generated_bias.transpose());
  } else {

    ROS_INFO("No added bias on acceleration");
  }

  //Setup gyroscope
  n.param<bool>("isGyroBiasFixed", isGyroBiasFixed, true);
  ROS_INFO_STREAM(
      "Gyroscope bias" << (isGyroBiasFixed ? "" : " not") << " fixed");

  vector<double> gyroBias_std(3);
  if (n.getParam("gyroBias", gyroBias_std) && gyroBias_std.size() != 3) {
    gyroBias.resize(3);
    gyroBias << gyroBias_std[0], gyroBias_std[1], gyroBias_std[2];
  } else {
    gyroBias.setZero();

    ROS_INFO("No default gyroscope bias");
  }

  if (n.getParam("gyroStd", gyro_stdev)) {
    ROS_INFO_STREAM("Adding noise on accelerometer with std " << gyro_stdev);
  } else {
    gyro_stdev = 0.0;

    ROS_INFO("No added noise on angular velocity");
  }

  double gen_gyro_bias_stdev;
  gyro_generated_bias.resize(3);
  gyro_generated_bias.setZero();

  if (n.getParam("genGyroBiasStd", gen_gyro_bias_stdev)) {
    add_gaussian_noise(gyro_generated_bias.data(), 3, 0.0, gen_gyro_bias_stdev);

    ROS_INFO_STREAM(
        "Generating random bias on gyroscope with std: " << gyro_generated_bias.transpose());
  } else {

    ROS_INFO("No added bias on angular velocity");
  }

  //Setup imu integration steps
  if (!n.getParam("imu_N", imu_N)) {
    throw runtime_error("parameter imu_N undefined");
  }

  //Setup imu nominal period
  if (!n.getParam("imu_dt", imu_dt)) {
    throw runtime_error("parameter imu_dt undefined");
  }
}

void FullSlamConfig::initCamera(const ros::NodeHandle& n) {
  //Camera calibration
  vector<double> K_std(9);
  if (!n.getParam("K", K_std) || K_std.size() != 9) {
    throw runtime_error("Incorrect initial pose or no initial pose specified");
  }

  if (n.getParam("pixelStd", pixel_stdev)) {
    ROS_INFO_STREAM("Adding noise on pixels with std " << pixel_stdev);
  } else {
    acc_stdev = 0.0;

    ROS_INFO("No added noise on pixels");
  }

  K.resize(9);
  K << K_std[0], K_std[1], K_std[2], K_std[3], K_std[4], K_std[5], K_std[6], K_std[7], K_std[8];

  //Set initial scale
  n.param<double>("initialScale", initialScale, 4.0);
  ROS_INFO_STREAM("Initial scale: " << initialScale);

  //Set minimum window length in seconds
  n.param<double>("minWindowLenghtSecond", minWindowLenghtSecond, 2.0);
  ROS_INFO_STREAM("minimum window length " << minWindowLenghtSecond << "s");

  //Set minimum window length in seconds
  n.param<int>("minActiveFeatures", minActiveFeatures, 3);
  ROS_INFO_STREAM("minimum number of active features " << minActiveFeatures);
}

void FullSlamConfig::initRoamfree(const ros::NodeHandle& n) {
  n.param<bool>("useGaussNewton", useGaussNewton, true);

  ROS_INFO_STREAM(
      "Using " << (useGaussNewton ? "Gauss-Newton" : "Levenberg–Marquardt") << " Optimization algorithm");

  n.param<int>("iterationN", iterationN, 1);
  ROS_INFO_STREAM(
      "Using " << iterationN << " iterations for each optimization");
}

}
