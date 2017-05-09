#include <ros/ros.h>
#include <kdl/chainfksolverpos_recursive.hpp>

#include "react_controller/baxterChain.h"

using namespace Eigen;
using namespace   std;

/**************************************************************************/
/*                            BaxterChain                                 */
/**************************************************************************/

BaxterChain::BaxterChain(): nrOfJoints(0), nrOfSegments(0), segments(0)
{

}

BaxterChain::BaxterChain(const KDL::Chain& in): BaxterChain()
{
    for(size_t i=0; i<in.getNrOfSegments(); ++i)
    {
        this->addSegment(in.getSegment(i));
    }
}

BaxterChain::BaxterChain(urdf::Model _robot_model,
                         const string& _base_link, const string& _tip_link):
                         BaxterChain()
{
    // Read joints and links from URDF
    ROS_INFO("Reading joints and links from URDF, from %s link to %s link",
                                    _base_link.c_str(), _tip_link.c_str());

    KDL::Tree tree;
    if (not kdl_parser::treeFromUrdfModel(_robot_model, tree))
    {
        ROS_FATAL("Failed to extract KDL tree from xml robot description");
    }

    KDL::Chain chain;
    if (not tree.getChain(_base_link, _tip_link, chain))
    {
        ROS_FATAL("Couldn't find chain %s to %s",_base_link.c_str(),_tip_link.c_str());
    }
    *this = chain;

    // Read upper and lower bounds
    std::vector<KDL::Segment> kdl_chain_segs = segments;

    boost::shared_ptr<const urdf::Joint> joint;

    std::vector<double> l_bounds, u_bounds;

    uint joint_num=0;

    for (size_t i = 0; i < kdl_chain_segs.size(); ++i)
    {
        joint = _robot_model.getJoint(kdl_chain_segs[i].getJoint().getName());

        if (joint->type != urdf::Joint::UNKNOWN && joint->type != urdf::Joint::FIXED)
        {
            joint_num++;
            float lower, upper;
            int hasLimits = 0;

            if (joint->type != urdf::Joint::CONTINUOUS )
            {
                if (joint->safety)
                {
                    lower = std::max(joint->limits->lower, joint->safety->soft_lower_limit);
                    upper = std::min(joint->limits->upper, joint->safety->soft_upper_limit);
                }
                else
                {
                    lower = joint->limits->lower;
                    upper = joint->limits->upper;
                }

                hasLimits = 1;
            }

            if (hasLimits)
            {
                lb(joint_num-1)=lower;
                ub(joint_num-1)=upper;
            }
            else
            {
                lb(joint_num-1)=std::numeric_limits<float>::lowest();
                ub(joint_num-1)=std::numeric_limits<float>::max();
            }

            ROS_DEBUG_STREAM("IK Using joint "<<joint->name<<" "<<
                              lb(joint_num-1)<<" "<<ub(joint_num-1));
        }
    }

    // Assign default values for q
    for (size_t i = 0; i < getNrOfJoints(); ++i)
    {
        // This will initialize the joint in the
        // middle of its operational range
        q[i] = (lb[i]+ub[i])/2;
    }
}

BaxterChain::BaxterChain(urdf::Model _robot_model,
                         const string& _base_link, const string& _tip_link,
                         std::vector<double> _q_0):
                         BaxterChain(_robot_model, _base_link, _tip_link)

{
    // TODO : instead of assert, just
    // place a ROS_ERROR and fill q with defaults.
    ROS_ASSERT(getNrOfJoints() == _q_0.size());

    for (size_t i = 0; i < getNrOfJoints(); ++i)
    {
        q[i] = _q_0[i];
    }
}

bool BaxterChain::resetChain()
{
    nrOfJoints=0;
    nrOfSegments=0;
    segments.resize(0);
    q.resize(0);
    lb.resize(0);
    ub.resize(0);

    return true;
}

BaxterChain::operator KDL::Chain()
{
    KDL::Chain res;

    for (size_t i = 0; i < getNrOfSegments(); ++i)
    {
        res.addSegment(getSegment(i));
    }

    return res;
}

BaxterChain& BaxterChain::operator=(const KDL::Chain& _ch)
{
    resetChain();
    for(size_t i=0; i<_ch.getNrOfSegments(); ++i)
    {
        addSegment(_ch.getSegment(i));
    }

    for (size_t i = 0; i < getNrOfJoints(); ++i)
    {
        q[i]  = 0;
        lb[i] = 0;
        ub[i] = 0;
    }

    return *this;
}

BaxterChain& BaxterChain::operator=(const BaxterChain& _ch)
{
    // self-assignment check
    if (this != &_ch)
    {
        resetChain();
        for(size_t i=0; i<_ch.getNrOfSegments(); ++i)
        {
            addSegment(_ch.getSegment(i));
        }

        for (size_t i = 0; i < getNrOfJoints(); ++i)
        {
            q[i]  = _ch.q[i];
            lb[i] = _ch.lb[i];
            ub[i] = _ch.ub[i];
        }
    }

    return *this;
}

void BaxterChain::addSegment(const KDL::Segment& segment)
{
    segments.push_back(segment);
    nrOfSegments++;
    if(segment.getJoint().getType()!=KDL::Joint::None)
    {
        nrOfJoints++;

        lb.conservativeResize(getNrOfJoints());
        ub.conservativeResize(getNrOfJoints());
         q.conservativeResize(getNrOfJoints());
    }
}

void BaxterChain::addChain(const KDL::Chain& chain)
{
    for(size_t i=0; i<chain.getNrOfSegments(); ++i)
    {
        this->addSegment(chain.getSegment(i));
    }
}

const KDL::Segment& BaxterChain::getSegment(size_t nr)const
{
    return segments[nr];
}

MatrixXd BaxterChain::GeoJacobian()
{
    KDL::Jacobian J;
    J.resize(getNrOfJoints());
    KDL::JntArray jnts(getNrOfJoints());

    for (size_t i = 0, _i = getNrOfJoints(); i < _i; ++i)
    {
        jnts(i) = q[i];
    }

    JntToJac(jnts, J);
    return J.data;
}

VectorXd BaxterChain::getAng()
{
    return Map<VectorXd>(q.data(), q.size());
}

bool BaxterChain::setAng(sensor_msgs::JointState _q)
{
    if (_q.position.size() != getNrOfJoints()) { return false; }

    std::vector<double> angles;
    for (size_t i = 0; i < getNrOfJoints(); ++i)
    {
        angles.push_back(_q.position[i]);
    }
    setAng(angles);
    return true;
}

bool BaxterChain::setAng(Eigen::VectorXd _q)
{
    if (_q.size() != int(getNrOfJoints()))     { return false; }

    q = _q;
    return true;
}

bool BaxterChain::setAng(std::vector<double> _q)
{
    if (_q.size() != getNrOfJoints())          { return false; }

    q = Map<VectorXd>(_q.data(), _q.size());
    return true;
}

bool BaxterChain::JntToCart(const KDL::JntArray& _q_in, KDL::Frame& _p_out, int seg_nr)
{
    size_t segmentNr;
    if (seg_nr<0) { segmentNr = getNrOfSegments(); }
    else          { segmentNr =            seg_nr; }

    _p_out = KDL::Frame::Identity();

    // if      (_q_in.rows()!=getNrOfJoints()) { return false; }
    if (segmentNr>getNrOfSegments())   { return false; }
    else
    {
        int j=0;
        for (size_t i=0; i<segmentNr; ++i)
        {
            if (getSegment(i).getJoint().getType()!=KDL::Joint::None)
            {
                _p_out = _p_out*getSegment(i).pose(_q_in(j));
                j++;
            }
            else
            {
                _p_out = _p_out*getSegment(i).pose(0.0);
            }
        }
        return true;
    }
}

bool BaxterChain::JntToJac(const KDL::JntArray& q_in, KDL::Jacobian& jac, int seg_nr)
{
    size_t segmentNr;
    if (seg_nr<0) { segmentNr = getNrOfSegments(); }
    else          { segmentNr =            seg_nr; }

    //Initialize Jacobian to zero since only segmentNr columns are computed
    SetToZero(jac) ;

    if (q_in.rows()!=getNrOfJoints()||getNrOfJoints()!=jac.columns()) { return false; }
    else if (segmentNr>getNrOfSegments())                             { return false; }

    KDL::Frame T_tmp(KDL::Frame::Identity());
    KDL::Frame total(KDL::Frame::Identity());

    KDL::Twist t_tmp;
    SetToZero(t_tmp);

    int j=0, k=0;

    for (size_t i=0; i<segmentNr; ++i)
    {
        //Calculate new Frame_base_ee
        if (getSegment(i).getJoint().getType()!=KDL::Joint::None)
        {
            //pose of the new end-point expressed in the base
            total = T_tmp*getSegment(i).pose(q_in(j));
            //changing base of new segment's twist to base frame if it is not locked
            //t_tmp = T_tmp.M*chain.getSegment(i).twist(1.0);
            t_tmp = T_tmp.M*getSegment(i).twist(q_in(j),1.0);
        }
        else
        {
            total = T_tmp*getSegment(i).pose(0.0);
        }

        //Changing Refpoint of all columns to new ee
        changeRefPoint(jac,total.p-T_tmp.p,jac);

        //Only increase jointnr if the segment has a joint
        if (getSegment(i).getJoint().getType()!=KDL::Joint::None)
        {
            //Only put the twist inside if it is not locked
            jac.setColumn(k++,t_tmp);
            j++;
        }

        T_tmp = total;
    }

    return true;
}

bool BaxterChain::GetJointPositions(std::vector<Eigen::Vector3d>& positions)
{
    Eigen::Vector3d point(0.40, -0.25, 0.45);

    size_t segmentNr=getNrOfSegments();

    KDL::JntArray jnts(getNrOfJoints());

    for (size_t i = 0; i < getNrOfJoints() - 1; ++i)
    {
        jnts(i) = q[i];
    }

    int j=0;
    KDL::Frame frame(KDL::Frame::Identity());

    for (size_t i=0; i<segmentNr; ++i)
    {
        if (getSegment(i).getJoint().getType()!=KDL::Joint::None)
        {
            frame = frame*getSegment(i).pose(jnts(j));
            KDL::Vector   posKDL = frame.p;
            Eigen::Vector3d posEig;
            tf::vectorKDLToEigen(posKDL, posEig);
            positions.push_back(posEig);
            j++;
        }
        else
        {
            frame = frame*getSegment(i).pose(0.0);
        }
    }

    // for (size_t i = 0; i < positions.size(); ++i)
    // {
    //     ROS_INFO("joint %zu at x: %g y: %g z: %g", i, positions[i](0), positions[i](1), positions[i](2));
    // }

    // std::vector<Eigen::Vector3d> coll_points;
    // std::vector<Eigen::Vector3d> norms;
    // for (size_t i = 0; i < joint_pos.size() - 1; ++i)
    // {
    //     Eigen::Vector3d ab = joint_pos[i + 1] - joint_pos[i];
    //     Eigen::Vector3d ap = point - joint_pos[i];
    //     Eigen::Vector3d coll_pt = joint_pos[i] + ((ap).dot(ab)) / ((ab).dot(ab)) * ab;
    //     coll_points.push_back(coll_pt);
    //     norms.push_back(point - coll_pt);
    //     ROS_INFO("coll point %zu at x:%f y:%f z:%f", i, coll_points[i](0), coll_points[i](1), coll_points[i](2));
    //     ROS_INFO("norm %zu at x:%f y:%f z:%f", i, norms[i](0), norms[i](1), norms[i](2));
    // }

    return true;
}

geometry_msgs::Pose BaxterChain::getPose()
{
    geometry_msgs::Pose result;

    Matrix4d H = getH();
    result.position.x = H(0,3);
    result.position.y = H(1,3);
    result.position.z = H(2,3);

    Quaterniond o(H.block<3,3>(0,0));
    result.orientation.x = o.x();
    result.orientation.y = o.y();
    result.orientation.z = o.z();
    result.orientation.w = o.w();

    return result;
}

Matrix4d BaxterChain::getH()
{
    return getH(q.size() - 1);
}

Matrix4d BaxterChain::getH(const size_t _i)
{
    //num joints in chain
    size_t num_joints = getNrOfJoints();

    // TODO also here, remove the assert, place a ROS_ERROR, and return
    // if i > than num_joints
    ROS_ASSERT_MSG(_i < num_joints, "_i %lu, num_joints %lu", _i, num_joints);

    KDL::JntArray jnts(_i + 1);

    for (size_t i = 0; i < _i + 1; ++i)
    {
        jnts(i) = q[i];
    }

    KDL::Frame frame;

    size_t seg_nr = 0;
    if (_i + 1 == getNrOfJoints()) { seg_nr = getNrOfSegments(); }
    else
    {
        size_t jnt_counter = 0;
        while (jnt_counter < _i + 1)
        {
            if (getSegment(seg_nr++).getJoint().getType() != KDL::Joint::None)
            {
                jnt_counter++;
            }
        }
    }

    JntToCart(jnts,frame, seg_nr);

    return KDLFrameToEigen(frame);
}

void BaxterChain::removeSegment()
{
    if(segments.back().getJoint().getType()!=KDL::Joint::None)
    {
        --nrOfJoints;
        lb.conservativeResize(getNrOfJoints());
        ub.conservativeResize(getNrOfJoints());
        q.conservativeResize(getNrOfJoints());
    }
    segments.pop_back();
    --nrOfSegments;

    return;
}

void BaxterChain::removeJoint()
{
    while(true)
    {
        if(segments.back().getJoint().getType()!=KDL::Joint::None)
        {
            removeSegment();
            break;
        }
        else
        {
            removeSegment();
        }
    }

    return;
}

double BaxterChain::getMax(const size_t _i)
{
    return ub[_i];
}

double BaxterChain::getMin(const size_t _i)
{
    return lb[_i];
}

BaxterChain::~BaxterChain()
{
    return;
}
