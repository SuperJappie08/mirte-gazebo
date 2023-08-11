#include <ros/ros.h>


#include <iostream>
#include <pcl/ModelCoefficients.h>
#include <pcl/common/common.h>
#include <pcl/features/normal_3d.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/search/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl_conversions/pcl_conversions.h>
#include <prius_msgs/Control.h>
#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <vision_msgs/Detection3DArray.h>
 #include <pcl/io/pcd_io.h>
 #include <pcl/io/ply_io.h>
 #include <pcl/point_cloud.h>
 #include <pcl/console/parse.h>
 #include <pcl/common/transforms.h>
 #include <pcl/visualization/pcl_visualizer.h>
 #include <pcl/filters/passthrough.h>

class pcl_rotate {

public:
   void removePlane(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud) {
    pcl::ModelCoefficients::Ptr coefficients(new pcl::ModelCoefficients);
    pcl::PointIndices::Ptr inliers(new pcl::PointIndices);
    // std::cout << "planeStart" << cloud->size() << std::endl;

    // Create the segmentation object
    pcl::SACSegmentation<pcl::PointXYZ> seg;

    // Optional
    seg.setOptimizeCoefficients(true);

    // Mandatory
    seg.setModelType(pcl::SACMODEL_PLANE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(0.01);

    seg.setInputCloud(cloud);
    seg.segment(*inliers, *coefficients);

    // Filter based on indices
    pcl::ExtractIndices<pcl::PointXYZ> extract;
    extract.setInputCloud(cloud);
    extract.setIndices(inliers);
    extract.setNegative(true);
    extract.filter(*cloud);
    // std::cout << "plane" << cloud->size() << std::endl;
  }
  std::vector<pcl::PointCloud<pcl::PointXYZ>>
  cluster(pcl::PointCloud<pcl::PointXYZ>::Ptr const cloud) {
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(
        new pcl::search::KdTree<pcl::PointXYZ>);
    if (cloud->empty()) {
      return {};
    }
    tree->setInputCloud(cloud);

    std::vector<pcl::PointIndices> cluster_indices;
    pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
    // std::cout << __LINE__ << std::endl;
    ec.setClusterTolerance(0.01); // in meters
    ec.setMinClusterSize(2);
    ec.setMaxClusterSize(25000);
    ec.setSearchMethod(tree);
    ec.setInputCloud(cloud);
    ec.extract(cluster_indices);

    std::vector<pcl::PointCloud<pcl::PointXYZ>> clusters;
    int j = 0;
    for (std::vector<pcl::PointIndices>::const_iterator it =
             cluster_indices.begin();
         it != cluster_indices.end(); ++it) {

      pcl::PointCloud<pcl::PointXYZ> cloud_cluster;
      for (std::vector<int>::const_iterator pit = it->indices.begin();
           pit != it->indices.end(); ++pit)
        cloud_cluster.points.push_back(cloud->points[*pit]);
      cloud_cluster.width = cloud_cluster.points.size();
      cloud_cluster.height = 1;
      cloud_cluster.is_dense = true;

      clusters.push_back(cloud_cluster);
      j++;
    }

    return clusters;
  }
vision_msgs::BoundingBox3D toBoundingBox(pcl::PointCloud<pcl::PointXYZ> const & cloud) {
	vision_msgs::BoundingBox3D result;
	if (cloud.empty()) { return result; }
	// Computes centroid of the cluster
	Eigen::Vector4f centroid;

	pcl::compute3DCentroid(cloud, centroid);

	result.center.position.x = centroid[0];
	result.center.position.y = centroid[1];
	result.center.position.z = centroid[2];

	pcl::PointXYZ min, max;
	pcl::getMinMax3D(cloud, min, max);

	result.size.x = max.x - min.x;
	result.size.y = max.y - min.y;
	result.size.z = max.z - min.z;

	return result;
}


  pcl_rotate(ros::NodeHandle nh) {
    // Topic you publish to
    pub_ = n_.advertise<sensor_msgs::PointCloud2>(
        "/mirte/depth_cam/pointsRot", 1000);
   
    // Topic you subscribe to
    sub_ = n_.subscribe("/mirte/depth_cam/points", 1000,
                        &pcl_rotate::callback, this);
  }
  
  void callback(const sensor_msgs::PointCloud2 &input_cloud) {
    vision_msgs::Detection3DArray detectionArray;
    detectionArray.header =
        input_cloud.header; // Copy the input header to the output header
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(
        new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromROSMsg(input_cloud, *cloud); // Map the input cloud data to a new
    // cloud with the PointCloud format
    // std::cout << __LINE__ << std::endl;
    pcl::PassThrough<pcl::PointXYZ> pass;
  pass.setInputCloud (cloud);
  pass.setFilterFieldName ("z");
  pass.setFilterLimits (0.0, 1.8);
  //pass.setNegative (true);
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered (new pcl::PointCloud<pcl::PointXYZ>);

  pass.filter (*cloud_filtered);

    this->removeNan(cloud_filtered);
  Eigen::Affine3f transform_2 = Eigen::Affine3f::Identity();

  float theta = -M_PI/2;
  transform_2.rotate (Eigen::AngleAxisf (theta, Eigen::Vector3f::UnitX()));
  transform_2.rotate (Eigen::AngleAxisf (-theta, Eigen::Vector3f::UnitY()));

  // Print the transformation
//   printf ("\nMethod #2: using an Affine3f\n");
  // std::cout << transform_2.matrix() << std::endl;

  // Executing the transformation
 pcl::PointCloud<pcl::PointXYZ>::Ptr transformed_cloud (new pcl::PointCloud<pcl::PointXYZ> ());
  // You can either apply transform_1 or transform_2; they are the same
  pcl::transformPointCloud (*cloud_filtered, *transformed_cloud, transform_2);
      sensor_msgs::PointCloud2 out_cloud;
    pcl::toROSMsg(*transformed_cloud, out_cloud);

    pub_.publish(out_cloud);
  }
  void removeNan(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud) {
    cloud->is_dense = false;
    // std::cerr << "orig cloud data: " << cloud->size() << " points" << std::endl;

    boost::shared_ptr<std::vector<int>> indices(new std::vector<int>);
    pcl::removeNaNFromPointCloud(*cloud, *indices);
    pcl::ExtractIndices<pcl::PointXYZ> extract;
    extract.setInputCloud(cloud);
    extract.setIndices(indices);
    extract.setNegative(false);
    extract.filter(*cloud);
    // std::cout << "removed" << indices->size() << std::endl;

    // std::cerr << "Point cloud data: " << cloud->size() << " points"
    //           << std::endl;
  }

private:
  // Create the necessary objects for subscribing and publishing
  ros::NodeHandle n_;
  ros::Publisher pub_;  ros::Publisher pub_2;

  ros::Subscriber sub_;
}; // End of class SubscribeAndPublish


int main (int argc, char **argv)
{
  ros::init(argc, argv, "pcl_pointcloud_rotate");
  ros::NodeHandle nh;

  //Create an object of class pcl_object_detector_sub_and_pub that will take care of everything
  pcl_rotate pcl_object(nh);
  ros::spin();
  return 0;
}