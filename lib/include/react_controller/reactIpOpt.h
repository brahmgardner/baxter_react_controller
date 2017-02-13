#ifndef __REACTIPOPT_H__
#define __REACTIPOPT_H__

#include <IpTNLP.hpp>
#include <IpIpoptApplication.hpp>
#include <Eigen/Dense>
#include <kdl/chain.hpp>
#include <kdl/chainfksolver.hpp>
#include <kdl_parser/kdl_parser.hpp>
#include <urdf/model.h>
#include <math.h>

#include <react_controller/baxterChain.h>

using namespace std;
using namespace Eigen;

#define CTRL_RAD2DEG (180.0 / M_PI)
#define CTRL_DEG2RAD (M_PI/180.0)

/****************************************************************/
class ControllerNLP : public Ipopt::TNLP
{
    BaxterChain chain;
    KDL::JntArray lb, ub;
    bool hitting_constraints;
    bool orientation_control;

    VectorXd xr,pr;
    MatrixXd Hr,skew_nr,skew_sr,skew_ar;
    MatrixXd q_lim,v_lim;
    VectorXd p0, q0, v0, v;
    MatrixXd H0,R0,He,J0_xyz,J0_ang,Derr_ang;
    VectorXd err_xyz,err_ang;
    MatrixXd bounds;
    double dt;

    double shou_m,shou_n;
    double elb_m,elb_n;

    VectorXd qGuard;
    VectorXd qGuardMinExt;
    VectorXd qGuardMinInt;
    VectorXd qGuardMinCOG;
    VectorXd qGuardMaxExt;
    VectorXd qGuardMaxInt;
    VectorXd qGuardMaxCOG;

    /****************************************************************/
    void computeSelfAvoidanceConstraints();
    void computeGuard();
    void computeBounds();
    MatrixXd v2m(const VectorXd &x);
    MatrixXd skew(const VectorXd &w);

    public:
    ControllerNLP(BaxterChain chain_, KDL::JntArray &lb_, KDL::JntArray &ub_);
    void set_xr(const VectorXd &xr);
    void set_v_limInDegPerSecond(const MatrixXd &v_lim);
    void set_hitting_constraints(const bool _hitting_constraints);
    void set_orientation_control(const bool _orientation_control);
    void set_dt(const double dt);
    void set_v0InDegPerSecond(const VectorXd &v0);
    void init();
    VectorXd get_resultInDegPerSecond() const;
    // Property getParameters() const;
    bool get_nlp_info(Ipopt::Index &n, Ipopt::Index &m, Ipopt::Index &nnz_jac_g,
                      Ipopt::Index &nnz_h_lag, IndexStyleEnum &index_style);
    bool get_bounds_info(Ipopt::Index n, Ipopt::Number *x_l, Ipopt::Number *x_u,
                         Ipopt::Index m, Ipopt::Number *g_l, Ipopt::Number *g_u);
    bool get_starting_point(Ipopt::Index n, bool init_x, Ipopt::Number *x,
                            bool init_z, Ipopt::Number *z_L, Ipopt::Number *z_U,
                            Ipopt::Index m, bool init_lambda, Ipopt::Number *lambda);
    void computeQuantities(const Ipopt::Number *x, const bool new_x);
    bool eval_f(Ipopt::Index n, const Ipopt::Number *x, bool new_x, Ipopt::Number &obj_value);
    bool eval_grad_f(Ipopt::Index n, const Ipopt::Number* x, bool new_x, Ipopt::Number *grad_f);
    bool eval_g(Ipopt::Index n, const Ipopt::Number *x, bool new_x,Ipopt::Index m, Ipopt::Number *g);
    bool eval_jac_g(Ipopt::Index n, const Ipopt::Number *x, bool new_x, Ipopt::Index m, Ipopt::Index nele_jac, Ipopt::Index *iRow,
                    Ipopt::Index *jCol, Ipopt::Number *values);
    void finalize_solution(Ipopt::SolverReturn status, Ipopt::Index n, const Ipopt::Number *x, const Ipopt::Number *z_L,
                           const Ipopt::Number *z_U, Ipopt::Index m, const Ipopt::Number *g, const Ipopt::Number *lambda,
                           Ipopt::Number obj_value, const Ipopt::IpoptData *ip_data, Ipopt::IpoptCalculatedQuantities *ip_cq);
    ~ControllerNLP();
};




#endif

