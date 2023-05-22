//! File contains ways to manipulate the propagation matrix

#include "agenda_class.h"
#include "auto_md.h"
#include "debug.h"
#include "jacobian.h"
#include "species_tags.h"

void propmat_clearskyAddScaledSpecies(  // Workspace reference:
    Workspace& ws,
    // WS Output:
    PropmatVector& propmat_clearsky,
    StokvecVector& nlte_source,
    // WS Input:
    const ArrayOfRetrievalQuantity& jacobian_quantities,
    const ArrayOfSpeciesTag& select_abs_species,
    const Vector& f_grid,
    const Vector& rtp_los,
    const AtmPoint& atm_point,
    const Agenda& propmat_clearsky_agenda,
    // WS Generic Input:
    const ArrayOfSpeciesTag& target,
    const Numeric& scale) {
  ARTS_USER_ERROR_IF(jacobian_quantities.nelem(), "Cannot use with derivatives")

  if (select_abs_species not_eq target) {
    ARTS_USER_ERROR_IF(
        select_abs_species.nelem(),
        "Non-empty select_abs_species (lookup table calculations set select_abs_species)")

    PropmatVector pm;
    StokvecVector sv;
    PropmatMatrix dpropmat_clearsky_dx;
    StokvecMatrix dnlte_source_dx;

    propmat_clearsky_agendaExecute(ws,
                                   pm,
                                   sv,
                                   dpropmat_clearsky_dx,
                                   dnlte_source_dx,
                                   jacobian_quantities,
                                   target,
                                   f_grid,
                                   rtp_los,
                                   atm_point,
                                   propmat_clearsky_agenda);

    ARTS_USER_ERROR_IF(propmat_clearsky.shape() not_eq pm.shape(), "Mismatching sizes")
    ARTS_USER_ERROR_IF(nlte_source.shape() not_eq sv.shape(), "Mismatching sizes")
    
    std::transform(propmat_clearsky.begin(),
                   propmat_clearsky.end(),
                   pm.begin(),
                   propmat_clearsky.begin(),
                   [scale](auto& a, auto& b) { return a + scale * b; });
    
    std::transform(nlte_source.begin(),
                   nlte_source.end(),
                   sv.begin(),
                   nlte_source.begin(),
                   [scale](auto& a, auto& b) { return a + scale * b; });
  }
}