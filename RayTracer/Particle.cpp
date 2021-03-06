
#include "Particle.hpp"
#include "Geodesic.hpp"

#include "ProgressBar.hpp"

Particle::Particle(const Spacetime &a_st, const VecD &a_pos3, double a_alpha,
                   double a_beta, double a_modV, double vel_squared)
    : st(a_st), pos3(a_pos3), alpha(a_alpha), beta(a_beta), modV(a_modV),
      V2(vel_squared), angle_H(M_PI / 4.), angle_V(M_PI / 4.)
{
}

void Particle::setAngleViews(double horizontal, double vertical)
{
    angle_H = horizontal;
    angle_V = vertical;
}

Matrix<VecD> Particle::view(unsigned points_horizontal)
{
    // shoot light rays
    Geodesic geo(st, 0); // -1 for particles, 0 for light

    unsigned points_vertical =
        std::round(points_horizontal * angle_V / angle_H);
    points_vertical = std::max((unsigned)2, points_vertical);

    Matrix<VecD> mat(points_vertical, points_horizontal, VecD());

    ProgressBar pb(points_vertical);

    double dV = 2. * angle_V / (points_vertical - 1);
    double dH = 2. * angle_H / (points_horizontal - 1);

#pragma omp parallel for default(none)                                         \
    shared(pb, points_vertical, points_horizontal, geo, mat, dV, dH)
    for (unsigned l = 0; l < points_vertical; ++l)
    {
        for (unsigned c = 0; c < points_horizontal; ++c)
        {
            double alpha_light = angle_V - l * dV;
            double beta_light = -angle_H + c * dH;

            VecD vel3 = make_velocity3(alpha_light, beta_light, 1.);

            VecD end_point = geo.shoot(pos3, vel3, true);
            // VecD end_point = geo.shoot(pos3, vel3, true, true);

            mat[l][c] = end_point;
        }
        ++pb;
    }

    return mat;
}

VecD Particle::make_velocity3(double alpha_light, double beta_light,
                              double modV_light)
{
    // alpha_light and beta_light: light angles from user reference point
    // alpha and beta: direction user is looking at
    modV_light = abs(modV_light);

    double cosa = cos(alpha);
    double cosal = cos(alpha_light);
    double sina = sin(alpha);
    double sinal = sin(alpha_light);
    double cosb = cos(beta);
    double cosbl = cos(beta_light);
    double sinb = sin(beta);
    double sinbl = sin(beta_light);

    double vr_rot = cosa * cosal * cosb * cosbl - cosb * sina * sinal -
                    cosal * sinb * sinbl;
    double vtheta_rot = cosal * cosbl * sina + cosa * sinal;
    double vphi_rot = cosa * cosal * cosbl * sinb - sina * sinal * sinb +
                      cosal * cosb * sinbl;

    VecD vel3_rot = {modV_light * vr_rot, modV_light * vtheta_rot,
                     modV_light * vphi_rot};

    // std::cout << std::endl;
    // std::cout << vel3_rot << std::endl;
    // std::cout << vel3_rot[0] * vel3_rot[0] + vel3_rot[1] * vel3_rot[1] +
    //                  vel3_rot[2] * vel3_rot[2]
    //           << std::endl;

    // 1st coordinate shouldn't matter, so sending a nan just to make
    // sure it really doesn't matter
    VecD pos4({NAN});
    pos4.insert(pos3);

    // convert to global coordinates
    // this assumes the metric is of type (g00 g01 g10 g11) x (g22 g33), not
    // sure it stands in general
    auto ig = st.imetric(pos4);
    vel3_rot[0] *= sqrt(std::abs(ig[1][1]));
    vel3_rot[1] *= sqrt(std::abs(ig[2][2]));
    vel3_rot[2] *= sqrt(std::abs(ig[3][3]));

    // in the end, flip ray of light so that we can evolve it backwards
    vel3_rot[0] *= -1;
    vel3_rot[1] *= -1;
    vel3_rot[2] *= -1;

    auto vel4 = st.velocity(pos4, vel3_rot, 0);
    // std::cout << "VEL4" << std::endl;
    // vel4.print();
    // std::cout << std::endl;
    if (vel4.norm() == 0. /* they are all 0*/)
    {
        throw std::runtime_error(
            "3 velocity invalid for position and particle type chosen.");
        return pos4;
    }
    // geodesic invalid (probably inside BH in an orbit that is invalid, for
    // example too big angles, just let it be and assume it collides to
    // singularity)
    if (std::isnan(vel4[0]))
    {
    }

    return vel3_rot;
}
