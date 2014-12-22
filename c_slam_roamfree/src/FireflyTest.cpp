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

#include <vector>
#include <sstream>

#include <ros/ros.h>

#include <sensor_msgs/Imu.h>
#include <c_slam_msgs/TrackedObject.h>

#include <tf/transform_broadcaster.h>
#include <tf_conversions/tf_eigen.h>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <visualization_msgs/Marker.h>

using namespace std;

class RosPublisher {
public:

	RosPublisher() {
		setTracks();

		trackPublisher = n.advertise<c_slam_msgs::TrackedObject>("/tracks", 6000);
		markersPublisher = n.advertise<visualization_msgs::Marker>("/visualization/features", 1);

		gtSubscriber = n.subscribe("/firefly/ground_truth/pose", 1000,
				&RosPublisher::publishTracks, this);

		K << 565.59102697808, 0.0, 337.839450567586, //
		0.0, 563.936510489792, 199.522081717361, //
		0.0, 0.0, 1.0;
	}

	void publishTracks(const geometry_msgs::PoseWithCovarianceStamped& pose_msg) {

		tf::Transform t_wr(
				tf::Quaternion(pose_msg.pose.pose.orientation.x,
						pose_msg.pose.pose.orientation.y, pose_msg.pose.pose.orientation.z,
						pose_msg.pose.pose.orientation.w),
				tf::Vector3(pose_msg.pose.pose.position.x,
						pose_msg.pose.pose.position.y, pose_msg.pose.pose.position.z));

		tf::Transform t_rc(tf::Quaternion(-0.5, 0.5, -0.5, 0.5),
				tf::Vector3(0.0, 0.0, 0.0));

		tf::Transform t_wc = t_wr * t_rc;
		for (int i = 0; i < tracks.size(); i++) {
			c_slam_msgs::TrackedObject msg;

			msg.id = i;
			msg.imageStamp = pose_msg.header.stamp;

			// check if the track center of mass is visible

			tf::Vector3 track_w(tracksCM[i](0), tracksCM[i](1), tracksCM[i](2));
			tf::Vector3 track_c = t_wc.inverse() * track_w;

			Eigen::Vector3d track_c_eig;

			tf::vectorTFToEigen(track_c, track_c_eig);

			if (pointVisibile(track_c_eig)) {

				for (int j = 0; j < tracks[i].size(); j++) {

					tf::Vector3 track_pt_w(tracks[i][j](0), tracks[i][j](1),
							tracks[i][j](2));
					tf::Vector3 track_pt_c = t_wc.inverse() * track_pt_w;

					Eigen::Vector3d track_pt_c_eig;

					tf::vectorTFToEigen(track_pt_c, track_pt_c_eig);

					Eigen::Vector3d homogeneusPoint = K * track_pt_c_eig;

					homogeneusPoint /= homogeneusPoint(2);
					geometry_msgs::Point32 point;

					point.x = homogeneusPoint(0);
					point.y = homogeneusPoint(1);

					msg.polygon.points.push_back(point);
				}

				trackPublisher.publish(msg);
			}
		}

		br.sendTransform(
				tf::StampedTransform(t_wc, ros::Time::now(), "world", "camera_gt"));

		publishGroundTruthLandmark();

	}

	void publishGroundTruthLandmarkTF() {
		for (int i = 0; i < tracksCM.size(); i++) {
			stringstream ss;
			ss << "Track_" << i;
			tf::Transform trasform;
			tf::Vector3 t_m_tf;

			tf::vectorEigenToTF(tracksCM[i], t_m_tf);

			trasform.setOrigin(t_m_tf);
			trasform.setRotation(tf::Quaternion::getIdentity());
			br.sendTransform(
					tf::StampedTransform(trasform, ros::Time::now(), "world", ss.str()));
		}
	}

	void publishGroundTruthLandmark() {
		visualization_msgs::Marker msg;

		msg.header.stamp = ros::Time::now();
		msg.header.frame_id = "/world";
		msg.type = visualization_msgs::Marker::CUBE_LIST;
		//msg.lifetime = ros::Duration(0.2);
		msg.frame_locked = false;
		msg.ns = "roamfree_visualodometry_gt";
		msg.id = 1;
		msg.action = visualization_msgs::Marker::ADD;

		msg.color.r = 0.0;
		msg.color.g = 1.0;
		msg.color.b = 0.0;
		msg.color.a = 1.0;

		msg.scale.x = 0.10;
		msg.scale.y = 0.10;
		msg.scale.z = 0.40;

		msg.pose.position.x = 0.0;
		msg.pose.position.y = 0.0;
		msg.pose.position.z = 0.0;

		msg.pose.orientation.w = 1.0;

		msg.points.resize(tracksCM.size());

		for (int i = 0; i < tracksCM.size(); i++) {
			msg.points[i].x = tracksCM[i](0);
			msg.points[i].y = tracksCM[i](1);
			msg.points[i].z = tracksCM[i](2);
		}

		markersPublisher.publish(msg);
	}

private:
	bool pointVisible(Eigen::Vector4d& trackPoint, Eigen::Matrix4d H_CW) {
		Eigen::Vector4d trackRC = H_CW * trackPoint;
		trackRC /= trackRC(3);

		Eigen::Vector3d projection = K * H_CW.topRows(3) * trackPoint;
		projection /= projection(2);

		return trackRC(2) > 0 && projection(0) >= 0 && projection(0) < 640
				&& projection(1) >= 0 && projection(1) < 360;
	}

	bool pointVisibile(Eigen::Vector3d & trackPointInCamera) {
		Eigen::Vector3d projection = K * trackPointInCamera;
		projection /= projection(2);

		return trackPointInCamera(2) > 0 && projection(0) >= 0
				&& projection(0) < 640 && projection(1) >= 0 && projection(1) < 360;
	}

	bool trackVisible(vector<Eigen::Vector4d>& track,
			const Eigen::Matrix4d& H_CW) {
		for (int i = 0; i < track.size(); i++) {
			if (pointVisible(track[i], H_CW)) {
				return true;
			}
		}

		return false;

	}

	void setTracks() {

		/*
		 const int numTracks = 16;

		 double cm[][3] = { { 2.0, -2.0, -0.3+1.0 }, { 2.0, -1.0, 0.3+1.0 },
		 { 2.0, 0.0, -0.3+1.0 }, { 2.0, 1.0, 0.3+1.0 }, { 2.0, 2.0, -0.3+1.0 },

		 { -2.0, -2.0, -0.3+1.0 }, { -2.0, -1.0, 0.3+1.0 }, { -2.0, 0.0, -0.3+1.0 }, { -2.0,
		 1.0, 0.3+1.0 }, { -2.0, 2.0, -0.3+1.0 },

		 { -1.0, 2.0, 0.3+1.0 }, { -0.0, 2.0, -0.3+1.0 }, { 1.0, 2.0, 0.3+1.0 },

		 { -1.0, -2.0, 0.3+1.0 }, { 0.0, -2.0, -0.3+1.0 }, { 1.0, -2.0, 0.3+1.0 } };

		 tracks.resize(numTracks);

		 for (int k = 0; k < numTracks; k++) {
		 Eigen::Vector3d trackCM;
		 trackCM << cm[k][0], cm[k][1], cm[k][2];
		 tracksCM.push_back(trackCM);

		 for (int j = 0; j < 4; j++) {
		 Eigen::Vector4d track;
		 track << cm[k][0], cm[k][1], cm[k][2], 1;

		 cout << track(0) << "," << track(1) << "," << track(2) << ";";
		 cout << endl;
		 tracks[k].push_back(track);
		 }
		 }
		 //*/

		// generate a carpet of tracks in the area [-2.5 12.5] X [-2.5 7.5]
		double x = -20;
		while (x < 25.0) {
			double y = -20;

			while (y < 25.0) {
				Eigen::Vector4d track;
				track << x, y, 0.0, 1.0;

				tracks.resize(tracks.size() + 1);

				for (int k = 0; k < 4; k++) {  // four superimposed points
					tracks[tracks.size() - 1].push_back(track);
				}

				Eigen::Vector3d trackCM;
				trackCM = track.head(3);
				tracksCM.push_back(trackCM);

				y += 5.0;
			}

			x += 5.0;
		}

		//*/

	}

private:
	ros::NodeHandle n;
	ros::Publisher trackPublisher, markersPublisher;
	ros::Subscriber gtSubscriber;
	tf::TransformBroadcaster br;

	//tracks
	vector<vector<Eigen::Vector4d> > tracks;
	vector<Eigen::Vector3d> tracksCM;

	Eigen::Matrix3d K;
};

int main(int argc, char *argv[]) {
	ros::init(argc, argv, "circular_test");

	RosPublisher publisher;

	ros::spin();

	return 0;
}
