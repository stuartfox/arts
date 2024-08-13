#include <disort-test.h>

#include "artstime.h"
#include "disort.h"
#include "matpack_data.h"
#include "matpack_math.h"

void test_11a_1layer() try {
  const AscendingGrid tau_arr{8.};
  const Vector omega_arr{0.999999};
  const Index NQuad = 16;
  const Matrix Leg_coeffs_all{
      Vector{1.00000000e+00, 7.50000000e-01, 5.62500000e-01, 4.21875000e-01,
             3.16406250e-01, 2.37304688e-01, 1.77978516e-01, 1.33483887e-01,
             1.00112915e-01, 7.50846863e-02, 5.63135147e-02, 4.22351360e-02,
             3.16763520e-02, 2.37572640e-02, 1.78179480e-02, 1.33634610e-02,
             1.00225958e-02, 7.51694682e-03, 5.63771011e-03, 4.22828259e-03,
             3.17121194e-03, 2.37840895e-03, 1.78380672e-03, 1.33785504e-03,
             1.00339128e-03, 7.52543458e-04, 5.64407594e-04, 4.23305695e-04,
             3.17479271e-04, 2.38109454e-04, 1.78582090e-04, 1.33936568e-04}
          .reshape(tau_arr.nelem(), 32)};

  const Numeric mu0  = 0.6;
  const Numeric I0   = Constant::pi / mu0;
  const Numeric phi0 = 0.9 * Constant::pi;
  Matrix b_neg(NQuad, NQuad / 2, 0);
  b_neg[0] = 1;
  Matrix b_pos(NQuad, NQuad / 2, 0);
  b_pos[0] = 1;
  const std::vector<disort::BDRF> BDRF_Fourier_modes{
      disort::BDRF{[](auto c, auto&, auto&) { c = 1; }}};
  const Matrix s_poly_coeffs{
      Vector{172311.79936609, -102511.4417051}.reshape(tau_arr.nelem(), 2)};
  const Vector f_arr{Leg_coeffs_all(joker, NQuad)};

  // Optional (unused)
  const Index NLeg     = NQuad;
  const Index NFourier = NQuad;

  const disort::main_data dis(NQuad,
                              NLeg,
                              NFourier,
                              tau_arr,
                              omega_arr,
                              Leg_coeffs_all,
                              b_pos,
                              b_neg,
                              f_arr,
                              s_poly_coeffs,
                              BDRF_Fourier_modes,
                              mu0,
                              I0,
                              phi0);

  const Vector taus{Vector{
      0.06354625877794251,
      0.6354625877794251,
      6.354625877794252,
  }
                        .reshape_as(3)};

  const Vector phis{Vector{
      0.0,
      1.5705463267948965,
      3.141092653589793,
      4.71163898038469,
      6.282185307179586,
  }
                        .reshape_as(5)};

  const Tensor3 u{Vector{
      -67450758.16951953,  -68163936.52049603,  -69234833.32187165,
      -70493301.40856712,  -71761958.65132673,  -72860082.97247753,
      -73659184.17959407,  -74106654.75041592,  -66959005.01757623,
      -65526060.17272041,  -64948569.81054169,  -64759278.69986952,
      -64692577.74400561,  -64666522.843816414, -64655496.42802922,
      -64651031.49232986,  -650686894.4741026,  -651272164.6780624,
      -652228952.2036492,  -653403724.1198788,  -654598302.6476525,
      -655622176.5082631,  -656354668.0476904,  -656758576.4843147,
      -650400581.2871597,  -649799226.1011513,  -648759862.1255602,
      -647771339.7824199,  -647164663.3275844,  -646851695.9821538,
      -646700194.0494821,  -646634640.5543195,  -6477459952.126891,
      -6477674415.033772,  -6477906773.728024,  -6477970231.297722,
      -6477850974.864132,  -6477680699.931821,  -6477545425.358499,
      -6477469392.277737,  -6477337708.4923315, -6477051473.021847,
      -6476486177.443923,  -6475639420.811406,  -6474608811.336018,
      -6473576485.232794,  -6472737952.128982,  -6472232973.386699,
      -67450757.90557855,  -68163936.23959401,  -69234833.05442454,
      -70493301.1889061,   -71761958.49122615,  -72860082.86657849,
      -73659184.11718208,  -74106654.72396477,  -66959004.78386354,
      -65526060.06670893,  -64948569.759241685, -64759278.66701288,
      -64692577.71779058,  -64666522.82054599,  -64655496.408649854,
      -64651031.48106596,  -650686894.1577288,  -651272164.3860651,
      -652228951.959851,   -653403723.9370276,  -654598302.5219841,
      -655622176.4281206,  -656354668.0015876,  -656758576.4650228,
      -650400580.9614232,  -649799225.7652177,  -648759861.8139644,
      -647771339.5205104,  -647164663.1052219,  -646851695.7883159,
      -646700193.8930585,  -646634640.4658027,  -6477459952.112427,
      -6477674415.021716,  -6477906773.719225,  -6477970231.292038,
      -6477850974.860798,  -6477680699.929986,  -6477545425.357564,
      -6477469392.277379,  -6477337708.476555,  -6477051473.003029,
      -6476486177.418904,  -6475639420.776205,  -6474608811.287537,
      -6473576485.173855,  -6472737952.072827,  -6472232973.354409,
      -67450757.09557769,  -68163935.5343148,   -69234832.53857543,
      -70493300.86562322,  -71761958.30898766,  -72860082.77142973,
      -73659184.07194658,  -74106654.7080745,   -66959003.978329994,
      -65526059.605015315, -64948569.40597644,  -64759278.25724076,
      -64692577.24689745,  -64666522.569943115, -64655496.33859091,
      -64651031.46785406,  -650686893.3997865,  -651272163.7866305,
      -652228951.5582715,  -653403723.6991161,  -654598302.3917136,
      -655622176.3607389,  -656354667.969506,   -656758576.4537528,
      -650400580.114515,   -649799224.7150289,  -648759860.4284285,
      -647771337.6209338,  -647164660.7906611,  -646851694.4073068,
      -646700193.4464177,  -646634640.3713492,  -6477459952.102764,
      -6477674415.013875,  -6477906773.713715,  -6477970231.288613,
      -6477850974.858863,  -6477680699.92896,   -6477545425.357059,
      -6477469392.277191,  -6477337708.465864,  -6477051472.989859,
      -6476486177.400225,  -6475639420.746941,  -6474608811.241033,
      -6473576485.113884,  -6472737952.023491,  -6472232973.333137,
      -67450758.08345927,  -68163936.42571846,  -69234833.22735378,
      -70493301.32565282,  -71761958.5868767,   -72860082.92706731,
      -73659184.1511521,   -74106654.73746914,  -66959004.942842305,
      -65526060.140064254, -64948569.79565495,  -64759278.69119482,
      -64692577.73741896,  -64666522.83784527,  -64655496.422317065,
      -64651031.48795751,  -650686894.3660356,  -651272164.5751565,
      -652228952.1136987,  -653403724.0484096,  -654598302.595602,
      -655622176.4731523,  -656354668.0263708,  -656758576.4748386,
      -650400581.1776748,  -649799225.9918531,  -648759862.0290012,
      -647771339.7061839,  -647164663.2654041,  -646851695.9271611,
      -646700193.9997499,  -646634640.5186839,  -6477459952.120042,
      -6477674415.028028,  -6477906773.723791,  -6477970231.29496,
      -6477850974.862495,  -6477680699.930912,  -6477545425.358031,
      -6477469392.277556,  -6477337708.484887,  -6477051473.013036,
      -6476486177.432378,  -6475639420.7955065, -6474608811.314658,
      -6473576485.207162,  -6472737952.104084,  -6472232973.371436,
      -67450758.16955158,  -68163936.52052972,  -69234833.32190868,
      -70493301.40859495,  -71761958.65135679,  -72860082.9724962,
      -73659184.1796066,   -74106654.75042164,  -66959005.01760073,
      -65526060.172731206, -64948569.8105462,   -64759278.699872315,
      -64692577.7440076,   -64666522.84381816,  -64655496.428031065,
      -64651031.49233159,  -650686894.4741422,  -651272164.6781001,
      -652228952.2036842,  -653403724.1199051,  -654598302.6476754,
      -655622176.508278,   -656354668.0476999,  -656758576.4843189,
      -650400581.2871983,  -649799226.1011896,  -648759862.1255922,
      -647771339.7824451,  -647164663.3276044,  -646851695.9821712,
      -646700194.049499,   -646634640.5543336,  -6477459952.126894,
      -6477674415.033774,  -6477906773.728025,  -6477970231.297724,
      -6477850974.864131,  -6477680699.931822,  -6477545425.358499,
      -6477469392.277737,  -6477337708.492333,  -6477051473.02185,
      -6476486177.443929,  -6475639420.811412,  -6474608811.336026,
      -6473576485.232804,  -6472737952.128993,  -6472232973.386706,
  }
                      .reshape_as(5, 3, 16)};

  const Matrix u0{Vector{
      -67450757.82621326, -68163936.19030188,  -69234833.04060522,
      -70493301.19912213, -71761958.51009054,  -72860082.884712,
      -73659184.12987866, -74106654.73013352,  -66959004.6939199,
      -65526060.00522814, -64948569.70208727,  -64759278.58400482,
      -64692577.61873609, -64666522.772953674, -64655496.40075306,
      -64651031.48236285, -650686894.1091124,  -651272164.3632954,
      -652228951.9620285, -653403723.9522649,  -654598302.5395283,
      -655622176.4427109, -656354668.0112506,  -656758576.4695544,
      -650400580.8967929, -649799225.6604894,  -648759861.6294249,
      -647771339.1857026, -647164662.6600226,  -646851695.5523425,
      -646700193.8544433, -646634640.4781708,  -6477459952.115544,
      -6477674415.024357, -6477906773.721194,  -6477970231.293337,
      -6477850974.861573, -6477680699.930421,  -6477545425.357788,
      -6477469392.277466, -6477337708.479925,  -6477051473.006965,
      -6476486177.423901, -6475639420.782612,  -6474608811.295008,
      -6473576485.182222, -6472737952.082489,  -6472232973.361435,
  }
                      .reshape_as(3, 16)};

  const Vector flux_down_diffuse{
      Vector{-203346309.84111682, -2033298515.7799737, -20339001823.603107}
          .reshape_as(3)};

  const Vector flux_down_direct{Vector{
      14.796267664990843,
      5.704076430616008,
      0.00041353941227710225,
  }
                                    .reshape_as(3)};

  const Vector flux_up{
      Vector{-226730089.1295884, -2057655944.8032463, -20350383841.43313}
          .reshape_as(3)};

  //flat_print(u, compute_u(dis, taus, phis, true) );
  //const auto [flux_up_, flux_down_diffuse_, flux_down_direct_] =  compute_flux(dis, taus);
  //flat_print(flux_up, flux_up_);

  compare("test_11a-1layer",
          dis,
          taus,
          phis,
          u,
          u0,
          flux_down_diffuse,
          flux_down_direct,
          flux_up,
          true);
} catch (std::exception& e) {
  throw std::runtime_error(var_string("Error in test-11a-1layer:\n", e.what()));
}

void test_11a_multilayer() try {
  const AscendingGrid tau_arr{
      0.5, 1., 1.5, 2., 2.5, 3., 3.5, 4., 4.5, 5., 5.5, 6., 6.5, 7., 7.5, 8.};
  const Vector omega_arr(tau_arr.size(), 0.999999);
  const Index NQuad = 16;
  Matrix Leg_coeffs_all(tau_arr.size(), 32);
  for (auto&& v : Leg_coeffs_all)
    v = {1.00000000e+00, 7.50000000e-01, 5.62500000e-01, 4.21875000e-01,
         3.16406250e-01, 2.37304688e-01, 1.77978516e-01, 1.33483887e-01,
         1.00112915e-01, 7.50846863e-02, 5.63135147e-02, 4.22351360e-02,
         3.16763520e-02, 2.37572640e-02, 1.78179480e-02, 1.33634610e-02,
         1.00225958e-02, 7.51694682e-03, 5.63771011e-03, 4.22828259e-03,
         3.17121194e-03, 2.37840895e-03, 1.78380672e-03, 1.33785504e-03,
         1.00339128e-03, 7.52543458e-04, 5.64407594e-04, 4.23305695e-04,
         3.17479271e-04, 2.38109454e-04, 1.78582090e-04, 1.33936568e-04};

  const Numeric mu0  = 0.6;
  const Numeric I0   = Constant::pi / mu0;
  const Numeric phi0 = 0.9 * Constant::pi;
  Matrix b_neg(NQuad, NQuad / 2, 0);
  b_neg[0] = 1;
  Matrix b_pos(NQuad, NQuad / 2, 0);
  b_pos[0] = 1;
  const std::vector<disort::BDRF> BDRF_Fourier_modes{
      disort::BDRF{[](auto c, auto&, auto&) { c = 1; }}};
  Matrix s_poly_coeffs(tau_arr.size(), 2);
  for (auto&& v : s_poly_coeffs) v = {172311.79936609, -102511.4417051};
  const Vector f_arr{Leg_coeffs_all(joker, NQuad)};

  // Optional (unused)
  const Index NLeg     = NQuad;
  const Index NFourier = NQuad;

  const disort::main_data dis(NQuad,
                              NLeg,
                              NFourier,
                              tau_arr,
                              omega_arr,
                              Leg_coeffs_all,
                              b_pos,
                              b_neg,
                              f_arr,
                              s_poly_coeffs,
                              BDRF_Fourier_modes,
                              mu0,
                              I0,
                              phi0);

  const Vector taus{Vector{
      0.06354625877794251,
      0.6354625877794251,
      6.354625877794252,
  }
                        .reshape_as(3)};

  const Vector phis{Vector{
      0.0,
      1.5705463267948965,
      3.141092653589793,
      4.71163898038469,
      6.282185307179586,
  }
                        .reshape_as(5)};

  const Tensor3 u{
      Vector{-67450758.14903769,  -68163936.50382535,  -69234833.29132824,
             -70493301.37079276,  -71761958.60901774,  -72860082.92745098,
             -73659184.12813313,  -74106654.70020135,  -66959005.01002408,
             -65526060.16575466,  -64948569.82191234,  -64759278.71767634,
             -64692577.743328,    -64666522.848696455, -64655496.42265687,
             -64651031.49208526,  -650686894.4463683,  -651272164.6676837,
             -652228952.1759232,  -653403724.0940198,  -654598302.6281518,
             -655622176.4697446,  -656354667.9985384,  -656758576.4423634,
             -650400581.2621005,  -649799226.0846163,  -648759862.1039226,
             -647771339.7619898,  -647164663.2810019,  -646851695.9957008,
             -646700194.0090532,  -646634640.5420512,  -6477459952.1887245,
             -6477674415.098106,  -6477906773.78792,   -6477970231.3711195,
             -6477850974.929533,  -6477680700.001882,  -6477545425.417721,
             -6477469392.354252,  -6477337708.55976,   -6477051473.08671,
             -6476486177.502056,  -6475639420.85857,   -6474608811.388152,
             -6473576485.320278,  -6472737952.176019,  -6472232973.393154,
             -67450757.88565686,  -68163936.22365266,  -69234833.02485746,
             -70493301.15301536,  -71761958.45123464,  -72860082.82389942,
             -73659184.06751738,  -74106654.67408988,  -66959004.776765816,
             -65526060.05991195,  -64948569.77066825,  -64759278.68483988,
             -64692577.717121124, -64666522.82542967,  -64655496.40327916,
             -64651031.48082197,  -650686894.1309899,  -651272164.3769156,
             -652228951.9338365,  -653403723.9139154,  -654598302.5057462,
             -655622176.392624,   -656354667.9546527,  -656758576.4238464,
             -650400580.9372557,  -649799225.749543,   -648759861.7947971,
             -647771339.4991487,  -647164663.0565672,  -646851695.8017068,
             -646700193.8562099,  -646634640.4549712,  -6477459952.180079,
             -6477674415.091173,  -6477906773.783278,  -6477970231.368403,
             -6477850974.928045,  -6477680700.001124,  -6477545425.417101,
             -6477469392.354011,  -6477337708.550181,  -6477051473.074819,
             -6476486177.484775,  -6475639420.830778,  -6474608811.379449,
             -6473576485.213845,  -6472737952.169698,  -6472232973.363037,
             -67450757.07598934,  -68163935.51891647,  -69234832.50958839,
             -70493300.83050717,  -71761958.27064274,  -72860082.73041318,
             -73659184.02272005,  -74106654.65912507,  -66959003.97150129,
             -65526059.59831737,  -64948569.417435355, -64759278.27507921,
             -64692577.246232525, -64666522.57482877,  -64655496.33322109,
             -64651031.467610374, -650686893.3736631,  -651272163.7783521,
             -652228951.5334057,  -653403723.6777627,  -654598302.3778365,
             -655622176.3273402,  -656354667.9235806,  -656758576.4133506,
             -650400580.0908878,  -649799224.6959357,  -648759860.4186151,
             -647771337.5915831,  -647164660.6995014,  -646851694.3985306,
             -646700193.4132613,  -646634640.3591969,  -6477459952.177609,
             -6477674415.089273,  -6477906773.7820215, -6477970231.367653,
             -6477850974.927633,  -6477680700.000901,  -6477545425.417023,
             -6477469392.353989,  -6477337708.547455,  -6477051473.071504,
             -6476486177.480588,  -6475639420.827091,  -6474608811.3889265,
             -6473576485.23422,   -6472737952.169537,  -6472232973.457826,
             -67450758.06325135,  -68163936.40943101,  -69234833.19738722,
             -70493301.28855269,  -71761958.54573691,  -72860082.88317712,
             -73659184.1005644,   -74106654.68763705,  -66959004.93551264,
             -65526060.133181304, -64948569.807053134, -64759278.70901162,
             -64692577.7367454,   -64666522.84272714,  -64655496.416945554,
             -64651031.48771322,  -650686894.3387845,  -651272164.5653957,
             -652228952.086854,   -653403724.0237433,  -654598302.5776662,
             -655622176.4361007,  -656354667.9782602,  -656758576.4333789,
             -650400581.1530479,  -649799225.974835,   -648759862.0080485,
             -647771339.6866204,  -647164663.2180353,  -646851695.9405566,
             -646700193.9609994,  -646634640.5040421,  -6477459952.183318,
             -6477674415.093759,  -6477906773.784961,  -6477970231.369385,
             -6477850974.928655,  -6477680700.001463,  -6477545425.417612,
             -6477469392.354268,  -6477337708.553781,  -6477051473.079463,
             -6476486177.492759,  -6475639420.847761,  -6474608811.351992,
             -6473576485.237803,  -6472737952.167742,  -6472232973.562391,
             -67450758.14906965,  -68163936.50385895,  -69234833.29136601,
             -70493301.37081768,  -71761958.60904449,  -72860082.92746592,
             -73659184.12814641,  -74106654.70020661,  -66959005.010048494,
             -65526060.165765435, -64948569.82191683,  -64759278.717679106,
             -64692577.74332999,  -64666522.848698206, -64655496.42265871,
             -64651031.492087,    -650686894.4464079,  -651272164.667721,
             -652228952.1759582,  -653403724.0940443,  -654598302.6281725,
             -655622176.4697573,  -656354667.9985479,  -656758576.4423673,
             -650400581.2621387,  -649799226.0846542,  -648759862.1039586,
             -647771339.7620115,  -647164663.2810249,  -646851695.9957173,
             -646700194.0090697,  -646634640.5420663,  -6477459952.188728,
             -6477674415.09811,   -6477906773.787922,  -6477970231.37112,
             -6477850974.929533,  -6477680700.001883,  -6477545425.417722,
             -6477469392.354253,  -6477337708.559765,  -6477051473.086717,
             -6476486177.50207,   -6475639420.858608,  -6474608811.388416,
             -6473576485.320588,  -6472737952.175774,  -6472232973.392494}
          .reshape_as(5, 3, 16)};

  const Matrix u0{
      Vector{-67450757.8061632,  -68163936.17421912, -69234833.01082006,
             -70493301.16255033, -71761958.46994467, -72860082.84146859,
             -73659184.0796162,  -74106654.68038987, -66959004.68671775,
             -65526059.9983922,  -64948569.71350084, -64759278.601827085,
             -64692577.6180647,  -64666522.77783649, -64655496.395381965,
             -64651031.48211871, -650686894.0821501, -651272164.353896,
             -652228951.9356537, -653403723.9284612, -654598302.5227894,
             -655622176.4065475, -656354667.9637042, -656758576.4282947,
             -650400580.8724246, -649799225.6441625, -648759861.6084594,
             -647771339.1651642, -647164662.61388,   -646851695.5650378,
             -646700193.8151977, -646634640.4655975, -6477459952.182439,
             -6477674415.093082, -6477906773.784549, -6477970231.369143,
             -6477850974.928468, -6477680700.001344, -6477545425.417358,
             -6477469392.354126, -6477337708.552801, -6477051473.078132,
             -6476486177.490063, -6475639420.841084, -6474608811.37826,
             -6473576485.251894, -6472737952.175217, -6472232973.442459}
          .reshape_as(3, 16)};

  const Vector flux_down_diffuse{
      Vector{-203346309.85131612, -2033298515.7140882, -20339001823.843243}
          .reshape_as(3)};

  const Vector flux_down_direct{Vector{
      14.796267664990843,
      5.704076430616008,
      0.00041353941227710225,
  }
                                    .reshape_as(3)};

  const Vector flux_up{
      Vector{-226730088.99771807, -2057655944.7039967, -20350383841.64821}
          .reshape_as(3)};

  //flat_print(u, compute_u(dis, taus, phis, true) );
  //  const auto [flux_up_, flux_down_diffuse_, flux_down_direct_] =  compute_flux(dis, taus);
  //flat_print(flux_down_diffuse, flux_down_diffuse_);

  compare("test_11a-multilayer",
          dis,
          taus,
          phis,
          u,
          u0,
          flux_down_diffuse,
          flux_down_direct,
          flux_up,
          true);
} catch (std::exception& e) {
  throw std::runtime_error(
      var_string("Error in test-11a-multilayer:\n", e.what()));
}

int main() try {
  std::cout << std::setprecision(16);
  test_11a_1layer();
  test_11a_multilayer();
} catch (std::exception& e) {
  std::cerr << "Error in main:\n" << e.what() << '\n';
  return EXIT_FAILURE;
}
