#include "catch.hpp"

#include "libirc/transformation.h"

#include "config.h"
#include "libirc/conversion.h"
#include "libirc/io.h"
#include "libirc/irc.h"
#include "libirc/molecule.h"
#include "libirc/wilson.h"

#include <iostream>

#ifdef HAVE_ARMA
#include <armadillo>
using vec3 = arma::vec3;
using vec = arma::vec;
using mat = arma::mat;

template<typename T>
using Mat = arma::Mat<T>;
#elif HAVE_EIGEN3
#include <eigen3/Eigen/Dense>
using vec3 = Eigen::Vector3d;
using vec = Eigen::VectorXd;
using mat = Eigen::MatrixXd;

template<typename T>
using Mat = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>;
#else
#error
#endif

using namespace irc;

TEST_CASE("RMS") {
  using namespace transformation;

  vec v{1, 2, 3, 4};

  CHECK(rms(v) == Approx(std::sqrt(30 / 4.)));
}

TEST_CASE("Transformation") {

  bool verbose{true};

  SECTION("Cartesian to internal for ethanol") {
    using namespace molecule;
    using namespace connectivity;
    using namespace transformation;
    using namespace tools::conversion;
    using namespace io;
    using namespace std;

    // Load molecule from file
    const auto molecule = load_xyz<vec3>(config::molecules_dir + "ethanol.xyz");

    // Compute interatomic distance for formaldehyde molecule
    mat dd{distances<vec3, mat>(molecule)};

    // Build graph based on the adjacency matrix
    UGraph adj{adjacency_matrix(dd, molecule)};

    // Compute distance matrix and predecessor matrix
    mat dist{distance_matrix<mat>(adj)};

    // Compute bonds
    std::vector<Bond> B{bonds(dist, molecule)};

    // Print bonds
    if (verbose) {
      print_bonds<vec3, vec>(to_cartesian<vec3, vec>(molecule), B);
    }

    // Compute angles
    std::vector<Angle> A{angles(dist, molecule)};

    // Print angles
    if (verbose) {
      print_angles<vec3, vec>(to_cartesian<vec3, vec>(molecule), A);
    }

    // Compute dihedral angles
    std::vector<Dihedral> D{dihedrals(dist, molecule)};

    // Print dihedral angles
    if (verbose) {
      print_dihedrals<vec3, vec>(to_cartesian<vec3, vec>(molecule), D);
    }

    // Compute linear angles
    std::vector<LinearAngle<vec3>> LA{linear_angles(dist, molecule)};

    // Print linear angles
    if (verbose) {
      print_linear_angles<vec3, vec>(to_cartesian<vec3, vec>(molecule), LA);
    }

    // Compute linear angles
    std::vector<OutOfPlaneBend> OOPB{out_of_plane_bends(dist, molecule)};

    // Print linear angles
    if (verbose) {
      print_out_of_plane_bends<vec3, vec>(to_cartesian<vec3, vec>(molecule),
                                          OOPB);
    }

    // Compute number of cartesian coordinates
    std::size_t n_c{3 * molecule.size()};

    // Allocate vector for cartesian positions
    vec x_c{linalg::zeros<vec>(n_c)};

    // Fill vector with cartesian positions
    for (std::size_t i{0}; i < molecule.size(); i++) {
      x_c(3 * i + 0) = molecule[i].position(0);
      x_c(3 * i + 1) = molecule[i].position(1);
      x_c(3 * i + 2) = molecule[i].position(2);
    }

    if (verbose) {
      // Print cartesian coordinates
      cout << "\nCartesian coordinates (a.u.):\n " << x_c << endl;
    }

    if (verbose) {
      // Compute and print internal redundant coordinates
      cout << "Internal redundant coordinates (a.u.):\n"
           << cartesian_to_irc<vec3, vec>(x_c, B, A, D, LA, OOPB) << endl;
    }
  }

  SECTION("Internal to cartesian for H2") {
    using namespace molecule;
    using namespace connectivity;
    using namespace transformation;
    using namespace tools::conversion;
    using namespace io;
    using namespace std;
    using namespace wilson;

    // Load molecule from file
    Molecule<vec3> molecule{{{"H", {0., 0., 0.}}, {"H", {1., 0., 0.}}}};

    // Connectivity
    mat dd{distances<vec3, mat>(molecule)};
    UGraph adj{adjacency_matrix(dd, molecule)};
    mat dist{distance_matrix<mat>(adj)};

    // Compute bonds
    std::vector<Bond> B{bonds(dist, molecule)};

    // Print bonds
    if (verbose) {
      print_bonds<vec3, vec>(to_cartesian<vec3, vec>(molecule), B);
    }

    // Check number of bonds
    REQUIRE(B.size() == 1);

    // Wilson B matrix
    mat W = wilson_matrix<vec3, vec, mat>(
        molecule::to_cartesian<vec3, vec>(molecule), B);

    // Allocate vector for internal reaction coordinates
    vec q_irc{connectivity::cartesian_to_irc<vec3, vec>(
        molecule::to_cartesian<vec3, vec>(molecule), B, {}, {}, {}, {})};

    // Displacement in internal coordinates
    vec dq_irc{0.1};

    // Print displacement in internal coordinates
    if (verbose) {
      cout << "\nDisplacement in internal coordinates (a.u.):\n " << dq_irc
           << endl;
    }

    // Compute number of cartesian coordinates
    std::size_t n_c{3 * molecule.size()};

    // Allocate vector for cartesian positions
    vec x_c_old{linalg::zeros<vec>(n_c)};

    // Fill vector with cartesian positions
    for (std::size_t i{0}; i < molecule.size(); i++) {
      x_c_old(3 * i + 0) = molecule[i].position(0);
      x_c_old(3 * i + 1) = molecule[i].position(1);
      x_c_old(3 * i + 2) = molecule[i].position(2);
    }

    // Compute new cartesian coordinates
    const auto itc_result = irc_to_cartesian<vec3, vec, mat>(
        q_irc, dq_irc, x_c_old, B, {}, {}, {}, {});
    const auto x_c = itc_result.x_c;

    // Print cartesian coordinates
    if (verbose) {
      cout << "\nNew cartesian coordinates (a.u.):\n " << x_c << endl;
    }

    vec3 p1{x_c(0), x_c(1), x_c(2)};
    vec3 p2{x_c(3), x_c(4), x_c(5)};

    Approx target{q_irc(0) + dq_irc(0)};
    target.margin(1e-6);

    REQUIRE(distance(p1, p2) == target);
  }

  SECTION("Internal to cartesian for H2O") {
    using namespace molecule;
    using namespace connectivity;
    using namespace transformation;
    using namespace tools::conversion;
    using namespace io;
    using namespace std;
    using namespace wilson;

    // Load molecule from file
    const auto molecule = load_xyz<vec3>(config::molecules_dir + "water.xyz");

    // Compute interatomic distance for formaldehyde molecule
    mat dd{distances<vec3, mat>(molecule)};

    // Build graph based on the adjacency matrix
    UGraph adj{adjacency_matrix(dd, molecule)};

    // Compute distance matrix and predecessor matrix
    mat dist{distance_matrix<mat>(adj)};

    // Compute bonds
    std::vector<Bond> B{bonds(dist, molecule)};

    // Print bonds
    if (verbose) {
      print_bonds<vec3, vec>(to_cartesian<vec3, vec>(molecule), B);
    }

    // Check number of bonds
    REQUIRE(B.size() == 2);

    // Compute angle
    std::vector<Angle> A{angles(dist, molecule)};

    // Print angles
    if (verbose) {
      print_angles<vec3, vec>(to_cartesian<vec3, vec>(molecule), A);
    }

    // Compute Wilson B matrix
    mat W = wilson_matrix<vec3, vec, mat>(
        molecule::to_cartesian<vec3, vec>(molecule), B, A);

    // Allocate vector for internal reaction coordinates
    vec q_irc_old{connectivity::cartesian_to_irc<vec3, vec>(
        molecule::to_cartesian<vec3, vec>(molecule), B, A, {}, {}, {})};

    // Displacement in internal coordinates
    vec dq_irc{0.0, 0.0, 1. / 180. * tools::constants::pi};

    // Compute new internal coordinates
    vec q_irc_new{q_irc_old + dq_irc};

    // Print new internal coordinates
    if (verbose) {
      cout << "\nNew internal coordinates:\n " << q_irc_new << endl;
    }

    // Allocate vector for cartesian positions
    vec x_c_old{to_cartesian<vec3, vec>(molecule)};

    // Compute new cartesian coordinates
    const auto itc_result = irc_to_cartesian<vec3, vec, mat>(
        q_irc_old, dq_irc, x_c_old, B, A, {}, {}, {});
    const auto x_c = itc_result.x_c;

    // Print cartesian coordinates
    if (verbose) {
      cout << "\nNew cartesian coordinates (a.u.):\n " << x_c << endl;
    }

    // Reconstruct atomic positions (points in 3D space)
    vec3 p1{x_c(0), x_c(1), x_c(2)};
    vec3 p2{x_c(3), x_c(4), x_c(5)};
    vec3 p3{x_c(6), x_c(7), x_c(8)};

    SECTION("Bond 1") {
      Approx target{q_irc_new(0)};
      target.margin(1e-4);
      REQUIRE(distance(p1, p2) == target);
    }

    SECTION("Bond 2") {
      Approx target{q_irc_new(1)};
      target.margin(1e-4);
      REQUIRE(distance(p2, p3) == target);
    }

    SECTION("Angle") {
      Approx target{q_irc_new(2)};
      target.margin(1e-4);
      REQUIRE(angle(p1, p2, p3) == target);
    }
  }

  SECTION("Internal to cartesian for H2O2") {
    using namespace molecule;
    using namespace connectivity;
    using namespace transformation;
    using namespace tools::conversion;
    using namespace io;
    using namespace std;
    using namespace wilson;

    // Load molecule from file
    const auto molecule =
        load_xyz<vec3>(config::molecules_dir + "hydrogen_peroxide.xyz");

    // Compute interatomic distance for formaldehyde molecule
    mat dd{distances<vec3, mat>(molecule)};

    // Build graph based on the adjacency matrix
    UGraph adj{adjacency_matrix(dd, molecule)};

    // Compute distance matrix and predecessor matrix
    mat dist{distance_matrix<mat>(adj)};

    // Compute bonds
    std::vector<Bond> B{bonds(dist, molecule)};

    // Print bonds
    if (verbose) {
      print_bonds<vec3, vec>(to_cartesian<vec3, vec>(molecule), B);
    }

    // Check number of bonds
    REQUIRE(B.size() == 3);

    // Compute angles
    std::vector<Angle> A{angles(dist, molecule)};

    // Print angles
    if (verbose) {
      print_angles<vec3, vec>(to_cartesian<vec3, vec>(molecule), A);
    }

    // Check number of angles
    REQUIRE(A.size() == 2);

    // Compute dihedral angles
    std::vector<Dihedral> D{dihedrals(dist, molecule)};

    // Print dihedrals
    if (verbose) {
      print_dihedrals<vec3, vec>(to_cartesian<vec3, vec>(molecule), D);
    }

    // Check number of dihedrals
    REQUIRE(D.size() == 1);

    // Compute Wilson B matrix
    mat W = wilson_matrix<vec3, vec, mat>(
        molecule::to_cartesian<vec3, vec>(molecule), B, A, D);

    // Allocate vector for internal reaction coordinates
    vec q_irc_old{connectivity::cartesian_to_irc<vec3, vec>(
        molecule::to_cartesian<vec3, vec>(molecule), B, A, D, {}, {})};

    // Displacement in internal coordinates
    vec dq_irc{0.0, 0.0, 0.0, 0.0, 0.0, 1. / 180 * tools::constants::pi};

    // Compute new internal coordinates
    vec q_irc_new{q_irc_old + dq_irc};

    // Print new internal coordinates
    if (verbose) {
      cout << "\nNew internal coordinates:\n " << q_irc_new << endl;
    }

    // Allocate vector for cartesian positions
    vec x_c_old{to_cartesian<vec3, vec>(molecule)};

    // Compute new cartesian coordinates
    const auto itc_result = irc_to_cartesian<vec3, vec, mat>(
        q_irc_old, dq_irc, x_c_old, B, A, D, {}, {});
    const auto x_c = itc_result.x_c;

    // Print cartesian coordinates
    if (verbose) {
      cout << "\nNew cartesian coordinates (a.u.):\n " << x_c << endl;
    }

    // Reconstruct atomic positions (points in 3D space)
    vec3 p1{x_c(0), x_c(1), x_c(2)};
    vec3 p2{x_c(3), x_c(4), x_c(5)};
    vec3 p3{x_c(6), x_c(7), x_c(8)};
    vec3 p4{x_c(9), x_c(10), x_c(11)};

    SECTION("Bond 1") {
      Approx target{q_irc_new(0)};
      target.margin(1e-4);
      REQUIRE(distance(p1, p2) == target);
    }

    SECTION("Bond 2") {
      Approx target{q_irc_new(1)};
      target.margin(1e-4);
      REQUIRE(distance(p1, p3) == target);
    }

    SECTION("Bond 3") {
      Approx target{q_irc_new(2)};
      target.margin(1e-4);
      REQUIRE(distance(p2, p4) == target);
    }

    SECTION("Angle 1") {
      Approx target{q_irc_new(3)};
      target.margin(1e-4);
      REQUIRE(angle(p2, p1, p3) == target);
    }

    SECTION("Angle 2") {
      Approx target{q_irc_new(4)};
      target.margin(1e-4);
      REQUIRE(angle(p1, p2, p4) == target);
    }

    SECTION("Dihedral") {
      Approx target{q_irc_new(5)};
      target.margin(1e-4);
      REQUIRE(dihedral(p4, p2, p1, p3) == target);
    }
  }

  SECTION("Internal to cartesian for CO2") {
    using namespace molecule;
    using namespace connectivity;
    using namespace transformation;
    using namespace tools::conversion;
    using namespace io;
    using namespace std;
    using namespace wilson;

    // Load molecule from file
    const auto molecule =
        load_xyz<vec3>(config::molecules_dir + "carbon_dioxide.xyz");

    // Compute interatomic distance for molecule
    mat dd{distances<vec3, mat>(molecule)};

    // Build graph based on the adjacency matrix
    UGraph adj{adjacency_matrix(dd, molecule)};

    // Compute distance matrix and predecessor matrix
    mat dist{distance_matrix<mat>(adj)};

    // Compute bonds
    std::vector<Bond> B{bonds(dist, molecule)};

    // Print bonds
    if (verbose) {
      print_bonds<vec3, vec>(to_cartesian<vec3, vec>(molecule), B);
    }

    // Check number of bonds
    REQUIRE(B.size() == 2);

    // Compute angles
    std::vector<Angle> A{angles(dist, molecule)};

    // Print angles
    if (verbose) {
      print_angles<vec3, vec>(to_cartesian<vec3, vec>(molecule), A);
    }

    // Check number of angles
    REQUIRE(A.size() == 0);

    // Compute dihedral angles
    std::vector<Dihedral> D{dihedrals(dist, molecule)};

    // Print dihedrals
    if (verbose) {
      print_dihedrals<vec3, vec>(to_cartesian<vec3, vec>(molecule), D);
    }

    // Check number of dihedrals
    REQUIRE(D.size() == 0);

    // Compute linear angles
    std::vector<LinearAngle<vec3>> LA{linear_angles(dist, molecule)};

    // Print linear angles
    if (verbose) {
      print_linear_angles<vec3, vec>(to_cartesian<vec3, vec>(molecule), LA);
    }

    // Check number of linear angles
    REQUIRE(LA.size() == 2);

    // Compute Wilson B matrix
    mat W = wilson_matrix<vec3, vec, mat>(
        molecule::to_cartesian<vec3, vec>(molecule), B, A, D, LA, {});

    // Allocate vector for internal reaction coordinates
    vec q_irc_old{connectivity::cartesian_to_irc<vec3, vec>(
        molecule::to_cartesian<vec3, vec>(molecule), B, A, D, LA, {})};

    // Displacement in internal coordinates
    vec dq_irc{0.0, 0.0, 0.0, 1. / 180 * tools::constants::pi};

    // Compute new internal coordinates
    vec q_irc_new{q_irc_old + dq_irc};

    // Print new internal coordinates
    if (verbose) {
      cout << "\nNew internal coordinates:\n " << q_irc_new << endl;
    }

    // Allocate vector for cartesian positions
    vec x_c_old{to_cartesian<vec3, vec>(molecule)};

    // Compute new cartesian coordinates
    const auto itc_result = irc_to_cartesian<vec3, vec, mat>(
        q_irc_old, dq_irc, x_c_old, B, A, D, LA, {});
    const auto x_c = itc_result.x_c;

    // Print cartesian coordinates
    if (verbose) {
      cout << "\nNew cartesian coordinates (a.u.):\n " << x_c << endl;
    }

    // Reconstruct atomic positions (points in 3D space)
    vec3 p1{x_c(0), x_c(1), x_c(2)};
    vec3 p2{x_c(3), x_c(4), x_c(5)};
    vec3 p3{x_c(6), x_c(7), x_c(8)};

    SECTION("Bond CO{1}") {
      Approx target{q_irc_new(0)};
      target.margin(1e-4);
      REQUIRE(distance(p1, p2) == target);
    }

    SECTION("Bond CO{2}") {
      Approx target{q_irc_new(1)};
      target.margin(1e-4);
      REQUIRE(distance(p1, p3) == target);
    }

    SECTION("Angle OCO") {
      Approx target{q_irc_new(3)};
      target.margin(1e-4);
      REQUIRE(angle(LA[1], x_c) == target);
    }
  }

  SECTION("Big change in water") {
    using namespace molecule;
    using namespace connectivity;
    using namespace transformation;
    using namespace tools::conversion;
    using namespace io;
    using namespace std;
    using namespace wilson;

    auto molecule = molecule::Molecule<vec3>{
        {{"O", {0., 0., 0.}}, {"H", {1., 0., 0.}}, {"H", {0., 1., 0.}}}};
    multiply_positions(molecule, tools::conversion::angstrom_to_bohr);

    // Compute interatomic distance for formaldehyde molecule
    mat dd{distances<vec3, mat>(molecule)};

    // Build graph based on the adjacency matrix
    UGraph adj{adjacency_matrix(dd, molecule)};

    // Compute distance matrix and predecessor matrix
    mat dist{distance_matrix<mat>(adj)};

    // Compute bonds
    std::vector<Bond> B{bonds(dist, molecule)};

    // Check number of bonds
    REQUIRE(B.size() == 2);

    // Compute angle
    std::vector<Angle> A{angles(dist, molecule)};

    // Compute Wilson B matrix
    mat W = wilson_matrix<vec3, vec, mat>(
        molecule::to_cartesian<vec3, vec>(molecule), B, A);

    // Allocate vector for internal reaction coordinates
    vec q_irc_old{connectivity::cartesian_to_irc<vec3, vec>(
        molecule::to_cartesian<vec3, vec>(molecule), B, A, {}, {}, {})};

    // Displacement in internal coordinates
    vec dq_irc_75{0.5, 0.5, 75. / 180. * tools::constants::pi};
    vec dq_irc_89{0.5, 0.5, 89. / 180. * tools::constants::pi};

    // Compute new internal coordinates
    vec q_irc_new{q_irc_old + dq_irc_75};
    CAPTURE(q_irc_new);

    // Allocate vector for cartesian positions
    vec x_c_old{to_cartesian<vec3, vec>(molecule)};

    // Compute new cartesian coordinates
    const auto itc_result_single = irc_to_cartesian_single<vec3, vec, mat>(
        q_irc_old, dq_irc_75, x_c_old, B, A, {}, {}, {});
    CHECK(!itc_result_single.converged);

    const auto itc_result = irc_to_cartesian<vec3, vec, mat>(
        q_irc_old, dq_irc_75, x_c_old, B, A, {}, {}, {});
    CHECK(itc_result.converged);
    const auto x_c = itc_result.x_c;

    const auto itc_result89 = irc_to_cartesian<vec3, vec, mat>(
        q_irc_old, dq_irc_89, x_c_old, B, A, {}, {}, {});
    CHECK(itc_result89.converged);

    // Print cartesian coordinates
    CAPTURE(x_c);

    // Reconstruct atomic positions (points in 3D space)
    vec3 p1{x_c(0), x_c(1), x_c(2)};
    vec3 p2{x_c(3), x_c(4), x_c(5)};
    vec3 p3{x_c(6), x_c(7), x_c(8)};

    SECTION("Bond 1") {
      Approx target{q_irc_new(0)};
      target.margin(1e-4);
      REQUIRE(distance(p1, p2) == target);
    }

    SECTION("Bond 2") {
      Approx target{q_irc_new(1)};
      target.margin(1e-4);
      REQUIRE(distance(p1, p3) == target);
    }

    SECTION("Angle") {
      Approx target{q_irc_new(2)};
      target.margin(1e-4);
      REQUIRE(angle(p2, p1, p3) == target);
    }
  }
}