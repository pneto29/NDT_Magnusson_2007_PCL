#include "validationlib.h"
double computeCloudRMS(pcl::PointCloud<pcl::PointXYZ>::ConstPtr target, pcl::PointCloud<pcl::PointXYZ>::ConstPtr source, double max_range){


    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    tree->setInputCloud(target);

    double fitness_score = 0.0;

    std::vector<int> nn_indices (1);
    std::vector<float> nn_dists (1);

    // For each point in the source dataset
    int nr = 0;
    for (size_t i = 0; i < source->points.size (); ++i){
        //Avoid NaN points as they crash nn searches
        if(!pcl_isfinite((*source)[i].x)){
            continue;
        }

        // Find its nearest neighbor in the target
        tree->nearestKSearch (source->points[i], 1, nn_indices, nn_dists);

        // Deal with occlusions (incomplete targets)
        if (nn_dists[0] <= max_range*max_range){
            // Add to the fitness score
            fitness_score += nn_dists[0];
            nr++;
        }
    }

    if (nr > 0){
        //cout << "nr: " << nr << endl;
        //cout << "fitness_score: " << fitness_score << endl;
        return sqrt(fitness_score / nr);
    }else{
        return (std::numeric_limits<double>::max ());
    }
}

int
main (int argc, char** argv)
{
    clock_t tempo;
    tempo = clock();

    // Loading first scan of room.
    pcl::PointCloud<pcl::PointXYZ>::Ptr target_cloud (new pcl::PointCloud<pcl::PointXYZ>);
    if (pcl::io::loadPCDFile<pcl::PointXYZ> (argv[1], *target_cloud) == -1)
    {
        PCL_ERROR ("Couldn't read file room_scan1.pcd \n");
        return (-1);
    }
    std::cout << "Loaded " << target_cloud->size () << " data points from room_scan1.pcd" << std::endl;

    // Loading second scan of room from new perspective.
    pcl::PointCloud<pcl::PointXYZ>::Ptr input_cloud (new pcl::PointCloud<pcl::PointXYZ>);
    if (pcl::io::loadPCDFile<pcl::PointXYZ> (argv[2], *input_cloud) == -1)
    {
        PCL_ERROR ("Couldn't read file room_scan2.pcd \n");
        return (-1);
    }


    // Filtering input scan to roughly 10% of original size to increase speed of registration.
    pcl::PointCloud<pcl::PointXYZ>::Ptr filtered_cloud (new pcl::PointCloud<pcl::PointXYZ>);
    pcl::ApproximateVoxelGrid<pcl::PointXYZ> approximate_voxel_filter;
    approximate_voxel_filter.setLeafSize (0.2, 0.2, 0.2);
    approximate_voxel_filter.setInputCloud (input_cloud);
    approximate_voxel_filter.filter (*filtered_cloud);

    pcl::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ> ndt;


    float step = atof(argv[3]);
    float resolution = atof(argv[4]);
    int iteration = atoi(argv[5]);

    ndt.setStepSize(step); // ORIGINAL POLY
    ndt.setResolution(resolution); // ORIGINAL POLY
    ndt.setMaximumIterations(iteration); // ORIGINAL POLY

    // Setting point cloud to be aligned.
    ndt.setInputSource (filtered_cloud);
    // Setting point cloud to be aligned to.
    ndt.setInputTarget (target_cloud);

   
    // Calculating required rigid transform to align the input cloud to the target cloud.
    pcl::PointCloud<pcl::PointXYZ>::Ptr output_cloud (new pcl::PointCloud<pcl::PointXYZ>);
    ndt.align (*output_cloud);

    double rms = computeCloudRMS(target_cloud, output_cloud, std::numeric_limits<double>::max ());
    std::cout << "RMS: " << rms << std::endl;
    // Transforming unfiltered, input cloud using found transform.
    pcl::transformPointCloud (*input_cloud, *output_cloud, ndt.getFinalTransformation ());
    Eigen::Matrix4f rotation_matrix;
    rotation_matrix=ndt.getFinalTransformation ();
    double elem1, elem2, elem3, angle123,result;
    elem1 = rotation_matrix(0,0);

    elem2 = rotation_matrix(1,1);
    elem3= rotation_matrix(2,2);
    angle123= (elem1+elem2+elem3-1)/2;
    result = acos (angle123) * 180.0 / PI;

    double trans1, trans2, trans3, result2, trans12, trans22, trans32;
    trans1 = rotation_matrix(0,3);
    trans2 = rotation_matrix(1,3);
    trans3 = rotation_matrix(2,3);
    trans12= trans1*trans1;
    trans22= trans2*trans2;
    trans32= trans3*trans3;
    result2=sqrt(trans12+trans22+trans32);
    std::cout << "ROTATION: " << result << std::endl;
    std::cout << "TRANSLATION: " << result2 << std::endl;
    printf("Tempo:%f \n",(clock() - tempo) / (double)CLOCKS_PER_SEC);
    cout << "-------------------------------------------------------" << endl;


    // Initializing point cloud visualizer
    boost::shared_ptr<pcl::visualization::PCLVisualizer>
            viewer_final (new pcl::visualization::PCLVisualizer ("3D Viewer"));
    viewer_final->setBackgroundColor (255, 255, 255);

    // Coloring and visualizing target cloud (red).
    pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ>
            target_color (target_cloud, 0, 0, 255);
    viewer_final->addPointCloud<pcl::PointXYZ> (target_cloud, target_color, "target cloud");
    viewer_final->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE,
                                                    1, "target cloud");

    // Coloring and visualizing transformed input cloud (green).
    pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ>
            output_color (output_cloud, 0, 255, 0);
    viewer_final->addPointCloud<pcl::PointXYZ> (output_cloud, output_color, "output cloud");
    viewer_final->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE,
                                                    1, "output cloud");

    // Starting visualizer
    // viewer_final->addCoordinateSystem (1.0, "global");
    viewer_final->initCameraParameters ();

    // Wait until visualizer window is closed.
    while (!viewer_final->wasStopped ())
    {
        viewer_final->spinOnce (100);
        boost::this_thread::sleep (boost::posix_time::microseconds (100000));
    }

    return (0);
}
