/*!
  \file   m_hitran_xsec.cc
  \author Oliver Lemke <oliver.lemke@uni-hamburg.de>
  \date   2021-02-23

  \brief  Workspace methods for HITRAN absorption cross section data.

*/

#include <workspace.h>

#include "jacobian.h"
#include "physics_funcs.h"
#include "xml_io.h"
#include "xsec_fit.h"

/* Workspace method: Doxygen documentation will be auto-generated */
void ReadXsecData(ArrayOfXsecRecord& xsec_fit_data,
                  const ArrayOfArrayOfSpeciesTag& abs_species,
                  const String& basename) {
  // Build a set of species indices. Duplicates are ignored.
  std::set<Species::Species> unique_species;
  for (auto& asp : abs_species) {
    for (auto& sp : asp) {
      if (sp.Type() == Species::TagType::XsecFit) {
        unique_species.insert(sp.Spec());
      }
    }
  }

  String tmpbasename = basename;
  if (basename.length() && basename[basename.length() - 1] != '/') {
    tmpbasename += '.';
  }

  // Read xsec data for all active species and collect them in xsec_fit_data
  xsec_fit_data.clear();
  for (auto& species_name : unique_species) {
    XsecRecord xsec_coeffs;
    const String filename{tmpbasename +
                          String(Species::toShortName(species_name)) + ".xml"};

    try {
      xml_read_from_file(filename, xsec_coeffs);

      xsec_fit_data.push_back(xsec_coeffs);
    } catch (const std::exception& e) {
      ARTS_USER_ERROR(
          "Error reading coefficients file:\n", filename, "\n", e.what());
    }
  }
}

/* Workspace method: Doxygen documentation will be auto-generated */
void propmat_clearskyAddXsecFit(  // WS Output:
    PropmatVector& propmat_clearsky,
    PropmatMatrix& dpropmat_clearsky_dx,
    // WS Input:
    const ArrayOfArrayOfSpeciesTag& abs_species,
    const ArrayOfSpeciesTag& select_abs_species,
    const JacobianTargets& jacobian_targets,
    const Vector& f_grid,
    const AtmPoint& atm_point,
    const ArrayOfXsecRecord& xsec_fit_data,
    const Numeric& force_p,
    const Numeric& force_t) {
  // Forward simulations and their error handling
  ARTS_USER_ERROR_IF(
      propmat_clearsky.size() not_eq f_grid.size(),
      "Mismatch dimensions on internal matrices of xsec and frequency");

  // Derivatives and their error handling
  ARTS_USER_ERROR_IF(
    static_cast<Size>(dpropmat_clearsky_dx.nrows()) not_eq jacobian_targets.target_count()
    or
    dpropmat_clearsky_dx.ncols() not_eq f_grid.size(),
      "Mismatch dimensions on internal matrices of xsec derivatives and frequency");

  // Jacobian overhead START
  /* NOTE:  The calculations below are inefficient and could
              be made much better by using interp in Extract to
              return the derivatives as well. */
  // Jacobian vectors START
  Vector dxsec_temp_dT;
  Vector dxsec_temp_dF;
  Vector dfreq;
  // Jacobian vectors END
  const auto freq_jac = jacobian_targets.find_all<Jacobian::AtmTarget>(
      Atm::Key::wind_u, Atm::Key::wind_v, Atm::Key::wind_w);
  const auto temp_jac = jacobian_targets.find<Jacobian::AtmTarget>(Atm::Key::t);
  const bool do_freq_jac = std::ranges::any_of(freq_jac, [](auto& x) { return x.first; });
  const bool do_temp_jac = temp_jac.first;
  const Numeric df = field_perturbation(freq_jac);
  const Numeric dt = temp_jac.first;
  if (do_freq_jac) {
    dfreq.resize(f_grid.size());
    dfreq = f_grid;
    dfreq += df;
    dxsec_temp_dF.resize(f_grid.size());
  }
  if (do_temp_jac) {
    dxsec_temp_dT.resize(f_grid.size());
  }
  // Jacobian overhead END

  // Useful if there is no Jacobian to calculate
  ArrayOfMatrix empty;

  // Allocate a vector with dimension frequencies for constructing our
  // cross-sections before adding them (more efficient to allocate this here
  // outside of the loops)
  Vector xsec_temp(f_grid.size(), 0.);

  ArrayOfString fail_msg;
  bool do_abort = false;
  // Loop over Xsec data sets.
  // Index ii loops through the outer array (different tag groups),
  // index s through the inner array (different tags within each goup).
  for (Size i = 0; i < abs_species.size(); i++) {
    if (select_abs_species.size() and abs_species[i] not_eq select_abs_species)
      continue;

    const Numeric vmr = atm_point[abs_species[i].Species()];

    for (Size s = 0; s < abs_species[i].size(); s++) {
      const SpeciesTag& this_species = abs_species[i][s];

      // Check if this is a HITRAN cross section tag
      if (this_species.Type() != Species::TagType::XsecFit) continue;

      Index this_xdata_index =
          hitran_xsec_get_index(xsec_fit_data, this_species.Spec());
      ARTS_USER_ERROR_IF(this_xdata_index < 0,
                         "Cross-section species ",
                         this_species.Name(),
                         " not found in *xsec_fit_data*.")
      const XsecRecord& this_xdata = xsec_fit_data[this_xdata_index];
      // ArrayOfMatrix& this_dxsec = do_jac ? dpropmat_clearsky_dx[i] : empty;

      if (do_abort) continue;
      const Numeric current_p = force_p < 0 ? atm_point.pressure : force_p;
      const Numeric current_t = force_t < 0 ? atm_point.temperature : force_t;

      // Get the absorption cross sections from the HITRAN data:
      this_xdata.Extract(xsec_temp, f_grid, current_p, current_t);
      if (do_freq_jac) {
        this_xdata.Extract(
            dxsec_temp_dF, dfreq, current_p, current_t);
      }
      if (do_temp_jac) {
        this_xdata.Extract(
            dxsec_temp_dT, f_grid, current_p, current_t + dt);
      }
    }

    // Add to result variable:
    Numeric nd = number_density(atm_point.pressure, atm_point.temperature);

    Numeric dnd_dt = dnumber_density_dt(atm_point.pressure, atm_point.temperature);
    for (Index f = 0; f < f_grid.size(); f++) {
      propmat_clearsky[f].A() += xsec_temp[f] * nd * vmr;

      if (temp_jac.first) {
        const auto iq = temp_jac.second->target_pos;
        dpropmat_clearsky_dx(iq, f).A() +=
            ((dxsec_temp_dT[f] - xsec_temp[f]) / dt * nd +
             xsec_temp[f] * dnd_dt) *
            vmr;
      }

      for (auto&j : freq_jac) {
        if (j.first) {
          const auto iq = j.second->target_pos;
          dpropmat_clearsky_dx(iq, f).A() +=
              (dxsec_temp_dF[f] - xsec_temp[f]) * nd * vmr / df;
        }
      }

      if (const auto j =
              jacobian_targets.find<Jacobian::AtmTarget>(abs_species[i].Species());
          j.first) {
        const auto iq = j.second->target_pos;
        dpropmat_clearsky_dx(iq, f).A() += xsec_temp[f] * nd * vmr;
      }
    }
  }
}
