//path_service:
// example showing how to receive a nav_msgs/Path request
// run with complementary path_client
// responds immediately to ack new path...but execution takes longer

// this is a crude service; just assumes robot initial pose is 0,
// and all subgoals are expressed with respect to this initial frame.
// i.e., equivalent to expressing subgoals in odom frame

#include <ros/ros.h>
#include <mobot_path_service/MyPathSrv.h>
#include <nav_msgs/Path.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/Twist.h>
#include <iostream>
#include <string>
#include <math.h>
using namespace std;
//some tunable constants, global
const double g_move_speed = 1.0; // set forward speed to this value, e.g. 1m/s
const double g_spin_speed = 1.0; // set yaw rate to this value, e.g. 1 rad/s
const double g_sample_dt = 0.01;

//global variables, including a publisher object
geometry_msgs::Twist g_twist_cmd;
ros::Publisher g_twist_commander; //global publisher object
geometry_msgs::Pose g_current_pose; // not really true--should get this from odom 


// here are a few useful utility functions:
double sgn(double x);
double min_spin(double spin_angle);
double convertPlanarQuat2Phi(geometry_msgs::Quaternion quaternion);
geometry_msgs::Quaternion convertPlanarPhi2Quaternion(double phi);

void do_halt();
void do_move(double distance);
void do_spin(double spin_ang);

//signum function: strip off and return the sign of the argument
double sgn(double x) { 
    if (x>0.0) {return 1.0;}
    else if (x<0.0) {return -1.0;}
    else {return 0.0;}
}

//a function to consider periodicity and find min delta angle
double min_spin(double spin_angle) {
    if (spin_angle > M_PI) {
        spin_angle -= 2.0*M_PI;
    }
    if (spin_angle < -M_PI) {
        spin_angle += 2.0*M_PI;
    }
    return spin_angle;   
}            

// a useful conversion function: from quaternion to yaw
double convertPlanarQuat2Phi(geometry_msgs::Quaternion quaternion) {
    double quat_z = quaternion.z;
    double quat_w = quaternion.w;
    double phi = 2.0 * atan2(quat_z, quat_w); // cheap conversion from quaternion to heading for planar motion
    return phi;
}

//and the other direction:
geometry_msgs::Quaternion convertPlanarPhi2Quaternion(double phi) {
    geometry_msgs::Quaternion quaternion;
    quaternion.x = 0.0;
    quaternion.y = 0.0;
    quaternion.z = sin(phi / 2.0);
    quaternion.w = cos(phi / 2.0);
    return quaternion;
}

// a few action functions:
//a function to reorient by a specified angle (in radians), then halt
void do_spin(double spin_ang) {
    ros::Rate loop_timer(1/g_sample_dt);
    double timer = 0.0;
    double final_time = fabs(spin_ang)/g_spin_speed;
    g_twist_cmd.angular.z = sgn(spin_ang)*g_spin_speed;
    while(timer < final_time) {
          g_twist_commander.publish(g_twist_cmd);
          timer += g_sample_dt;
          loop_timer.sleep(); 
    }  
    do_halt(); 
}

//a function to move forward by a specified distance (in meters), then halt
void do_move(double distance) { // always assumes robot is already oriented properly
                                // but allow for negative distance to mean move backwards
    ros::Rate loop_timer(1/g_sample_dt);
    double timer=0.0;
    double final_time = fabs(distance)/g_move_speed;
    g_twist_cmd.angular.z = 0.0; //stop spinning
    g_twist_cmd.linear.x = sgn(distance)*g_move_speed;
    while(timer < final_time) {
          g_twist_commander.publish(g_twist_cmd);
          timer += g_sample_dt;
          loop_timer.sleep(); 
    }  
    do_halt();
}

void do_halt() {
    ros::Rate loop_timer(1/g_sample_dt);   
    g_twist_cmd.angular.z = 0.0;
    g_twist_cmd.linear.x = 0.0;
    for (int i = 0; i < 10; i++) {
        g_twist_commander.publish(g_twist_cmd);
        loop_timer.sleep(); 
    }   
}


bool callback(mobot_path_service::MyPathSrvRequest& request, mobot_path_service::MyPathSrvResponse& response) {
    ROS_INFO("callback activated");
    double yaw_desired, yaw_current, spin_angle;
    //double px_desired, py_desired, pz_desired, px_current, py_current, pz_current; 
    double travel_distance;
    geometry_msgs::Pose pose_desired;
    int npts = request.nav_path.poses.size();
    ROS_INFO("received path request with %d poses",npts);    
    
    for (int i=0;i<npts;i++) { //visit each subgoal
        // odd notation: drill down, access vector element, drill some more to get pose
        pose_desired = request.nav_path.poses[i].pose; //get first pose from vector of poses
        
        // a quaternion is overkill for navigation in a plane; really only need a heading angle
        // this yaw is measured CCW from x-axis
        yaw_desired = convertPlanarQuat2Phi(pose_desired.orientation); //from i'th desired pose
        ROS_INFO("pose %d: desired yaw = %f",i,yaw_desired);        
        yaw_current = convertPlanarQuat2Phi(g_current_pose.orientation); //our current yaw--should use a sensor
        spin_angle = yaw_desired - yaw_current; // spin this much
        spin_angle = min_spin(spin_angle);// but what if this angle is > pi?  then go the other way
        do_spin(spin_angle); // carry out this incremental action
        // we will just assume that this action was successful--really should have sensor feedback here
        g_current_pose.orientation = pose_desired.orientation; // assumes got to desired orientation precisely
        
        // move forward 1m...just for illustration; SHOULD compute this from subgoal pose      
        // px_desired = pose_desired.position.x; //our current position x--should use a sensor
        // py_desired = pose_desired.position.y; //our current position x--should use a sensor
        // pz_desired = pose_desired.position.z; //our current position x--should use a sensor
        // px_current = g_current_pose.position.x;
        // py_current = g_current_pose.position.y;
        // pz_current = g_current_pose.position.z;
        // travel_distance = (px_desired - px_current)*(px_desired - px_current)
        //                     + (py_desired - py_current)*(py_desired - py_current)
        //                     + (pz_desired - pz_current)*(pz_desired - pz_current);
        //travel_distance = sqrt(travel_distance);
        travel_distance = pose_desired.position.y;
        do_move(travel_distance); // carry out this incremental action
        // we will just assume that this action was successful--really should have sensor feedback here
        g_current_pose.position = pose_desired.position; // assumes got to desired orientation precisely
    }

    return true;
}

void do_inits(ros::NodeHandle &n) {
    //initialize components of the twist command global variable
    g_twist_cmd.linear.x = 0.0;
    g_twist_cmd.linear.y = 0.0;    
    g_twist_cmd.linear.z = 0.0;
    g_twist_cmd.angular.x = 0.0;
    g_twist_cmd.angular.y =0.0;
    g_twist_cmd.angular.z = 0.0;  
    
    //define initial position to be 0
    g_current_pose.position.x = 0.0;
    g_current_pose.position.y = 0.0;
    g_current_pose.position.z = 0.0;
    
    // define initial heading to be "0"
    g_current_pose.orientation.x = 0.0;
    g_current_pose.orientation.y = 0.0;
    g_current_pose.orientation.z = 0.0;
    g_current_pose.orientation.w = 1.0;
    
    // we declared g_twist_commander as global, but never set it up; do that now that we have a node handle
    g_twist_commander = n.advertise<geometry_msgs::Twist>("/cmd_vel", 1);    
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "mobot_path_service");
    ros::NodeHandle n;

    // to clean up "main", do initializations in a separate function
    // a poor-man's class constructor
    do_inits(n); //pass in a node handle so this function can set up publisher with it

    // establish a service to receive path commands
    ros::ServiceServer service = n.advertiseService("mobot_path_service", callback);
    ROS_INFO("Ready to accept paths.");
    ros::spin(); //callbacks do all the work now

    return 0;
}
