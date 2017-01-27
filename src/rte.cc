/* Copyright (C) 2002-2012
   Patrick Eriksson <Patrick.Eriksson@chalmers.se>
   Stefan Buehler   <sbuehler@ltu.se>
                            
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA. */



/*===========================================================================
  === File description 
  ===========================================================================*/

/*!
  \file   rte.cc
  \author Patrick Eriksson <Patrick.Eriksson@chalmers.se>
  \date   2002-05-29

  \brief  Functions to solve radiative transfer tasks.
*/



/*===========================================================================
  === External declarations
  ===========================================================================*/

#include <cmath>
#include <stdexcept>
#include "auto_md.h"
#include "check_input.h"
#include "geodetic.h"
#include "logic.h"
#include "math_funcs.h"
#include "montecarlo.h"
#include "physics_funcs.h"
#include "ppath.h"
#include "rte.h"
#include "special_interp.h"
#include "lin_alg.h"

extern const Numeric SPEED_OF_LIGHT;



/*===========================================================================
  === The functions in alphabetical order
  ===========================================================================*/


//! adjust_los
/*!
    Ensures that the zenith and azimuth angles of a line-of-sight vector are
    inside defined ranges.

    This function should not be used blindly, just when you know that the
    out-of-bounds values are obtained by an OK operation. As when making a
    disturbance calculation where e.g. the zenith angle is shifted with a small
    value. This function then handles the case when the original zenith angle
    is 0 or 180 and the disturbance then moves the angle outside the defined
    range. 

    \param   los              In/Out: LOS vector, defined as e.g. rte_los.
    \param   atmosphere_dim   As the WSV.

    \author Patrick Eriksson 
    \date   2012-04-11
*/
void adjust_los( 
         VectorView   los, 
   const Index &      atmosphere_dim )
{
  if( atmosphere_dim == 1 )
    {
           if( los[0] <   0 ) { los[0] = -los[0];    }
      else if( los[0] > 180 ) { los[0] = 360-los[0]; }
    }
  else if( atmosphere_dim == 2 )
    {
           if( los[0] < -180 ) { los[0] = los[0] + 360; }
      else if( los[0] >  180 ) { los[0] = los[0] - 360; }
    }
  else 
    {
      // If any of the angles out-of-bounds, use cart2zaaa to resolve 
      if( abs(los[0]-90) > 90  ||  abs(los[1]) > 180 )
        {
          Numeric dx, dy, dz;
          zaaa2cart( dx, dy, dz, los[0], los[1] );
          cart2zaaa( los[0], los[1], dx, dy, dz );
        }        
    }
}



//! apply_iy_unit
/*!
    Performs conversion from radiance to other units, as well as applies
    refractive index to fulfill the n2-law of radiance.

    Use *apply_iy_unit2* for conversion of jacobian data.

    \param   iy       In/Out: Tensor3 with data to be converted, where 
                      column dimension corresponds to Stokes dimensionality
                      and row dimension corresponds to frequency.
    \param   iy_unit  As the WSV.
    \param   f_grid   As the WSV.
    \param   n        Refractive index at the observation position.
    \param   i_pol    Polarisation indexes. See documentation of y_pol.

    \author Patrick Eriksson 
    \date   2010-04-07
*/
void apply_iy_unit( 
            MatrixView   iy, 
         const String&   iy_unit, 
       ConstVectorView   f_grid,
   const Numeric&        n,
   const ArrayOfIndex&   i_pol )
{
  // The code is largely identical between the two apply_iy_unit functions.
  // If any change here, remember to update the other function.

  const Index nf = iy.nrows();
  const Index ns = iy.ncols();

  assert( f_grid.nelem() == nf );
  assert( i_pol.nelem() == ns );

  if( iy_unit == "1" )
    {
      if( n != 1 )
        { iy *= (n*n); }
    }

  else if( iy_unit == "RJBT" )
    {
      for( Index iv=0; iv<nf; iv++ )
        {
          const Numeric scfac = invrayjean( 1, f_grid[iv] );
          for( Index is=0; is<ns; is++ )
            {
              if( i_pol[is] < 5 )           // Stokes components
                { iy(iv,is) *= scfac; }
              else                          // Measuement single pols
                { iy(iv,is) *= 2*scfac; }
            }
        }
    }

  else if( iy_unit == "PlanckBT" )
    {
      for( Index iv=0; iv<nf; iv++ )
        {
          for( Index is=ns-1; is>=0; is-- ) // Order must here be reversed
            {
              if( i_pol[is] == 1 )
                { iy(iv,is) = invplanck( iy(iv,is), f_grid[iv] ); }
              else if( i_pol[is] < 5 )
                { 
                  assert( i_pol[0] == 1 );
                  iy(iv,is) = 
                    invplanck( 0.5*(iy(iv,0)+iy(iv,is)), f_grid[iv] ) -
                    invplanck( 0.5*(iy(iv,0)-iy(iv,is)), f_grid[iv] );
                }
              else
                { iy(iv,is) = invplanck( 2*iy(iv,is), f_grid[iv] ); }
            }
        }
    }
  
  else if ( iy_unit == "W/(m^2 m sr)" )
    {
      for( Index iv=0; iv<nf; iv++ )
        {
          const Numeric scfac = n*n * f_grid[iv] * (f_grid[iv]/SPEED_OF_LIGHT);
          for( Index is=0; is<ns; is++ )
            { iy(iv,is) *= scfac; }
        }
    }
  
  else if ( iy_unit == "W/(m^2 m-1 sr)" )
    {
      iy *= ( n * n * SPEED_OF_LIGHT );
    }

  else
    {
      ostringstream os;
      os << "Unknown option: iy_unit = \"" << iy_unit << "\"\n" 
         << "Recognised choices are: \"1\", \"RJBT\", \"PlanckBT\""
         << "\"W/(m^2 m sr)\" and \"W/(m^2 m-1 sr)\""; 
      
      throw runtime_error( os.str() );      
    }
}



//! apply_iy_unit2
/*!
    Largely as *apply_iy_unit* but operates on jacobian data.

    The associated spectrum data *iy* must be in radiance. That is, the
    spectrum can only be converted to Tb after the jacobian data. 

    \param   J        In/Out: Tensor3 with data to be converted, where 
                      column dimension corresponds to Stokes dimensionality
                      and row dimension corresponds to frequency.
    \param   iy       Associated radiance data.
    \param   iy_unit  As the WSV.
    \param   f_grid   As the WSV.
    \param   n        Refractive index at the observation position.
    \param   i_pol    Polarisation indexes. See documentation of y_pol.

    \author Patrick Eriksson 
    \date   2010-04-10
*/
void apply_iy_unit2( 
   Tensor3View           J,
   ConstMatrixView       iy, 
   const String&         iy_unit, 
   ConstVectorView       f_grid,
   const Numeric&        n,
   const ArrayOfIndex&   i_pol )
{
  // The code is largely identical between the two apply_iy_unit functions.
  // If any change here, remember to update the other function.

  const Index nf = iy.nrows();
  const Index ns = iy.ncols();
  const Index np = J.npages();

  assert( J.nrows() == nf );
  assert( J.ncols() == ns );
  assert( f_grid.nelem() == nf );
  assert( i_pol.nelem() == ns );

  if( iy_unit == "1" )
    {
      if( n != 1 )
        { J *= (n*n); }
    }

  else if( iy_unit == "RJBT" )
    {
      for( Index iv=0; iv<nf; iv++ )
        {
          const Numeric scfac = invrayjean( 1, f_grid[iv] );
          for( Index is=0; is<ns; is++ )
            {
              if( i_pol[is] < 5 )           // Stokes componenets
                {
                  for( Index ip=0; ip<np; ip++ )
                    { J(ip,iv,is) *= scfac; }
                }
              else                          // Measuement single pols
                {
                  for( Index ip=0; ip<np; ip++ )
                    { J(ip,iv,is) *= 2*scfac; }
                }
            }
        }
    }

  else if( iy_unit == "PlanckBT" )
    {
      for( Index iv=0; iv<f_grid.nelem(); iv++ )
        {
          for( Index is=ns-1; is>=0; is-- )
            {
              Numeric scfac = 1;
              if( i_pol[is] == 1 )
                { scfac = dinvplanckdI( iy(iv,is), f_grid[iv] ); }
              else if( i_pol[is] < 5 )
                {
                  assert( i_pol[0] == 1 );
                  scfac = 
                    dinvplanckdI( 0.5*(iy(iv,0)+iy(iv,is)), f_grid[iv] ) +
                    dinvplanckdI( 0.5*(iy(iv,0)-iy(iv,is)), f_grid[iv] );
                }
              else
                { scfac = dinvplanckdI( 2*iy(iv,is), f_grid[iv] ); }
              //
              for( Index ip=0; ip<np; ip++ )
                { J(ip,iv,is) *= scfac; }
            }
        }
    }

  else if ( iy_unit == "W/(m^2 m sr)" )
    {
      for( Index iv=0; iv<nf; iv++ )
        {
          const Numeric scfac = n*n * f_grid[iv] * (f_grid[iv]/SPEED_OF_LIGHT);
          for( Index ip=0; ip<np; ip++ )
            {
              for( Index is=0; is<ns; is++ )
                { J(ip,iv,is) *= scfac; }
            }
        }
    }
  
  else if ( iy_unit == "W/(m^2 m-1 sr)" )
    {
      J *= ( n *n * SPEED_OF_LIGHT );
    }
  
  else
    {
      ostringstream os;
      os << "Unknown option: iy_unit = \"" << iy_unit << "\"\n" 
         << "Recognised choices are: \"1\", \"RJBT\", \"PlanckBT\""
         << "\"W/(m^2 m sr)\" and \"W/(m^2 m-1 sr)\""; 
      
      throw runtime_error( os.str() );      
    }  
}



//! bending_angle1d
/*!
    Calculates the bending angle for a 1D atmosphere.

    The expression used assumes a 1D atmosphere, that allows the bending angle
    to be calculated by start and end LOS. This is an approximation for 2D and
    3D, but a very small one and the function should in general be OK also for
    2D and 3D.

    The expression is taken from Kursinski et al., The GPS radio occultation
    technique, TAO, 2000.

    \return   alpha   Bending angle
    \param    ppath   Propagation path.

    \author Patrick Eriksson 
    \date   2012-04-05
*/
void bending_angle1d( 
        Numeric&   alpha,
  const Ppath&     ppath )
{
  Numeric theta;
  if( ppath.dim < 3 )
    { theta = abs( ppath.start_pos[1] - ppath.end_pos[1] ); }
  else
    { theta = sphdist( ppath.start_pos[1], ppath.start_pos[2],
                       ppath.end_pos[1], ppath.end_pos[2] ); }

  // Eq 17 in Kursinski et al., TAO, 2000:
  alpha = ppath.start_los[0] - ppath.end_los[0] + theta;

  // This as
  // phi_r = 180 - ppath.end_los[0]
  // phi_t = ppath.start_los[0]
}



//! defocusing_general_sub
/*!
    Just to avoid duplicatuion of code in *defocusing_general*.
   
    rte_los is mainly an input, but is also returned "adjusted" (with zenith
    and azimuth angles inside defined ranges) 
 
    \param    pos                 Out: Position of ppath at optical distance lo0
    \param    rte_los             In/out: Direction for transmitted signal 
                                  (disturbed from nominal value)
    \param    rte_pos             Out: Position of transmitter.
    \param    background          Out: Raditaive background of ppath.
    \param    lo0                 Optical path length between transmitter 
                                  and receiver.
    \param    ppath_step_agenda   As the WSV with the same name.
    \param    atmosphere_dim      As the WSV with the same name.
    \param    p_grid              As the WSV with the same name.
    \param    lat_grid            As the WSV with the same name.
    \param    lon_grid            As the WSV with the same name.
    \param    t_field             As the WSV with the same name.
    \param    z_field             As the WSV with the same name.
    \param    vmr_field           As the WSV with the same name.
    \param    f_grid              As the WSV with the same name.
    \param    refellipsoid        As the WSV with the same name.
    \param    z_surface           As the WSV with the same name.
    \param    verbosity           As the WSV with the same name.

    \author Patrick Eriksson 
    \date   2012-04-11
*/
void defocusing_general_sub( 
        Workspace&   ws,
        Vector&      pos,
        Vector&      rte_los,
        Index&       background,
  ConstVectorView    rte_pos,
  const Numeric&     lo0,
  const Agenda&      ppath_step_agenda,
  const Numeric&     ppath_lmax,
  const Numeric&     ppath_lraytrace,
  const Index&       atmosphere_dim,
  ConstVectorView    p_grid,
  ConstVectorView    lat_grid,
  ConstVectorView    lon_grid,
  ConstTensor3View   t_field,
  ConstTensor3View   z_field,
  ConstTensor4View   vmr_field,
  ConstVectorView    f_grid,
  ConstVectorView    refellipsoid,
  ConstMatrixView    z_surface,
  const Verbosity&   verbosity )
{
  // Special treatment of 1D around zenith/nadir
  // (zenith angles outside [0,180] are changed by *adjust_los*)
  bool invert_lat = false;
  if( atmosphere_dim == 1  &&  ( rte_los[0] < 0 || rte_los[0] > 180 ) )
    { invert_lat = true; }

  // Handle cases where angles have moved out-of-bounds due to disturbance
  adjust_los( rte_los, atmosphere_dim );

  // Calculate the ppath for disturbed rte_los
  Ppath ppx;
  //
  ppath_calc( ws, ppx, ppath_step_agenda, atmosphere_dim, p_grid, lat_grid,
              lon_grid, t_field, z_field, vmr_field, 
              f_grid, refellipsoid, z_surface, 0, ArrayOfIndex(0), 
              rte_pos, rte_los, ppath_lmax, ppath_lraytrace, 0, verbosity );
  //
  background = ppath_what_background( ppx );

  // Calcualte cumulative optical path for ppx
  Vector lox( ppx.np );
  Index ilast = ppx.np-1;
  lox[0] = ppx.end_lstep;
  for( Index i=1; i<=ilast; i++ )
    { lox[i] = lox[i-1] + ppx.lstep[i-1] * ( ppx.nreal[i-1] + 
                                             ppx.nreal[i] ) / 2.0; }

  pos.resize( max( Index(2), atmosphere_dim ) );

  // Reciever at a longer distance (most likely out in space):
  if( lox[ilast] < lo0 )
    {
      const Numeric dl = lo0 - lox[ilast];
      if( atmosphere_dim < 3 )
        {
          Numeric x, z, dx, dz;
          poslos2cart( x, z, dx, dz, ppx.r[ilast], ppx.pos(ilast,1), 
                       ppx.los(ilast,0) );
          cart2pol( pos[0], pos[1], x+dl*dx, z+dl*dz, ppx.pos(ilast,1), 
                    ppx.los(ilast,0) );
        }
      else
        {
          Numeric x, y, z, dx, dy, dz;
          poslos2cart( x, y, z, dx, dy, dz, ppx.r[ilast], ppx.pos(ilast,1), 
                       ppx.pos(ilast,2), ppx.los(ilast,0), ppx.los(ilast,1) );
          cart2sph( pos[0], pos[1], pos[2], x+dl*dx, y+dl*dy, z+dl*dz, 
                    ppx.pos(ilast,1), ppx.pos(ilast,2), 
                    ppx.los(ilast,0), ppx.los(ilast,1) );
        }
    }

  // Interpolate to lo0
  else
    { 
      GridPos   gp;
      Vector    itw(2);
      gridpos( gp, lox, lo0 );
      interpweights( itw, gp );
      //
      pos[0] = interp( itw, ppx.r, gp );
      pos[1] = interp( itw, ppx.pos(joker,1), gp );
      if( atmosphere_dim == 3 )
        { pos[2] = interp( itw, ppx.pos(joker,2), gp ); }
    } 

  if( invert_lat )
    { pos[1] = -pos[1]; }
}


//! defocusing_general
/*!
    Defocusing for arbitrary geometry (zenith angle part only)

    Estimates the defocusing loss factor by calculating two paths with zenith
    angle off-sets. The distance between the two path at the optical path
    length between the transmitter and the receiver, divided with the
    corresponding distance for free space propagation, gives the defocusing
    loss. 

    The azimuth (gain) factor is not calculated. The path calculations are here
    done starting from the transmitter, which is the reversed direction
    compared to the ordinary path calculations starting at the receiver.
    
    \return   dlf                 Defocusing loss factor (1 for no loss)
    \param    ppath_step_agenda   As the WSV with the same name.
    \param    atmosphere_dim      As the WSV with the same name.
    \param    p_grid              As the WSV with the same name.
    \param    lat_grid            As the WSV with the same name.
    \param    lon_grid            As the WSV with the same name.
    \param    t_field             As the WSV with the same name.
    \param    z_field             As the WSV with the same name.
    \param    vmr_field           As the WSV with the same name.
    \param    f_grid              As the WSV with the same name.
    \param    refellipsoid        As the WSV with the same name.
    \param    z_surface           As the WSV with the same name.
    \param    ppath               As the WSV with the same name.
    \param    ppath_lmax          As the WSV with the same name.
    \param    ppath_lraytrace     As the WSV with the same name.
    \param    dza                 Size of angular shift to apply.
    \param    verbosity           As the WSV with the same name.

    \author Patrick Eriksson 
    \date   2012-04-11
*/
void defocusing_general( 
        Workspace&   ws,
        Numeric&     dlf,
  const Agenda&      ppath_step_agenda,
  const Index&       atmosphere_dim,
  ConstVectorView    p_grid,
  ConstVectorView    lat_grid,
  ConstVectorView    lon_grid,
  ConstTensor3View   t_field,
  ConstTensor3View   z_field,
  ConstTensor4View   vmr_field,
  ConstVectorView    f_grid,
  ConstVectorView    refellipsoid,
  ConstMatrixView    z_surface,
  const Ppath&       ppath,
  const Numeric&     ppath_lmax,
  const Numeric&     ppath_lraytrace,
  const Numeric&     dza,
  const Verbosity&   verbosity )
{
  // Optical and physical path between transmitter and reciver
  Numeric lo = ppath.start_lstep + ppath.end_lstep;
  Numeric lp = lo;
  for( Index i=0; i<=ppath.np-2; i++ )
    { lp += ppath.lstep[i];
      lo += ppath.lstep[i] * ( ppath.nreal[i] + ppath.nreal[i+1] ) / 2.0; 
    }
  // Extract rte_pos and rte_los
  const Vector rte_pos = ppath.start_pos[Range(0,atmosphere_dim)];
  //
  Vector rte_los0(max(Index(1),atmosphere_dim-1)), rte_los;
  mirror_los( rte_los, ppath.start_los, atmosphere_dim );
  rte_los0 = rte_los[Range(0,max(Index(1),atmosphere_dim-1))];

  // A new ppath with positive zenith angle off-set
  //
  Vector  pos1;
  Index   backg1;
  //
  rte_los     = rte_los0;
  rte_los[0] += dza;
  //
  defocusing_general_sub( ws, pos1, rte_los, backg1, rte_pos, lo, 
                          ppath_step_agenda, ppath_lmax, ppath_lraytrace,
                          atmosphere_dim, p_grid, lat_grid, lon_grid, t_field, z_field, 
                          vmr_field, f_grid, refellipsoid, 
                          z_surface, verbosity );

  // Same thing with negative zenit angle off-set
  Vector  pos2;
  Index   backg2;
  //
  rte_los     = rte_los0;  // Use rte_los0 as rte_los can have been "adjusted"
  rte_los[0] -= dza;
  //
  defocusing_general_sub( ws, pos2, rte_los, backg2, rte_pos, lo, 
                          ppath_step_agenda, ppath_lmax, ppath_lraytrace,
                          atmosphere_dim, p_grid, lat_grid, lon_grid, t_field, z_field, 
                          vmr_field, f_grid, refellipsoid, 
                          z_surface, verbosity );

  // Calculate distance between pos1 and 2, and derive the loss factor
  // All appears OK:
  if( backg1 == backg2 )
    {
      Numeric l12;
      if( atmosphere_dim < 3 )
        { distance2D( l12, pos1[0], pos1[1], pos2[0], pos2[1] ); }
      else
        { distance3D( l12, pos1[0], pos1[1], pos1[2], 
                           pos2[0], pos2[1], pos2[2] ); }
      //
      dlf = lp*2*DEG2RAD*dza /  l12;
    }
  // If different backgrounds, then only use the second calculation
  else
    {
      Numeric l12;
      if( atmosphere_dim == 1 )
        { 
          const Numeric r = refellipsoid[0];
          distance2D( l12, r+ppath.end_pos[0], 0, pos2[0], pos2[1] ); 
        }
      else if( atmosphere_dim == 2 )
        { 
          const Numeric r = refell2r( refellipsoid, ppath.end_pos[1] );
          distance2D( l12, r+ppath.end_pos[0], ppath.end_pos[1], 
                                                        pos2[0], pos2[1] ); 
        }
      else
        { 
          const Numeric r = refell2r( refellipsoid, ppath.end_pos[1] );
          distance3D( l12, r+ppath.end_pos[0], ppath.end_pos[1], 
                             ppath.end_pos[2], pos2[0], pos2[1], pos2[2] ); 
        }
      //
      dlf = lp*DEG2RAD*dza /  l12;
    }
}



//! defocusing_sat2sat
/*!
    Calculates defocusing for limb measurements between two satellites.

    The expressions used assume a 1D atmosphere, and can only be applied on
    limb sounding geometry. The function works for 2D and 3D and should give 
    OK estimates. Both the zenith angle (loss) and azimuth angle (gain) terms
    are considered.

    The expressions is taken from Kursinski et al., The GPS radio occultation
    technique, TAO, 2000.

    \return   dlf                 Defocusing loss factor (1 for no loss)
    \param    ppath_step_agenda   As the WSV with the same name.
    \param    atmosphere_dim      As the WSV with the same name.
    \param    p_grid              As the WSV with the same name.
    \param    lat_grid            As the WSV with the same name.
    \param    lon_grid            As the WSV with the same name.
    \param    t_field             As the WSV with the same name.
    \param    z_field             As the WSV with the same name.
    \param    vmr_field           As the WSV with the same name.
    \param    f_grid              As the WSV with the same name.
    \param    refellipsoid        As the WSV with the same name.
    \param    z_surface           As the WSV with the same name.
    \param    ppath               As the WSV with the same name.
    \param    ppath_lmax          As the WSV with the same name.
    \param    ppath_lraytrace     As the WSV with the same name.
    \param    dza                 Size of angular shift to apply.
    \param    verbosity           As the WSV with the same name.

    \author Patrick Eriksson 
    \date   2012-04-11
*/
void defocusing_sat2sat( 
        Workspace&   ws,
        Numeric&     dlf,
  const Agenda&      ppath_step_agenda,
  const Index&       atmosphere_dim,
  ConstVectorView    p_grid,
  ConstVectorView    lat_grid,
  ConstVectorView    lon_grid,
  ConstTensor3View   t_field,
  ConstTensor3View   z_field,
  ConstTensor4View   vmr_field,
  ConstVectorView    f_grid,
  ConstVectorView    refellipsoid,
  ConstMatrixView    z_surface,
  const Ppath&       ppath,
  const Numeric&     ppath_lmax,
  const Numeric&     ppath_lraytrace,
  const Numeric&     dza,
  const Verbosity&   verbosity )
{
  if( ppath.end_los[0] < 90  ||  ppath.start_los[0] > 90  )
     throw runtime_error( "The function *defocusing_sat2sat* can only be used "
                         "for limb sounding geometry." );

  // Index of tangent point
  Index it;
  find_tanpoint( it, ppath );
  assert( it >= 0 );

  // Length between tangent point and transmitter/reciver
  Numeric lt = ppath.start_lstep, lr = ppath.end_lstep;
  for( Index i=it; i<=ppath.np-2; i++ )
    { lt += ppath.lstep[i]; }
  for( Index i=0; i<it; i++ )
    { lr += ppath.lstep[i]; }

  // Bending angle and impact parameter for centre ray
  Numeric alpha0, a0;
  bending_angle1d( alpha0, ppath );
  alpha0 *= DEG2RAD;
  a0      = ppath.constant; 

  // Azimuth loss term (Eq 18.5 in Kursinski et al.)
  const Numeric lf = lr*lt / (lr+lt);
  const Numeric alt = 1 / ( 1 - alpha0*lf / refellipsoid[0] );

  // Calculate two new ppaths to get dalpha/da
  Numeric   alpha1, a1, alpha2, a2, dada;
  Ppath     ppt;
  Vector    rte_pos = ppath.end_pos[Range(0,atmosphere_dim)];
  Vector    rte_los = ppath.end_los;
  //
  rte_los[0] -= dza;
  adjust_los( rte_los, atmosphere_dim );
  ppath_calc( ws, ppt, ppath_step_agenda, atmosphere_dim, p_grid, lat_grid,
              lon_grid, t_field, z_field, vmr_field, 
              f_grid, refellipsoid, z_surface, 0, ArrayOfIndex(0), 
              rte_pos, rte_los, ppath_lmax, ppath_lraytrace, 0, verbosity );
  bending_angle1d( alpha2, ppt );
  alpha2 *= DEG2RAD;
  a2      = ppt.constant; 
  //
  rte_los[0] += 2*dza;
  adjust_los( rte_los, atmosphere_dim );
  ppath_calc( ws, ppt, ppath_step_agenda, atmosphere_dim, p_grid, lat_grid,
              lon_grid, t_field, z_field, vmr_field, 
              f_grid, refellipsoid, z_surface, 0, ArrayOfIndex(0), 
              rte_pos, rte_los, ppath_lmax, ppath_lraytrace, 0, verbosity );
  // This path can hit the surface. And we need to check if ppt is OK.
  // (remember this function only deals with sat-to-sat links and OK 
  // background here is be space) 
  // Otherwise use the centre ray as the second one.
  if( ppath_what_background(ppt) == 1 )
    {
      bending_angle1d( alpha1, ppt );
      alpha1 *= DEG2RAD;
      a1      = ppt.constant; 
      dada    = (alpha2-alpha1) / (a2-a1); 
    }
  else
    {
      dada    = (alpha2-alpha0) / (a2-a0);       
    }

  // Zenith loss term (Eq 18 in Kursinski et al.)
  const Numeric zlt = 1 / ( 1 - dada*lf );

  // Total defocusing loss
  dlf = zlt * alt;
}



//! dotprod_with_los
/*!
    Calculates the dot product between a field and a LOS
    The latter three gives the derivatives with respect to the LOS

    The line-of-sight shall be given as in the ppath structure (i.e. the
    viewing direction), but the dot product is calculated for the photon
    direction. The field is specified by its three components.

    The returned value can be written as |f|*cos(theta), where |f| is the field
    strength, and theta the angle between the field and photon vectors.

    \return                    The result of the dot product
    \param   los               Pppath line-of-sight.
    \param   u                 U-component of field.
    \param   v                 V-component of field.
    \param   w                 W-component of field.
    \param   atmosphere_dim    As the WSV.

    \author Patrick Eriksson 
    \date   2012-12-12
*/
Numeric dotprod_with_los(
  ConstVectorView   los, 
  const Numeric&    u,
  const Numeric&    v,
  const Numeric&    w,
  const Index&      atmosphere_dim )
{
  // Strength of field
  const Numeric f = sqrt( u*u + v*v + w*w );

  // Zenith and azimuth angle for field (in radians) 
  const Numeric za_f = acos( w/f );
  const Numeric aa_f = atan2( u, v );

  // Zenith and azimuth angle for photon direction (in radians)
  Vector los_p;
  mirror_los( los_p, los, atmosphere_dim );
  const Numeric za_p = DEG2RAD * los_p[0];
  const Numeric aa_p = DEG2RAD * los_p[1];
  
  return f * ( cos(za_f) * cos(za_p) +
               sin(za_f) * sin(za_p) * cos(aa_f-aa_p) );
}   


//! emission_rtstep
/*!
    Radiative transfer over a step, with emission.

    With LTE, in vector notation: iy = t*iy + (1-t)*B,

    and with non-LTE, in vector notation: iy = t*iy + (1-t)*B + (1-t)inv(extbar)*(Abar-extbar)*B
    
    where Jn = (Abar-extbar)*B is the non-LTE part of the source function as input through sourcebar,
    i.e., iy = t*iy + (1-t)* ( B + inv(extbar)*Jn )

    The calculations are done differently for extmat_case 1 and 2/3.

    Frequency is throughout leftmost dimension.

    \param   iy           In/out: Radiance values
    \param   stokes_dim   In: As the WSV.
    \param   bbar         In: Average of emission source function
    \param   extmat_case  In: As returned by get_ppath_trans, but just for the
                              frequency of cocncern.
    \param   t            In: Transmission matrix of the step.
    \param   nonlte       In: 0 if LTE applies, otherwise 1.
    \param   extbar       In: Average extinction matrix. Totally ignored if nonlte=0.
    \param   sourcebar    In: Average non-LTE source. Totally ignored if nonlte=0.

    \author Patrick Eriksson 
    \date   2013-04-19 (non-lte added 2015-05-31)
*/
void emission_rtstep(
          Matrix&         iy,
    const Index&          stokes_dim,
    ConstVectorView       bbar,
    const ArrayOfIndex&   extmat_case,
    ConstTensor3View      t,
    const bool&           nonlte,
    ConstTensor3View      extbar,
    ConstMatrixView       sourcebar )
{
  const Index nf = bbar.nelem();

  assert( t.ncols() == stokes_dim  &&  t.nrows() == stokes_dim ); 
  assert( t.npages() == nf );
  assert( extmat_case.nelem() == nf );

  //
  // LTE
  //
  if( !nonlte )
    {
      // LTE, scalar case
      if( stokes_dim == 1 )
        {
          for( Index iv=0; iv<nf; iv++ )  
            { iy(iv,0) = t(iv,0,0) * iy(iv,0) + ( 1 - t(iv,0,0) ) * bbar[iv]; }
        }

      // LTE, vector cases
      else
        {
#pragma omp parallel for      \
  if (!arts_omp_in_parallel()  \
      && nf >= arts_omp_get_max_threads())
          for( Index iv=0; iv<nf; iv++ )
            {
              assert( extmat_case[iv]>=1 && extmat_case[iv]<=3 );
              // Unpolarised absorption:
              if( extmat_case[iv] == 1 )
                {
                  iy(iv,0) = t(iv,0,0) * iy(iv,0) + ( 1 - t(iv,0,0) ) * bbar[iv];
                  for( Index is=1; is<stokes_dim; is++ )
                    { iy(iv,is) = t(iv,is,is) * iy(iv,is); }
                }
              // The general case:
              else
                {
                  // Transmitted term
                  Vector tt(stokes_dim);
                  mult( tt, t(iv,joker,joker), iy(iv,joker) );
                  // Add emission, first Stokes element
                  iy(iv,0) = tt[0] + ( 1 - t(iv,0,0) ) * bbar[iv];
                  // Remaining Stokes elements
                  for( Index i=1; i<stokes_dim; i++ )
                    { iy(iv,i) = tt[i] - t(iv,i,0) * bbar[iv]; }
                }
            }
        }
    }  // If LTE


  //
  // Non-LTE
  //
  else
    {
      //throw runtime_error( "Non-LTE not yet handled." );
      assert( extbar.ncols() == stokes_dim  &&  extbar.nrows() == stokes_dim ); 
      assert( extbar.npages() == nf );
      assert( sourcebar.ncols() == stokes_dim  &&  sourcebar.nrows() == nf ); 

      // non-LTE, scalar case
      if( stokes_dim == 1 )
        {
          for( Index iv=0; iv<nf; iv++ )  
            { iy(iv,0) = t(iv,0,0) * iy(iv,0) + ( 1 - t(iv,0,0) ) * ( bbar[iv] +
                                sourcebar(iv,0) / extbar(iv,0,0) ); }
        }

      // non-LTE, vector cases
      else
        {
#pragma omp parallel for      \
  if (!arts_omp_in_parallel()  \
      && nf >= arts_omp_get_max_threads())
          for( Index iv=0; iv<nf; iv++ )
            {
              assert( extmat_case[iv]>=1 && extmat_case[iv]<=3 );
              // Unpolarised extinction:
              if( extmat_case[iv] == 1 )
                {
                  iy(iv,0) = t(iv,0,0) * iy(iv,0) + ( 1 - t(iv,0,0) ) * ( bbar[iv] +
                                     sourcebar(iv,0) / extbar(iv,0,0) );
                  for( Index is=1; is<stokes_dim; is++ )
                    { iy(iv,is) = t(iv,is,is) * iy(iv,is); }
                }
              // The general case:
              else
                {
                  // Transmitted term
                  Vector tt(stokes_dim);
                  mult( tt, t(iv,joker,joker), iy(iv,joker) );
                  
                  // Source term  (full matrix multiplications since it can be polarized)
                  Matrix tmp(stokes_dim,stokes_dim);
                  Vector J_n(stokes_dim), J_bar(stokes_dim);
                  inv ( tmp, extbar(iv,joker,joker) );          // tmp   =  1/K
                  mult( J_n, tmp, sourcebar(iv, joker) );       // J_n   =  1/K * j_other
                  J_n[0]+=bbar[iv];                             // J_n   =  1/K * j_other + B... Source function!
                  id_mat(tmp);                                  // tmp   =  I
                  tmp-=t( iv, joker, joker);                    // tmp   =  I-T
                  mult(J_bar, tmp, J_n);                        // J_bar = (I-T) * (1/K * j_other + B)
                  
                  // Create final iy
                  for( Index i=0; i<stokes_dim; i++ )
                    { iy(iv,i) = tt[i] + J_bar[i]; }
                }
            }
        }
    }
}

 

//! 
/*!
    Converts an extinction matrix to a transmission matrix

    The function performs the calculations differently depending on the
    conditions, to improve the speed. There are three cases: <br>
       1. Scalar RT and/or the matrix ext_mat_av is diagonal. <br>
       2. Special expression for "horizontally_aligned" case. <br>
       3. The total general case.

    If the structure of *ext_mat* is known, *icase* can be set to "case index"
    (1, 2 or 3) and some time is saved. This includes that no asserts are
    performed on *ext_mat*.

    Otherwise, *icase* must be set to 0. *ext_mat* is then analysed and *icase*
    is set by the function and is returned.

    trans_mat must be sized before calling the function.

    \param   trans_mat          Input/Output: Transmission matrix of slab.
    \param   icase              Input/Output: Index giving ext_mat case.
    \param   ext_mat            Input: Averaged extinction matrix.
    \param   lstep              Input: The length of the RTE step.

    \author Patrick Eriksson (based on earlier version started by Claudia)
    \date   2013-05-17 
*/
void ext2trans(
         MatrixView   trans_mat,
         Index&       icase,
   ConstMatrixView    ext_mat,
   const Numeric&     lstep )
{
  const Index stokes_dim = ext_mat.ncols();

  assert( ext_mat.nrows()==stokes_dim );
  assert( trans_mat.nrows()==stokes_dim && trans_mat.ncols()==stokes_dim );

  // Theoretically ext_mat(0,0) >= 0, but to demand this can cause problems for
  // iterative retrievals, and the assert is skipped. Negative should be a
  // result of negative vmr, and this issue is checked in basics_checkedCalc.
  //assert( ext_mat(0,0) >= 0 );     

  assert( icase>=0 && icase<=3 );
  assert( !is_singular( ext_mat ) );
  assert( lstep >= 0 );

  // Analyse ext_mat?
  if( icase == 0 )
    { 
      icase = 1;  // Start guess is diagonal

      //--- Scalar case ----------------------------------------------------------
      if( stokes_dim == 1 )
        {}

      //--- Vector RT ------------------------------------------------------------
      else
        {
          // Check symmetries and analyse structure of exp_mat:
          assert( ext_mat(1,1) == ext_mat(0,0) );
          assert( ext_mat(1,0) == ext_mat(0,1) );

          if( ext_mat(1,0) != 0 )
            { icase = 2; }
      
          if( stokes_dim >= 3 )
            {     
              assert( ext_mat(2,2) == ext_mat(0,0) );
              assert( ext_mat(2,1) == -ext_mat(1,2) );
              assert( ext_mat(2,0) == ext_mat(0,2) );
              
              if( ext_mat(2,0) != 0  ||  ext_mat(2,1) != 0 )
                { icase = 3; }

              if( stokes_dim > 3 )  
                {
                  assert( ext_mat(3,3) == ext_mat(0,0) );
                  assert( ext_mat(3,2) == -ext_mat(2,3) );
                  assert( ext_mat(3,1) == -ext_mat(1,3) );
                  assert( ext_mat(3,0) == ext_mat(0,3) ); 

                  if( icase < 3 )  // if icase==3, already at most complex case
                    {
                      if( ext_mat(3,0) != 0  ||  ext_mat(3,1) != 0 )
                        { icase = 3; }
                      else if( ext_mat(3,2) != 0 )
                        { icase = 2; }
                    }
                }
            }
        }
    }


  // Calculation options:
  if( icase == 1 )
    {
      trans_mat = 0;
      trans_mat(0,0) = exp( -ext_mat(0,0) * lstep );
      for( Index i=1; i<stokes_dim; i++ )
        { trans_mat(i,i) = trans_mat(0,0); }
    }
      
  else if( icase == 2 )
    {
      // Expressions below are found in "Polarization in Spectral Lines" by
      // Landi Degl'Innocenti and Landolfi (2004).
      const Numeric tI = exp( -ext_mat(0,0) * lstep );
      const Numeric HQ = ext_mat(0,1) * lstep;
      trans_mat(0,0) = tI * cosh( HQ );
      trans_mat(1,1) = trans_mat(0,0);
      trans_mat(1,0) = -tI * sinh( HQ );
      trans_mat(0,1) = trans_mat(1,0);
      if( stokes_dim >= 3 )
        {
          trans_mat(2,0) = 0;
          trans_mat(2,1) = 0;
          trans_mat(0,2) = 0;
          trans_mat(1,2) = 0;
          const Numeric RQ = ext_mat(2,3) * lstep;
          trans_mat(2,2) = tI * cos( RQ );
          if( stokes_dim > 3 )
            {
              trans_mat(3,0) = 0;
              trans_mat(3,1) = 0;
              trans_mat(0,3) = 0;
              trans_mat(1,3) = 0;
              trans_mat(3,3) = trans_mat(2,2);
              trans_mat(3,2) = tI * sin( RQ );
              trans_mat(2,3) = -trans_mat(3,2); 
            }
        }
    }
  else
    {
      Matrix ext_mat_ds = ext_mat;
      ext_mat_ds *= -lstep; 
      //         
      Index q = 10;  // index for the precision of the matrix exp function
      //
      matrix_exp( trans_mat, ext_mat_ds, q );
    }
}


//! 
/*!
 *   Converts an extinction matrix to a transmission matrix
 * 
 *   The function performs the calculations differently depending on the
 *   conditions, to improve the speed. There are three cases: <br>
 *      1. Scalar RT and/or the matrix ext_mat_av is diagonal. <br>
 *      2. Special expression for "horizontally_aligned" case. <br>
 *      3. The total general case.
 * 
 *   If the structure of *ext_mat* is known, *icase* can be set to "case index"
 *   (1, 2 or 3) and some time is saved. This includes that no asserts are
 *   performed on *ext_mat*.
 * 
 *   Otherwise, *icase* must be set to 0. *ext_mat* is then analysed and *icase*
 *   is set by the function and is returned.
 * 
 *   trans_mat must be sized before calling the function.
 * 
 *   \param   trans_mat          Input/Output: Transmission matrix of slab.
 *   \param   dtrans_mat_dx_upp  Input/Output: Derivative with respect to level indexed above.
 *   \param   dtrans_mat_dx_low  Input/Output: Derivative with respect to level indexed below.
 *   \param   icase              Input/Output: Index giving ext_mat case.
 *   \param   ext_mat            Input: Averaged extinction matrix.
 *   \param   dext_mat_dx_upp    Input: Derivative with respect to level indexed above.
 *   \param   dext_mat_dx_low    Input: Derivative with respect to level indexed below.
 *   \param   lstep              Input: The length of the RTE step.
 * 
 *   \author Patrick Eriksson (based on earlier version started by Claudia)
 *   \date   2013-05-17 
 * 
 *   Added support for partial derivatives (copy-change of original function)
 *   \author Richard Larsson
 *   \date   2015-10-06
 */
void ext2trans_and_ext2dtrans_dx(
    MatrixView   trans_mat,
    Tensor3View  dtrans_mat_dx_upp,
    Tensor3View  dtrans_mat_dx_low,
    Index&       icase,
    ConstMatrixView    ext_mat,
    ConstTensor3View   dext_mat_dx_upp,
    ConstTensor3View   dext_mat_dx_low,
    const Numeric&     lstep )
{
    const Index stokes_dim = ext_mat.ncols();
    const Index nq         = dext_mat_dx_upp.npages();
    
    assert( ext_mat.nrows()==stokes_dim );
    assert( trans_mat.nrows()==stokes_dim && trans_mat.ncols()==stokes_dim );
    
    // Theoretically ext_mat(0,0) >= 0, but to demand this can cause problems for
    // iterative retrievals, and the assert is skipped. Negative should be a
    // result of negative vmr, and this issue is checked in basics_checkedCalc.
    //assert( ext_mat(0,0) >= 0 );     
    
    assert( icase>=0 && icase<=3 );
    assert( !is_singular( ext_mat ) );
    assert( lstep >= 0 );
    
    // Analyse ext_mat?
    if( icase == 0 )
    { 
        icase = 1;  // Start guess is diagonal
        
        //--- Scalar case ----------------------------------------------------------
        if( stokes_dim == 1 )
        {}
        
        //--- Vector RT ------------------------------------------------------------
        else
        {
            // Check symmetries and analyse structure of exp_mat:
            assert( ext_mat(1,1) == ext_mat(0,0) );
            assert( ext_mat(1,0) == ext_mat(0,1) );
            
            if( ext_mat(1,0) != 0 )
            { icase = 2; }
            
            if( stokes_dim >= 3 )
            {     
                assert( ext_mat(2,2) == ext_mat(0,0) );
                assert( ext_mat(2,1) == -ext_mat(1,2) );
                assert( ext_mat(2,0) == ext_mat(0,2) );
                
                if( ext_mat(2,0) != 0  ||  ext_mat(2,1) != 0 )
                { icase = 3; }
                
                if( stokes_dim > 3 )  
                {
                    assert( ext_mat(3,3) == ext_mat(0,0) );
                    assert( ext_mat(3,2) == -ext_mat(2,3) );
                    assert( ext_mat(3,1) == -ext_mat(1,3) );
                    assert( ext_mat(3,0) == ext_mat(0,3) ); 
                    
                    if( icase < 3 )  // if icase==3, already at most complex case
                    {
                        if( ext_mat(3,0) != 0  ||  ext_mat(3,1) != 0 )
                        { icase = 3; }
                        else if( ext_mat(3,2) != 0 )
                        { icase = 2; }
                    }
                }
            }
        }
    }
    
    
    // Calculation options:
    if( icase == 1 )
    {
        trans_mat = 0;
        trans_mat(0,0) = exp( -ext_mat(0,0) * lstep );
        for( Index i=1; i<stokes_dim; i++ )
        { trans_mat(i,i) = trans_mat(0,0); }
        
        // Set derivatives here to allow later code an easier time
        for( Index iq =0; iq<nq; iq++ )
        {
            dtrans_mat_dx_upp(iq,0,0) = - trans_mat(0,0) * lstep * dext_mat_dx_upp(iq,0,0);
            dtrans_mat_dx_low(iq,0,0) = - trans_mat(0,0) * lstep * dext_mat_dx_low(iq,0,0);
            for( Index i=1; i<stokes_dim; i++ )
            {
                dtrans_mat_dx_upp(iq,i,i) = dtrans_mat_dx_upp(iq,0,0);
                dtrans_mat_dx_low(iq,i,i) = dtrans_mat_dx_low(iq,0,0);
            }
        }
    }
    /*  Removed temporarily FIXME:  What is this, how do I use it???
    else if( icase == 2 )
    {
        // Expressions below are found in "Polarization in Spectral Lines" by
        // Landi Degl'Innocenti and Landolfi (2004).
        const Numeric tI = exp( -ext_mat(0,0) * lstep );
        const Numeric HQ = ext_mat(0,1) * lstep;
        trans_mat(0,0) = tI * cosh( HQ );
        trans_mat(1,1) = trans_mat(0,0);
        trans_mat(1,0) = -tI * sinh( HQ );
        trans_mat(0,1) = trans_mat(1,0);
        if( stokes_dim >= 3 )
        {
            trans_mat(2,0) = 0;
            trans_mat(2,1) = 0;
            trans_mat(0,2) = 0;
            trans_mat(1,2) = 0;
            const Numeric RQ = ext_mat(2,3) * lstep;
            trans_mat(2,2) = tI * cos( RQ );
            if( stokes_dim > 3 )
            {
                trans_mat(3,0) = 0;
                trans_mat(3,1) = 0;
                trans_mat(0,3) = 0;
                trans_mat(1,3) = 0;
                trans_mat(3,3) = trans_mat(2,2);
                trans_mat(3,2) = tI * sin( RQ );
                trans_mat(2,3) = -trans_mat(3,2); 
            }
        }
    }*/
    else
    {
        Matrix ext_mat_ds(stokes_dim,stokes_dim);
        Tensor3 dext_mat_ds_dx_upp(nq,stokes_dim,stokes_dim),dext_mat_ds_dx_low(nq,stokes_dim,stokes_dim);
        for(Index is1=0; is1<stokes_dim; is1++ )
        {
            for(Index is2=0; is2<stokes_dim; is2++ )
            {
                ext_mat_ds(is1,is2) = - ext_mat(is1,is2)*lstep;
                for(Index iq=0; iq<nq; iq++ )
                {
                    dext_mat_ds_dx_upp(iq,is1,is2) = - dext_mat_dx_upp(iq,is1,is2)*lstep;
                    dext_mat_ds_dx_low(iq,is1,is2) = - dext_mat_dx_low(iq,is1,is2)*lstep;
                }
            }
        }
        //         
        Index q = 10;  // index for the precision of the matrix exp function
        //
        special_matrix_exp_and_dmatrix_exp_dx_for_rt( trans_mat, dtrans_mat_dx_upp, dtrans_mat_dx_low,
                                ext_mat_ds, dext_mat_ds_dx_upp, dext_mat_ds_dx_low, q );
    }
}




//! get_iy
/*!
    Basic call of *iy_main_agenda*.

    This function is an interface to *iy_main_agenda* that can be used when
    only *iy* is of interest. That is, jacobian and auxilary parts are
    deactivated/ignored.

    \param   ws                    Out: The workspace
    \param   iy                    Out: As the WSV.
    \param   t_field               As the WSV.
    \param   z_field               As the WSV.
    \param   vmr_field             As the WSV.
    \param   cloudbox_on           As the WSV.
    \param   rte_pos               As the WSV.
    \param   rte_los               As the WSV.
    \param   iy_unit               As the WSV.
    \param   iy_main_agenda        As the WSV.

    \author Patrick Eriksson 
    \date   2012-08-08
*/
void get_iy(
         Workspace&   ws,
         Matrix&      iy,
   ConstTensor3View   t_field,
   ConstTensor3View   z_field,
   ConstTensor4View   vmr_field,
   const Index&       cloudbox_on,
   ConstVectorView    f_grid,
   ConstVectorView    rte_pos,
   ConstVectorView    rte_los,
   ConstVectorView    rte_pos2,
   const String&      iy_unit,
   const Agenda&      iy_main_agenda )
{
  ArrayOfTensor3    diy_dx;
  ArrayOfTensor4    iy_aux;
  Ppath             ppath;
  Tensor3           iy_transmission(0,0,0);
  const Index       iy_agenda_call1 = 1;
  const ArrayOfString iy_aux_vars(0);
  const Index       iy_id = 0;
  const Index       jacobian_do = 0;

  iy_main_agendaExecute( ws, iy, iy_aux, ppath, diy_dx, 
                         iy_agenda_call1, iy_unit, iy_transmission, iy_aux_vars,
                         iy_id, cloudbox_on, jacobian_do, t_field, z_field,
                         vmr_field, f_grid, rte_pos, rte_los, rte_pos2,
                         iy_main_agenda );
}




//! get_iy_of_background
/*!
    Determines iy of the "background" of a propgation path.

    The task is to determine *iy* and related variables for the
    background, or to continue the raditiave calculations
    "backwards". The details here depends on the method selected for
    the agendas.

    Each background is handled by an agenda. Several of these agandes
    can involve recursive calls of *iy_main_agenda*. 

    \param   ws                    Out: The workspace
    \param   iy                    Out: As the WSV.
    \param   diy_dx                Out: As the WSV.
    \param   iy_transmission       As the WSV.
    \param   jacobian_do           As the WSV.
    \param   ppath                 As the WSV.
    \param   atmosphere_dim        As the WSV.
    \param   t_field               As the WSV.
    \param   z_field               As the WSV.
    \param   vmr_field             As the WSV.
    \param   cloudbox_on           As the WSV.
    \param   stokes_dim            As the WSV.
    \param   f_grid                As the WSV.
    \param   iy_unit               As the WSV.    
    \param   iy_main_agenda        As the WSV.
    \param   iy_space_agenda       As the WSV.
    \param   iy_surface_agenda     As the WSV.
    \param   iy_cloudbox_agenda    As the WSV.

    \author Patrick Eriksson 
    \date   2009-10-08
*/
void get_iy_of_background(
        Workspace&        ws,
        Matrix&           iy,
        ArrayOfTensor3&   diy_dx,
  ConstTensor3View        iy_transmission,
  const Index&            iy_id, 
  const Index&            jacobian_do,
  const Ppath&            ppath,
  ConstVectorView         rte_pos2,
  const Index&            atmosphere_dim,
  ConstTensor3View        t_field,
  ConstTensor3View        z_field,
  ConstTensor4View        vmr_field,
  const Index&            cloudbox_on,
  const Index&            stokes_dim,
  ConstVectorView         f_grid,
  const String&           iy_unit,  
  const Agenda&           iy_main_agenda,
  const Agenda&           iy_space_agenda,
  const Agenda&           iy_surface_agenda,
  const Agenda&           iy_cloudbox_agenda,
  const Verbosity&        verbosity)
{
  CREATE_OUT3;
  
  // Some sizes
  const Index nf = f_grid.nelem();
  const Index np = ppath.np;

  // Set rtp_pos and rtp_los to match the last point in ppath.
  //
  // Note that the Ppath positions (ppath.pos) for 1D have one column more
  // than expected by most functions. Only the first atmosphere_dim values
  // shall be copied.
  //
  Vector rtp_pos, rtp_los;
  rtp_pos.resize( atmosphere_dim );
  rtp_pos = ppath.pos(np-1,Range(0,atmosphere_dim));
  rtp_los.resize( ppath.los.ncols() );
  rtp_los = ppath.los(np-1,joker);

  out3 << "Radiative background: " << ppath.background << "\n";


  // Handle the different background cases
  //
  String agenda_name;
  // 
  switch( ppath_what_background( ppath ) )
    {

    case 1:   //--- Space ---------------------------------------------------- 
      {
        agenda_name = "iy_space_agenda";
        chk_not_empty( agenda_name, iy_space_agenda );
        iy_space_agendaExecute( ws, iy, f_grid, rtp_pos, rtp_los, 
                                iy_space_agenda );
      }
      break;

    case 2:   //--- The surface -----------------------------------------------
      {
        agenda_name = "iy_surface_agenda";
        chk_not_empty( agenda_name, iy_surface_agenda );
        //
        const Index los_id = iy_id % (Index)1000;
        Index iy_id_new = iy_id + (Index)9*los_id; 
        //
        iy_surface_agendaExecute( ws, iy, diy_dx, 
                                  iy_unit, iy_transmission, iy_id_new, cloudbox_on,
                                  jacobian_do, t_field, z_field, vmr_field,
                                  f_grid, iy_main_agenda, rtp_pos, rtp_los, 
                                  rte_pos2, iy_surface_agenda );
      }
      break;

    case 3:   //--- Cloudbox boundary or interior ------------------------------
    case 4:
      {
        agenda_name = "iy_cloudbox_agenda";
        chk_not_empty( agenda_name, iy_cloudbox_agenda );
        iy_cloudbox_agendaExecute( ws, iy, f_grid, rtp_pos, rtp_los, 
                                   iy_cloudbox_agenda );
      }
      break;

    default:  //--- ????? ----------------------------------------------------
      // Are we here, the coding is wrong somewhere
      assert( false );
    }

  if( iy.ncols() != stokes_dim  ||  iy.nrows() != nf )
    {
      ostringstream os;
      os << "The size of *iy* returned from *" << agenda_name << "* is\n"
         << "not correct:\n"
         << "  expected size = [" << nf << "," << stokes_dim << "]\n"
         << "  size of iy    = [" << iy.nrows() << "," << iy.ncols()<< "]\n";
      throw runtime_error( os.str() );      
    }
}



//! get_ppath_atmvars
/*!
    Determines pressure, temperature, VMR, winds and magnetic field for each
    propgataion path point.

    The output variables are sized inside the function. For VMR the
    dimensions are [ species, propagation path point ].

    \param   ppath_p           Out: Pressure for each ppath point.
    \param   ppath_t           Out: Temperature for each ppath point.
    \param   ppath_vmr         Out: VMR values for each ppath point.
    \param   ppath_wind        Out: Wind vector for each ppath point.
    \param   ppath_mag         Out: Mag. field vector for each ppath point.
    \param   ppath             As the WSV.
    \param   atmosphere_dim    As the WSV.
    \param   p_grid            As the WSV.
    \param   lat_grid          As the WSV.
    \param   lon_grid          As the WSV.
    \param   t_field           As the WSV.
    \param   vmr_field         As the WSV.
    \param   wind_u_field      As the WSV.
    \param   wind_v_field      As the WSV.
    \param   wind_w_field      As the WSV.
    \param   mag_u_field       As the WSV.
    \param   mag_v_field       As the WSV.
    \param   mag_w_field       As the WSV.

    \author Patrick Eriksson 
    \date   2009-10-05
*/
void get_ppath_atmvars( 
        Vector&      ppath_p, 
        Vector&      ppath_t, 
        Matrix&      ppath_t_nlte,
        Matrix&      ppath_vmr, 
        Matrix&      ppath_wind, 
        Matrix&      ppath_mag,
  const Ppath&       ppath,
  const Index&       atmosphere_dim,
  ConstVectorView    p_grid,
  ConstTensor3View   t_field,
  ConstTensor4View   t_nlte_field,
  ConstTensor4View   vmr_field,
  ConstTensor3View   wind_u_field,
  ConstTensor3View   wind_v_field,
  ConstTensor3View   wind_w_field,
  ConstTensor3View   mag_u_field,
  ConstTensor3View   mag_v_field,
  ConstTensor3View   mag_w_field )
{
  const Index   np  = ppath.np;
  // Pressure:
  ppath_p.resize(np);
  Matrix itw_p(np,2);
  interpweights( itw_p, ppath.gp_p );      
  itw2p( ppath_p, p_grid, ppath.gp_p, itw_p );
  
  // Temperature:
  ppath_t.resize(np);
  Matrix   itw_field;
  interp_atmfield_gp2itw( itw_field, atmosphere_dim, 
                          ppath.gp_p, ppath.gp_lat, ppath.gp_lon );
  interp_atmfield_by_itw( ppath_t,  atmosphere_dim, t_field, 
                          ppath.gp_p, ppath.gp_lat, ppath.gp_lon, itw_field );

  // VMR fields:
  const Index ns = vmr_field.nbooks();
  ppath_vmr.resize(ns,np);
  for( Index is=0; is<ns; is++ )
    {
      interp_atmfield_by_itw( ppath_vmr(is, joker), atmosphere_dim,
                              vmr_field( is, joker, joker, joker ), 
                              ppath.gp_p, ppath.gp_lat, ppath.gp_lon, 
                              itw_field );
    }
    
  // NLTE temperatures
  const Index nnlte = t_nlte_field.nbooks();
  if( nnlte )
    ppath_t_nlte.resize(nnlte, np);
  else 
    ppath_t_nlte.resize(0,0);
  for( Index itnlte=0; itnlte<nnlte;itnlte++ )
  {
    interp_atmfield_by_itw( ppath_t_nlte(itnlte, joker),  atmosphere_dim, 
                            t_nlte_field( itnlte, joker, joker, joker ), 
                            ppath.gp_p, ppath.gp_lat, ppath.gp_lon, itw_field );
  }

  // Winds:
  ppath_wind.resize(3,np);
  ppath_wind = 0;
  //
  if( wind_u_field.npages() > 0 ) 
    { 
      interp_atmfield_by_itw( ppath_wind(0,joker), atmosphere_dim, wind_u_field,
                            ppath.gp_p, ppath.gp_lat, ppath.gp_lon, itw_field );
    }
  if( wind_v_field.npages() > 0 ) 
    { 
      interp_atmfield_by_itw( ppath_wind(1,joker), atmosphere_dim, wind_v_field,
                            ppath.gp_p, ppath.gp_lat, ppath.gp_lon, itw_field );
    }
  if( wind_w_field.npages() > 0 ) 
    { 
      interp_atmfield_by_itw( ppath_wind(2,joker), atmosphere_dim, wind_w_field,
                            ppath.gp_p, ppath.gp_lat, ppath.gp_lon, itw_field );
    }

  // Magnetic field:
  ppath_mag.resize(3,np);
  ppath_mag = 0;
  //
  if( mag_u_field.npages() > 0 )
    {
      interp_atmfield_by_itw( ppath_mag(0,joker), atmosphere_dim, mag_u_field,
                            ppath.gp_p, ppath.gp_lat, ppath.gp_lon, itw_field );
    }
  if( mag_v_field.npages() > 0 )
    {
      interp_atmfield_by_itw( ppath_mag(1,joker), atmosphere_dim, mag_v_field,
                            ppath.gp_p, ppath.gp_lat, ppath.gp_lon, itw_field );
    }
  if( mag_w_field.npages() > 0 )
    {
      interp_atmfield_by_itw( ppath_mag(2,joker), atmosphere_dim, mag_w_field,
                            ppath.gp_p, ppath.gp_lat, ppath.gp_lon, itw_field );
    }
}



//! get_ppath_pmat
/*!
    Determines the "clearsky" propagation matrices along a propagation path.
    The output arguments are:

    *ppath_ext* returns the summed extinction (propmat_clearsky) and has
       dimensions [ frequency, stokes, stokes, ppath point ].

    *ppath_abs* returns the summed source relevant absorption
       (propmat_source_clearsky) and has dimensions [ frequency, stokes, ppath
       point ]. Where lte=1, the data are identical to matching data in
       *ppath_ext*.

    *lte* flags if LTE is valid or not, at each path point. This variable is
       set to 1 if *propmat_clearsky_agenda* returns an empty
       *propmat_source_clearsky*. Otherwise zero.

    *abs_per_species* can hold absorption for individual species. The species
      to include are selected by *ispecies*. For example, to store first and
      third species in abs_per_species, set ispecies to [0][2]. The dimensions
      are [ absorption species, frequency, stokes, stokes, ppath point ].

    \param   ws                  Out: The workspace
    \param   ppath_ext           Out: Summed extinction at each ppath point
    \param   ppath_abs           Out: Summed absorption at each ppath point
    \param   ppath_nlte_source   Out: Summed nlte source at each ppath point
    \param   abs_per_species     Out: Absorption for "ispecies"
    \param   propmat_clearsky_agenda As the WSV.    
    \param   ppath               As the WSV.    
    \param   ppath_p             Pressure for each ppath point.
    \param   ppath_t             Temperature for each ppath point.
    \param   ppath_vmr           VMR values for each ppath point.
    \param   ppath_f             See get_ppath_f.
    \param   ppath_mag           See get_ppath_atmvars.
    \param   f_grid              As the WSV.    
    \param   stokes_dim          As the WSV.
    \param   ispecies            Index of species to store in abs_per_species

    \author Patrick Eriksson 
    \date   2012-08-15
*/
void get_ppath_pmat( 
        Workspace&      ws,
        Tensor4&        ppath_ext,
        Tensor3&        ppath_nlte_source,
        ArrayOfIndex&   lte,
        Tensor5&        abs_per_species,
        Tensor5&        dppath_ext_dx,
        Tensor4&        dppath_nlte_source_dx,
  const Agenda&         propmat_clearsky_agenda,
  const ArrayOfRetrievalQuantity& jacobian_quantities,
  const Ppath&          ppath,
  ConstVectorView       ppath_p, 
  ConstVectorView       ppath_t, 
  ConstMatrixView       ppath_t_nlte, 
  ConstMatrixView       ppath_vmr, 
  ConstMatrixView       ppath_f, 
  ConstMatrixView       ppath_mag,
  ConstVectorView       f_grid, 
  const Index&          stokes_dim,
  const ArrayOfIndex&   ispecies )
{
  // Helper variable
  const PropmatPartialsData ppd(jacobian_quantities);
    
  // Sizes
  const Index   nf   = f_grid.nelem();
  const Index   np   = ppath.np;
  const Index   nabs = ppath_vmr.nrows();
  const Index   nisp = ispecies.nelem();
  const Index   nq   = ppd.nelem();

  DEBUG_ONLY(
    for( Index i=0; i<nisp; i++ )
      {
        assert( ispecies[i] >= 0 );
        assert( ispecies[i] < nabs );
      }
  )

  // Size variables
  //
  try 
    {
      ppath_ext.resize( nf, stokes_dim, stokes_dim, np ); 
      ppath_nlte_source.resize( nf, stokes_dim, np );   // We start by assuming non-LTE
      lte.resize( np );
      abs_per_species.resize( nisp, nf, stokes_dim, stokes_dim, np ); 
      nq?dppath_ext_dx.resize(nq, nf, stokes_dim, stokes_dim, np ):
         dppath_ext_dx.resize( 0,  0,          0,          0,  0 );
      nq?dppath_nlte_source_dx.resize(nq, nf, stokes_dim, np):
         dppath_nlte_source_dx.resize( 0,  0,          0,  0);
    } 
  catch (std::bad_alloc x) 
    {
      ostringstream os;
      os << "Run-time error in function: get_ppath_pmat" << endl
         << "Memory allocation failed for ppath_ext("
         << nabs << ", " << nf << ", " << stokes_dim << ", "
         << stokes_dim << ", " << np << ")" << endl;
      throw runtime_error(os.str());
    }

  String fail_msg;
  bool failed = false;

  // Loop ppath points
  //
  Workspace l_ws (ws);
  Agenda l_propmat_clearsky_agenda (propmat_clearsky_agenda);
  //
  if (np)
#pragma omp parallel for                    \
  if (!arts_omp_in_parallel()               \
      && np >= arts_omp_get_max_threads())  \
  firstprivate(l_ws, l_propmat_clearsky_agenda)
  for( Index ip=0; ip<np; ip++ )
    {
      if (failed) continue;

      // Call agenda
      //
      ArrayOfTensor3 dpropmat_clearsky_dx;
      ArrayOfMatrix  dnlte_dx_source, nlte_dx_dsource_dx;
      Tensor4  propmat_clearsky;
      Tensor3  nlte_source;
      //
      try {
        Vector rtp_vmr(0);
        if( nabs )
          {
            propmat_clearsky_agendaExecute( 
            l_ws, propmat_clearsky, nlte_source, dpropmat_clearsky_dx, dnlte_dx_source, nlte_dx_dsource_dx,
               jacobian_quantities, ppath_f(joker,ip), ppath_mag(joker,ip), ppath.los(ip,joker), 
               ppath_p[ip], ppath_t[ip], (ppath_t_nlte.nrows()&&ppath_t_nlte.nrows())?ppath_t_nlte(joker,ip):Vector(0), ppath_vmr(joker,ip),
               l_propmat_clearsky_agenda );
          }
        else
          {
            propmat_clearsky_agendaExecute( 
            l_ws, propmat_clearsky, nlte_source, dpropmat_clearsky_dx, dnlte_dx_source, nlte_dx_dsource_dx,
               jacobian_quantities, ppath_f(joker,ip), ppath_mag(joker,ip), ppath.los(ip,joker), 
               ppath_p[ip], ppath_t[ip], (ppath_t_nlte.nrows()&&ppath_t_nlte.nrows())?ppath_t_nlte(joker,ip):Vector(0), Vector(0), l_propmat_clearsky_agenda );
          }
      } catch (runtime_error e) {
#pragma omp critical (get_ppath_ext_fail)
          { failed = true; fail_msg = e.what();}
      }

      // Copy to output argument
      if( !failed )
        {
          assert( propmat_clearsky.ncols() == stokes_dim );
          assert( propmat_clearsky.nrows() == stokes_dim );
          assert( propmat_clearsky.npages() == nf );
          assert( propmat_clearsky.nbooks() == max(nabs,Index(1)) );
              
          for( Index i1=0; i1<nf; i1++ )
            {
              for( Index i2=0; i2<stokes_dim; i2++ )
                {
                  for( Index i3=0; i3<stokes_dim; i3++ )
                    {
                      ppath_ext(i1,i2,i3,ip) = propmat_clearsky(joker,i1,i2,i3).sum();

                      for( Index ia=0; ia<nisp; ia++ )
                        {
                          abs_per_species(ia,i1,i2,i3,ip) = 
                                                propmat_clearsky(ispecies[ia],i1,i2,i3);
                        }
                        
                      for( Index iq=0; iq<nq; iq++ )
                        {
                          dppath_ext_dx(iq,i1,i2,i3,ip) = dpropmat_clearsky_dx[iq](i1,i2,i3);
                          if(i3==0&&!nlte_source.empty())
                            dppath_nlte_source_dx(iq,i1,i2,ip) = //FIXME: nlte_dx_dsource_dx must be multiplied by frequency terms for wind...
                            dnlte_dx_source[iq](i1,i2) + nlte_dx_dsource_dx[iq](i1,i2);
                        }
                      
                    }
                }
            }

          // Point with LTE
          if( nlte_source.empty() )
            {
              lte[ip] = 1;
              ppath_nlte_source(joker,joker,ip) = 0;
            }
          // Non-LTE point
          else
            {
              assert( nlte_source.ncols() == stokes_dim );
              assert( nlte_source.nrows() == nf );
              assert( nlte_source.npages() == max(nabs,Index(1)) );
              //
              lte[ip] = 0;
              for( Index i1=0; i1<nf; i1++ )
                {
                  for( Index i2=0; i2<stokes_dim; i2++ )
                    { 
                      ppath_nlte_source(i1,i2,ip) = nlte_source(joker,i1,i2).sum();
                    }
                }
            }
        }
    }
    
    if (failed)
      throw runtime_error(fail_msg);
}


//! get_ppath_pmat_and_tmat
/*!
 *   Determines the "clearsky" propagation matrices along a propagation path.
 *   The output arguments are:
 * 
 *ppath_ext* returns the summed extinction (propmat_clearsky) and has
 *      dimensions [ frequency, stokes, stokes, ppath point ].
 * 
 *ppath_abs* returns the summed source relevant absorption
 *      (propmat_source_clearsky) and has dimensions [ frequency, stokes, ppath
 *      point ]. Where lte=1, the data are identical to matching data in
 *ppath_ext*.
 * 
 *lte* flags if LTE is valid or not, at each path point. This variable is
 *      set to 1 if *propmat_clearsky_agenda* returns an empty
 *propmat_source_clearsky*. Otherwise zero.
 * 
 *abs_per_species* can hold absorption for individual species. The species
 *     to include are selected by *ispecies*. For example, to store first and
 *     third species in abs_per_species, set ispecies to [0][2]. The dimensions
 *     are [ absorption species, frequency, stokes, stokes, ppath point ].
 * 
 *   \param   ws                        Out: The workspace
 *   \param   ppath_ext                 Out: Summed extinction at each ppath point
 *   \param   ppath_nlte_source         Out: Summed nlte source at each ppath point
 *   \param   lte                       Out: Index flag for NLTE level
 *   \param   abs_per_species           Out: Absorption for "ispecies"
 *   \param   dppath_ext_dx             Out: Partial derivatives of ppath_ext
 *   \param   dppath_nlte_source_dx     Out: Partial derivatives of ppath_nlte_source
 *   \param   trans_partial             Out: Partial transmission between two ppath points
 *   \param   dtrans_partial_dx_above   Out: Partial derivative of trans_partial for ip+1 ppath point
 *   \param   dtrans_partial_dx_below   Out: Partial derivative of trans_partial for ip ppath point
 *   \param   extmat_case               Out: Index for polarization in ppath_ext
 *   \param   trans_cumlat              Out: Cumulation of trans_partial
 *   \param   scalar_tau                Out: Diagonal elements of ppath_ext times layer length
 *   \param   propmat_clearsky_agenda   In:  As the WSV.    
 *   \param   jacobian_quantities       In:  As the WSV.    
 *   \param   ppd                       In:  propmat friendly version of jacobian_quantities.
 *   \param   ppath                     In:  As the WSV.    
 *   \param   ppath_p                   In:  Pressure for each ppath point.
 *   \param   ppath_t                   In:  Temperature for each ppath point.
 *   \param   ppath_t_nlte              In:  NLTE temperature for each ppath point.
 *   \param   ppath_vmr                 In:  VMR values for each ppath point.
 *   \param   ppath_f                   In:  See get_ppath_f.
 *   \param   ppath_mag                 In:  See get_ppath_atmvars.
 *   \param   ppath_wind                In:  See get_ppath_atmvars.
 *   \param   f_grid                    In:  As the WSV but altered by wind.
 *   \param   jac_species_i             In:  Flags for species Jacobian
 *   \param   jac_is_t                  In:  Flags for temperature Jacobian
 *   \param   jac_wind_i                In:  Flags for wind Jacobian
 *   \param   jac_mag_i                 In:  Flags for magnetic Jacobian
 *   \param   jac_other                 In:  Flags for other propmat Jacobian
 *   \param   rte_alonglos_v            In:  As the WSV.
 *   \param   atmosphere_dim            In:  As the WSV.
 *   \param   stokes_dim                In:  As the WSV.
 *   \param   jacobian_do               In:  As the WSV.
 *   \param   ispecies                  In:  Index of species to store in abs_per_species
 * 
 *   \author Patrick Eriksson 
 *   \date   2012-08-15
 * 
 *   \author Richard Larsson (adding tmat outputs so internal derivatives can be kept here)
 *   \date   2015-12-02
 */
void get_ppath_pmat_and_tmat( 
                            Workspace&      ws,
                            Tensor4&        ppath_ext,
                            Tensor3&        ppath_nlte_source,
                            ArrayOfIndex&   lte,
                            Tensor5&        abs_per_species,
                            Tensor5&        dppath_ext_dx,
                            Tensor4&        dppath_nlte_source_dx,
                            Tensor4&               trans_partial,
                            Tensor5&               dtrans_partial_dx_above,
                            Tensor5&               dtrans_partial_dx_below,
                            ArrayOfArrayOfIndex&   extmat_case,
                            ArrayOfIndex&   clear2cloudbox,
                            Tensor4&               trans_cumulat,
                            Vector&                scalar_tau,
                            Tensor4&               pnd_ext_mat,
                            Matrix&                ppath_pnd,
                            const Agenda&         propmat_clearsky_agenda,
                            const ArrayOfRetrievalQuantity& jacobian_quantities,
                            const PropmatPartialsData&      ppd,
                            const Ppath&          ppath,
                            ConstVectorView       ppath_p, 
                            ConstVectorView       ppath_t, 
                            ConstMatrixView       ppath_t_nlte, 
                            ConstMatrixView       ppath_vmr, 
                            ConstMatrixView       ppath_mag,
                            ConstMatrixView       ppath_wind,
                            ConstMatrixView       ppath_f, 
                            ConstVectorView       f_grid, 
                            const ArrayOfIndex&   jac_species_i,
                            const ArrayOfIndex&   jac_is_t,
                            const ArrayOfIndex&   jac_wind_i,
                            const ArrayOfIndex&   jac_mag_i,
                            const ArrayOfIndex&   jac_to_integrate,
                            const ArrayOfIndex&   jac_other,
                            const ArrayOfIndex&   ispecies,
                            const ArrayOfArrayOfSingleScatteringData scat_data,
                            const Tensor4&        pnd_field,
                            const ArrayOfIndex&   cloudbox_limits,
                            const Index&          use_mean_scat_data,
                            const Numeric&        rte_alonglos_v,
                            const Index&          atmosphere_dim,
                            const Index&          stokes_dim,
                            const bool&           jacobian_do,
                            const bool&           cloudbox_on,
                            const Verbosity&      verbosity)
{   
    // Sizes
    const Index   nf   = f_grid.nelem();
    const Index   np   = ppath.np;
    const Index   nabs = ppath_vmr.nrows();
    const Index   nisp = ispecies.nelem();
    const Index   nq   = jacobian_quantities.nelem();
    
    DEBUG_ONLY(
        for( Index i=0; i<nisp; i++ )
        {
            assert( ispecies[i] >= 0 );
            assert( ispecies[i] < nabs );
        }
    )
    
    // Size variables
    //
    try 
    {
        ppath_ext.resize( nf, stokes_dim, stokes_dim, np ); 
        ppath_nlte_source.resize( nf, stokes_dim, np );   // We start by assuming non-LTE
        lte.resize( np );
        abs_per_species.resize( nisp, nf, stokes_dim, stokes_dim, np ); 
        if(jacobian_do)
        {
            dppath_ext_dx.resize(nq, nf, stokes_dim, stokes_dim, np );
            dppath_nlte_source_dx.resize(nq, nf, stokes_dim, np);
        }
    } 
    catch (std::bad_alloc x) 
    {
        ostringstream os;
        os << "Run-time error in function: get_ppath_pmat_and_tmat" << std::endl
        << "Memory allocation failed for ppath_ext("
        << nabs << ", " << nf << ", " << stokes_dim << ", "
        << stokes_dim << ", " << np << ")" << std::endl;
        throw std::runtime_error(os.str());
    }
    
    String fail_msg;
    bool failed = false;
    
    // For Jacobians
    ArrayOfMatrix AO_dWdx(4), AO_F2(3), dummy_amat;
    ArrayOfTensor3 dummy_at3;
    const Numeric dw = 5, 
                  dt = 0.1,
                  dm = 0.1e-6;
    
    // Loop ppath points
    //
    Workspace l_ws (ws);
    Agenda l_propmat_clearsky_agenda (propmat_clearsky_agenda);
    //
    if (np)
    {
        // Perhaps put all in a function "adapt_dppath_ext_dx"?
        for(Index iq=0;(iq<nq)&&jacobian_do;iq++)
        {
            if( jac_wind_i[iq] == JAC_IS_WIND_U_FROM_PROPMAT ||
                jac_wind_i[iq] == JAC_IS_WIND_V_FROM_PROPMAT ||
                jac_wind_i[iq] == JAC_IS_WIND_W_FROM_PROPMAT ||
                jac_wind_i[iq] == JAC_IS_WIND_ABS_FROM_PROPMAT )
            {
                Index component=-1;
                if(jac_wind_i[iq] == JAC_IS_WIND_ABS_FROM_PROPMAT)
                    component = 0;
                else if(jac_wind_i[iq] == JAC_IS_WIND_U_FROM_PROPMAT)
                    component=1;
                else if(jac_wind_i[iq] == JAC_IS_WIND_V_FROM_PROPMAT)
                    component=2;
                else if(jac_wind_i[iq] == JAC_IS_WIND_W_FROM_PROPMAT)
                    component=3;
                else 
                    throw std::runtime_error("To developer:  You have changed wind jacobians" 
                    " in\nan incompatible manners with some other code.");
                
                get_ppath_f_partials(AO_dWdx[component], component, ppath, f_grid,  atmosphere_dim);
            }
            if(jac_wind_i[iq] == JAC_IS_WIND_U_SEMI_ANALYTIC ||
               jac_wind_i[iq] == JAC_IS_WIND_V_SEMI_ANALYTIC ||
               jac_wind_i[iq] == JAC_IS_WIND_W_SEMI_ANALYTIC )
            {
                Index this_field=-1;
                if(jac_wind_i[iq] == JAC_IS_WIND_U_SEMI_ANALYTIC)
                    this_field=0;
                else if(jac_wind_i[iq] == JAC_IS_WIND_V_SEMI_ANALYTIC)
                    this_field=1;
                else if(jac_wind_i[iq] == JAC_IS_WIND_W_SEMI_ANALYTIC)
                    this_field=2;
                else 
                    throw std::runtime_error("To developer:  You have changed wind jacobians" 
                    "in an incompatible manners with some other code.");
                
                Matrix w2 = ppath_wind;   w2(this_field,joker) += dw;
                get_ppath_f(    AO_F2[this_field], ppath, f_grid,  atmosphere_dim, 
                                rte_alonglos_v, w2 );
            }
        }
        
        #pragma omp parallel for                    \
        if (!arts_omp_in_parallel()               \
        && np >= arts_omp_get_max_threads())  \
        firstprivate(l_ws, l_propmat_clearsky_agenda)
        for( Index ip=0; ip<np; ip++ )
        {
            if (failed) continue;
            
            // Call agenda
            //
            ArrayOfTensor3 dpropmat_clearsky_dx;
            ArrayOfMatrix  dnlte_dx_source, nlte_dx_dsource_dx;
            Tensor4  propmat_clearsky;
            Tensor3  nlte_source;
            //
            try 
            {
                Vector rtp_vmr(0);
                propmat_clearsky_agendaExecute( 
                l_ws, propmat_clearsky, nlte_source, dpropmat_clearsky_dx, dnlte_dx_source, nlte_dx_dsource_dx,
                jacobian_quantities, ppath_f(joker,ip), ppath_mag(joker,ip), ppath.los(ip,joker), 
                ppath_p[ip], ppath_t[ip], 
                (ppath_t_nlte.nrows()&&ppath_t_nlte.nrows())?ppath_t_nlte(joker,ip):Vector(0), 
                nabs?ppath_vmr(joker,ip):Vector(0),
                l_propmat_clearsky_agenda );
            } 
            catch (runtime_error e) 
            {
                #pragma omp critical (get_ppath_ext_fail)
                { failed = true; fail_msg = e.what();}
            }
            
            // Copy to output argument
            if( !failed )
            {
                assert( propmat_clearsky.ncols() == stokes_dim );
                assert( propmat_clearsky.nrows() == stokes_dim );
                assert( propmat_clearsky.npages() == nf );
                assert( propmat_clearsky.nbooks() == max(nabs,Index(1)) );
                
                for( Index i1=0; i1<nf; i1++ )
                {
                    for( Index i2=0; i2<stokes_dim; i2++ )
                    {
                        for( Index i3=0; i3<stokes_dim; i3++ )
                        {
                            ppath_ext(i1,i2,i3,ip) = propmat_clearsky(joker,i1,i2,i3).sum();
                            
                            for( Index ia=0; ia<nisp; ia++ )
                            {
                                abs_per_species(ia,i1,i2,i3,ip) = 
                                propmat_clearsky(ispecies[ia],i1,i2,i3);
                            }
                        }
                    }
                }
                
                // Point with LTE
                if( nlte_source.empty() )
                {
                    lte[ip] = 1;
                    ppath_nlte_source(joker,joker,ip) = 0;
                }
                // Non-LTE point
                else
                {
                    assert( nlte_source.ncols() == stokes_dim );
                    assert( nlte_source.nrows() == nf );
                    assert( nlte_source.npages() == max(nabs,Index(1)) );
                    //
                    lte[ip] = 0;
                    for( Index i1=0; i1<nf; i1++ )
                    {
                        for( Index i2=0; i2<stokes_dim; i2++ )
                        { 
                            ppath_nlte_source(i1,i2,ip) = nlte_source(joker,i1,i2).sum();
                        }
                    }
                }
            
            
                for(Index iq=0;iq<nq&&jacobian_do;iq++)
                {
                    
                    if( jac_is_t[iq] == JAC_IS_T_SEMI_ANALYTIC ) 
                    {
                        Tensor4 propmat_clearsky_dt;
                        Tensor3 nlte_source_dt;
                        const Numeric t2 = ppath_t[ip] + dt;
                        propmat_clearsky_agendaExecute( l_ws, 
                                                        propmat_clearsky_dt, nlte_source_dt, dummy_at3, dummy_amat, dummy_amat,
                                                        ArrayOfRetrievalQuantity(0), 
                                                        ppath_f(joker,ip), ppath_mag(joker,ip), ppath.los(ip,joker), 
                                                        ppath_p[ip], t2, 
                                                        (ppath_t_nlte.nrows()&&ppath_t_nlte.nrows())?ppath_t_nlte(joker,ip):Vector(0), 
                                                        nabs?ppath_vmr(joker,ip):Vector(0),
                                                        l_propmat_clearsky_agenda );
                        //For loops
                        if(!nlte_source.empty())
                            for( Index i1=0; i1<nf; i1++ ) for( Index i2=0; i2<stokes_dim; i2++ ) for( Index i3=0; i3<stokes_dim; i3++ )
                            {
                                dppath_ext_dx(iq,i1,i2,i3,ip) = ( propmat_clearsky_dt(joker,i1,i2,i3).sum() - propmat_clearsky(joker,i1,i2,i3).sum() ) / dt;
                                dppath_nlte_source_dx(iq,i1,i2,ip) = (nlte_source_dt(joker,i1,i2).sum() - nlte_source(joker,i1,i2).sum() ) / dt;
                            }
                        
                    }
                    else if( jac_wind_i[iq] == JAC_IS_WIND_U_SEMI_ANALYTIC ||
                             jac_wind_i[iq] == JAC_IS_WIND_V_SEMI_ANALYTIC ||
                             jac_wind_i[iq] == JAC_IS_WIND_W_SEMI_ANALYTIC ) 
                    {
                        Tensor4 propmat_clearsky_dw;
                        Tensor3 nlte_source_dw;
                        
                        Index this_field=-1;
                        if(jac_wind_i[iq] == JAC_IS_WIND_U_SEMI_ANALYTIC)
                            this_field=0;
                        else if(jac_wind_i[iq] == JAC_IS_WIND_V_SEMI_ANALYTIC)
                            this_field=1;
                        else if(jac_wind_i[iq] == JAC_IS_WIND_W_SEMI_ANALYTIC)
                            this_field=2;
                        else 
                            throw std::runtime_error("To developer:  You have changed wind jacobians" 
                            "in an incompatible manners with some other code.");
                        
                        propmat_clearsky_agendaExecute( l_ws, 
                                                        propmat_clearsky_dw, nlte_source_dw, dummy_at3, dummy_amat, dummy_amat,
                                                        ArrayOfRetrievalQuantity(0), 
                                                        AO_F2[this_field](joker,ip), ppath_mag(joker,ip), ppath.los(ip,joker), 
                                                        ppath_p[ip], ppath_t[ip], 
                                                        (ppath_t_nlte.nrows()&&ppath_t_nlte.nrows())?ppath_t_nlte(joker,ip):Vector(0), 
                                                        nabs?ppath_vmr(joker,ip):Vector(0),
                                                        l_propmat_clearsky_agenda );
                        //For loops
                        for( Index i1=0; i1<nf; i1++ ) for( Index i2=0; i2<stokes_dim; i2++ ) for( Index i3=0; i3<stokes_dim; i3++ )
                        {
                            dppath_ext_dx(iq,i1,i2,i3,ip) = ( propmat_clearsky_dw(joker,i1,i2,i3).sum() - propmat_clearsky(joker,i1,i2,i3).sum() ) / dw;
                            if(!nlte_source.empty())
                                dppath_nlte_source_dx(iq,i1,i2,ip) = (nlte_source_dw(joker,i1,i2).sum() - nlte_source(joker,i1,i2).sum() ) / dw;
                        }
                        
                    }
                    else if( jac_mag_i[iq] == JAC_IS_MAG_U_SEMI_ANALYTIC ||
                             jac_mag_i[iq] == JAC_IS_MAG_V_SEMI_ANALYTIC ||
                             jac_mag_i[iq] == JAC_IS_MAG_W_SEMI_ANALYTIC )
                    {
                        Tensor4 propmat_clearsky_dm;
                        Tensor3 nlte_source_dm;
                        
                        Index this_field=-1;
                        if(jac_mag_i[iq] == JAC_IS_MAG_U_SEMI_ANALYTIC)
                            this_field=0;
                        else if(jac_mag_i[iq] == JAC_IS_MAG_V_SEMI_ANALYTIC)
                            this_field=1;
                        else if(jac_mag_i[iq] == JAC_IS_MAG_W_SEMI_ANALYTIC)
                            this_field=2;
                        else 
                            throw std::runtime_error("To developer:  You have changed magnetic jacobians" 
                            "in an incompatible manners with some other code.");
                        
                        Vector m2 = ppath_mag(joker,ip);   m2[this_field] += dm;
                        
                        propmat_clearsky_agendaExecute( l_ws, 
                                                        propmat_clearsky_dm, nlte_source_dm, dummy_at3, dummy_amat, dummy_amat,
                                                        ArrayOfRetrievalQuantity(0), 
                                                        ppath_f(joker,ip), m2, ppath.los(ip,joker), 
                                                        ppath_p[ip], ppath_t[ip], 
                                                        (ppath_t_nlte.nrows()&&ppath_t_nlte.nrows())?ppath_t_nlte(joker,ip):Vector(0), 
                                                        nabs?ppath_vmr(joker,ip):Vector(0),
                                                        l_propmat_clearsky_agenda );
                        //For loops
                        for( Index i1=0; i1<nf; i1++ ) for( Index i2=0; i2<stokes_dim; i2++ )
                        {
                            if(!nlte_source.empty())
                                dppath_nlte_source_dx(iq,i1,i2,ip) = (nlte_source_dm(joker,i1,i2).sum() - nlte_source(joker,i1,i2).sum() ) / dm;
                            for( Index i3=0; i3<stokes_dim; i3++ )
                                dppath_ext_dx(iq,i1,i2,i3,ip) = ( propmat_clearsky_dm(joker,i1,i2,i3).sum() - propmat_clearsky(joker,i1,i2,i3).sum() ) / dm;
                        }
                    }
                    else if( jac_species_i[iq] > -1 && jacobian_quantities[iq].Analytical() )
                    {   
                        const bool from_propmat = jacobian_quantities[iq].SubSubtag() == PROPMAT_SUBSUBTAG;
                        const Index isp = jac_species_i[iq];
                        
                        // Scaling factors to handle retrieval unit
                        if(!from_propmat)
                        {
                            Numeric unitscf;
                            vmrunitscf( unitscf, 
                                        jacobian_quantities[iq].Mode(), 
                                        ppath_vmr(isp,ip), ppath_p[ip], 
                                        ppath_t[ip] );
                            if(!nlte_source.empty())
                            {
                                throw std::runtime_error("We do not do jacobians for NLTE and species tags.\n");
                            }
                            for( Index i1=0; i1<nf; i1++ ) for( Index i2=0; i2<stokes_dim; i2++ )
                            {
                                for( Index i3=0; i3<stokes_dim; i3++ )
                                    dppath_ext_dx(iq,i1,i2,i3,ip) = propmat_clearsky(jac_species_i[iq],i1,i2,i3)*unitscf;
                            }
                        }
                        else 
                        {
                            Numeric unitscf;
                            dxdvmrscf(  unitscf, 
                                        jacobian_quantities[iq].Mode(), 
                                        ppath_vmr(isp,ip), ppath_p[ip], 
                                        ppath_t[ip] );
                            // the d_var_dx arrays work on ppd grid rather than on jacobian_quantities grid, so first find ppd location for iq
                            const Index this_ppd_iq = ppd.this_jq_index(iq);
                            
                            for( Index i1=0; i1<nf; i1++ ) for( Index i2=0; i2<stokes_dim; i2++ )
                            {
                                if(!nlte_source.empty())
                                {
                                    dppath_nlte_source_dx(iq,i1,i2,ip) = 
                                    (dnlte_dx_source[this_ppd_iq](i1,i2) + 
                                    nlte_dx_dsource_dx[this_ppd_iq](i1,i2))*unitscf;
                                }
                                
                                if(unitscf==1.0)
                                    dppath_ext_dx(iq,joker,joker,joker,ip) =  dpropmat_clearsky_dx[this_ppd_iq];
                                else
                                    for( Index i3=0; i3<stokes_dim; i3++ )
                                        dppath_ext_dx(iq,i1,i2,i3,ip) = dpropmat_clearsky_dx[this_ppd_iq](i1,i2,i3)*unitscf;
                            }
                        }
                    }
                    else if(jac_is_t[iq]!=JAC_IS_NONE||jac_wind_i[iq]!=JAC_IS_NONE||jac_mag_i[iq]!=JAC_IS_NONE||jac_other[iq]!=JAC_IS_NONE)
                    {
                        Index component=-1;
                        if(jac_wind_i[iq] == JAC_IS_WIND_ABS_FROM_PROPMAT)
                            component = 0;
                        else if(jac_wind_i[iq] == JAC_IS_WIND_U_FROM_PROPMAT)
                            component=1;
                        else if(jac_wind_i[iq] == JAC_IS_WIND_V_FROM_PROPMAT)
                            component=2;
                        else if(jac_wind_i[iq] == JAC_IS_WIND_W_FROM_PROPMAT)
                            component=3;
                        
                        // the d_var_dx arrays work on ppd grid rather than on jacobian_quantities grid, so first find ppd location for iq
                        const Index this_ppd_iq = ppd.this_jq_index(iq);
                        
                        for( Index i1=0; i1<nf; i1++ ) for( Index i2=0; i2<stokes_dim; i2++ )
                        {
                            if(!nlte_source.empty())
                            {
                                dppath_nlte_source_dx(iq,i1,i2,ip) = (
                                dnlte_dx_source[this_ppd_iq](i1,i2) + 
                                nlte_dx_dsource_dx[this_ppd_iq](i1,i2) ) *
                                (component==-1?1.0:AO_dWdx[component](i1,ip));
                            }
                            for( Index i3=0; i3<stokes_dim; i3++ )
                                dppath_ext_dx(iq,i1,i2,i3,ip) = dpropmat_clearsky_dx[this_ppd_iq](i1,i2,i3);
                            
                        }
                    }
                    else if(jac_species_i[iq]==-9999)
                    {
                      /* Pass and do nothing, if this is to be integrated, 
                       * then it has to ignore the next else-statement */
                    }
                    else if(jac_to_integrate[iq] == JAC_IS_FLUX)
                    {
                        for( Index i1=0; i1<nf; i1++ ) for( Index i2=0; i2<stokes_dim; i2++ )
                        {
                            if(!nlte_source.empty())
                            {
                                dppath_nlte_source_dx(iq,i1,i2,ip) = 0.0;
                            }
                            for( Index i3=0; i3<stokes_dim; i3++ )
                                dppath_ext_dx(iq,i1,i2,i3,ip) = - ppath_ext(i1,i2,i3,ip);
                        }
                    }
                    else { /* This should only happen to things that have to be perturbed,
                        like pointing errors, or for flux, since distance then matters.  
                        So do nothing here. */ }
                }
            }
        }
        
        if (failed)
            throw std::runtime_error(fail_msg);
    }
    
    //ADAPT dppath_ext_dx
    // Perhaps put all in a function "adapt_dppath_ext_dx"?
    for(Index iq=0;(iq<nq)&&jacobian_do;iq++)
    {
        Index component=-1;
        if(jac_wind_i[iq] == JAC_IS_WIND_ABS_FROM_PROPMAT)
            component = 0;
        else if(jac_wind_i[iq] == JAC_IS_WIND_U_FROM_PROPMAT)
            component=1;
        else if(jac_wind_i[iq] == JAC_IS_WIND_V_FROM_PROPMAT)
            component=2;
        else if(jac_wind_i[iq] == JAC_IS_WIND_W_FROM_PROPMAT)
            component=3;
        
        if(component!=-1)
            for(Index is1=0;is1<stokes_dim;is1++)
                for(Index is2=0;is2<stokes_dim;is2++)
                    dppath_ext_dx(iq,joker,is1,is2,joker)*=AO_dWdx[component];
    }
    
    if(!cloudbox_on)
    {
        if(dppath_ext_dx.empty())
            get_ppath_trans(    trans_partial, extmat_case, trans_cumulat, 
                                scalar_tau, ppath, ppath_ext, f_grid, stokes_dim );
        else
            get_ppath_trans_and_dppath_trans_dx(
                trans_partial, dtrans_partial_dx_above, dtrans_partial_dx_below, 
                extmat_case, trans_cumulat, scalar_tau, ppath, ppath_ext, 
                dppath_ext_dx, jacobian_quantities, f_grid, jac_to_integrate, stokes_dim );
    }
    else
    {
        Array<ArrayOfArrayOfSingleScatteringData> scat_data_single;
        Tensor3 pnd_abs_vec;
        //
        get_ppath_ext( clear2cloudbox, pnd_abs_vec, pnd_ext_mat, scat_data_single,
                       ppath_pnd, ppath, ppath_t, stokes_dim, ppath_f, 
                       atmosphere_dim, cloudbox_limits, pnd_field, 
                       use_mean_scat_data, scat_data, verbosity );
        if(dppath_ext_dx.empty())
            get_ppath_trans2( trans_partial, extmat_case, trans_cumulat, 
                            scalar_tau, ppath, ppath_ext, f_grid, stokes_dim, 
                            clear2cloudbox, pnd_ext_mat );
        else
            get_ppath_trans2_and_dppath_trans_dx(  trans_partial,
                                                   dtrans_partial_dx_above,
                                                   dtrans_partial_dx_below,
                                                   extmat_case,
                                                   trans_cumulat,
                                                   scalar_tau,
                                                   ppath,
                                                   ppath_ext,
                                                   dppath_ext_dx,
                                                   jacobian_quantities,
                                                   f_grid, 
                                                   stokes_dim,
                                                   clear2cloudbox,
                                                   pnd_ext_mat );
            
    }
}


//! get_ppath_blackrad
/*!
    Determines blackbody radiation along the propagation path.

    The output variable is sized inside the function. The dimension order is 
       [ frequency, ppath point ]

    \param   ppath_blackrad    Out: Emission source term at each ppath point 
    \param   ppath             As the WSV
    \param   ppath_t           Temperature for each ppath point.
    \param   ppath_f           See get_ppath_f.

    \author Patrick Eriksson 
    \date   2012-08-15
*/
void get_ppath_blackrad( 
        Matrix&      ppath_blackrad,
  const Ppath&       ppath,
  ConstVectorView    ppath_t, 
  ConstMatrixView    ppath_f )
{
  // Sizes
  const Index   nf = ppath_f.nrows();
  const Index   np = ppath.np;

  // Loop path and call agenda
  //
  ppath_blackrad.resize( nf, np ); 
  //
  for( Index ip=0; ip<np; ip++ )
    { planck( ppath_blackrad(joker,ip), ppath_f(joker,ip), ppath_t[ip] ); }
}


//! get_dppath_blackrad_dt
/*!
 *   Determines partial derivation of the blackbody radiation along the propagation path
 *   with regards to temperature.
 * 
 *   The output variable is sized inside the function. The dimension order is 
 *      [ frequency, ppath point ]
 * 
 *   \param   ws                 Out: The workspace
 *   \param   dppath_blackrad_dt Out: Emission source term at each ppath point 
 *   \param   ppath_t            In:  Temperature for each ppath point.
 *   \param   ppath_f            In:  See get_ppath_f.
 * 
 *   \author Richard Larsson
 *   \date   2015-12-16
 */
void get_dppath_blackrad_dt( 
        Matrix&             dppath_blackrad_dt,
        ConstVectorView     ppath_t, 
        ConstMatrixView     ppath_f,
        const ArrayOfIndex& jac_is_t,
        const bool&         j_analytical_do)
{
    // do anything?
    if( j_analytical_do )
    { 
        // Sizes
        const Index   nf = ppath_f.nrows();
        const Index   np = ppath_f.ncols();
        assert( ppath_t.nelem() == np );
        
        dppath_blackrad_dt.resize(nf, np);
        
        for( Index iq=0; iq<jac_is_t.nelem(); iq++ )
            if( jac_is_t[iq] ) 
                for( Index ip=0; ip<np; ip++ )
                    for( Index iv=0; iv<nf; iv++ )
                        dppath_blackrad_dt(iv,ip) = dplanck_dt( ppath_f(iv,ip), ppath_t[ip] );
    }
}


//! get_ppath_ext
/*!
    Determines the particle optical properties along a propagation path.

    Note that the extinction for all scattering elements is summed. And that
    all frequencies are filled for pnd_abs_vec and pnd_ext_mat even if
    use_mean_scat_data is true (but data equal for all frequencies).

    \param   ws                  Out: The workspace
    \param   clear2cloudbox      Out: Mapping of index. See code for details. 
    \param   pnd_abs_vec         Out: Particle absorption vector
                                      (defined only where particles are found)
    \param   pnd_ext_vec         Out: Particle extinction matrix
                                      (defined only where particles are found)
    \param   scat_data_single    Out: Extracted scattering data. Length of
                                      array affected by *use_mean_scat_data*.
    \param   ppath_pnd           Out. The particle number density for each
                                      path point (also outside cloudbox).
    \param   ppath               As the WSV.    
    \param   ppath_t             Temperature for each ppath point.
    \param   stokes_dim          As the WSV.    
    \param   f_grid              As the WSV.    
    \param   cloubox_limits      As the WSV.    
    \param   pnd_field           As the WSV.    
    \param   use_mean_scat_data  As the WSV.    
    \param   scat_data       As the WSV.    

    \author Patrick Eriksson 
    \date   2012-08-23
*/
void get_ppath_ext( 
        ArrayOfIndex&                  clear2cloudbox,
        Tensor3&                       pnd_abs_vec, 
        Tensor4&                       pnd_ext_mat, 
  Array<ArrayOfArrayOfSingleScatteringData>&  scat_data_single,
        Matrix&                        ppath_pnd,
  const Ppath&                         ppath,
  ConstVectorView                      ppath_t, 
  const Index&                         stokes_dim,
  ConstMatrixView                      ppath_f, 
  const Index&                         atmosphere_dim,
  const ArrayOfIndex&                  cloudbox_limits,
  const Tensor4&                       pnd_field,
  const Index&                         use_mean_scat_data,
  const ArrayOfArrayOfSingleScatteringData&   scat_data,
  const Verbosity&                     verbosity )
{
  const Index nf = ppath_f.nrows();
  const Index np = ppath.np;

  // Pnd along the ppath
  ppath_pnd.resize( pnd_field.nbooks(), np );
  ppath_pnd = 0;

  // A variable that maps from total ppath to extension data index.
  // If outside cloudbox or all pnd=0, this variable holds -1.
  // Otherwise it gives the index in pnd_ext_mat etc.
  clear2cloudbox.resize( np );

  // Determine ppath_pnd
  Index nin = 0;
  for( Index ip=0; ip<np; ip++ )
    {
      Matrix itw( 1, Index(pow(2.0,Numeric(atmosphere_dim))) );

      ArrayOfGridPos gpc_p(1), gpc_lat(1), gpc_lon(1);
      GridPos gp_lat, gp_lon;
      if( atmosphere_dim >= 2 ) { gridpos_copy( gp_lat, ppath.gp_lat[ip] ); } 
      if( atmosphere_dim == 3 ) { gridpos_copy( gp_lon, ppath.gp_lon[ip] ); }
      if( is_gp_inside_cloudbox( ppath.gp_p[ip], gp_lat, gp_lon, 
                                 cloudbox_limits, true, atmosphere_dim ) )
        { 
          interp_cloudfield_gp2itw( itw(0,joker), 
                                    gpc_p[0], gpc_lat[0], gpc_lon[0], 
                                    ppath.gp_p[ip], gp_lat, gp_lon,
                                    atmosphere_dim, cloudbox_limits );
          for( Index i=0; i<pnd_field.nbooks(); i++ )
            {
              interp_atmfield_by_itw( ppath_pnd(i,ip), atmosphere_dim,
                                      pnd_field(i,joker,joker,joker), 
                                      gpc_p, gpc_lat, gpc_lon, itw );
            }
          if( max(ppath_pnd(joker,ip)) > 0 )
            { clear2cloudbox[ip] = nin;   nin++; }
          else
            { clear2cloudbox[ip] = -1; }
        }
      else
        { clear2cloudbox[ip] = -1; }
    }

  // Particle single scattering properties (are independent of position)
  //
  if( use_mean_scat_data )
    {
      const Numeric f = (mean(ppath_f(0,joker))+mean(ppath_f(nf-1,joker)))/2.0;
      scat_data_single.resize( 1 );
      scat_data_monoCalc( scat_data_single[0], scat_data, Vector(1,f), 0,
                          verbosity );
    }
  else
    {
      scat_data_single.resize( nf );
      for( Index iv=0; iv<nf; iv++ )
        { 
          const Numeric f = mean(ppath_f(iv,joker));
          scat_data_monoCalc( scat_data_single[iv], scat_data, Vector(1,f), 0, 
                              verbosity ); 
        }
    }

  // Resize absorption and extension tensors
  pnd_abs_vec.resize( nf, stokes_dim, nin ); 
  pnd_ext_mat.resize( nf, stokes_dim, stokes_dim, nin ); 

  // Loop ppath points
  //
  for( Index ip=0; ip<np; ip++ )
    {
      const Index i = clear2cloudbox[ip];
      if( i>=0 )
        {
          // Direction of outgoing scattered radiation (which is reversed to
          // LOS). Note that rtp_los2 is only used for extracting scattering
          // properties.
          Vector rtp_los2;
          mirror_los( rtp_los2, ppath.los(ip,joker), atmosphere_dim );

          // Extinction and absorption
          if( use_mean_scat_data )
            {
              Vector   abs_vec( stokes_dim );
              Matrix   ext_mat( stokes_dim, stokes_dim );
              opt_propCalc( ext_mat, abs_vec, rtp_los2[0], rtp_los2[1], 
                            scat_data_single[0], stokes_dim, ppath_pnd(joker,ip), 
                            ppath_t[ip], verbosity);
              for( Index iv=0; iv<nf; iv++ )
                { 
                  pnd_ext_mat(iv,joker,joker,i) = ext_mat;
                  pnd_abs_vec(iv,joker,i)       = abs_vec;
                }
            }
          else
            {
              for( Index iv=0; iv<nf; iv++ )
                { 
                  opt_propCalc( pnd_ext_mat(iv,joker,joker,i), 
                                pnd_abs_vec(iv,joker,i), rtp_los2[0], 
                                rtp_los2[1], scat_data_single[iv], stokes_dim,
                                ppath_pnd(joker,ip), ppath_t[ip], verbosity );
                }
            }
        }
    }
}



//! get_ppath_f
/*!
    Determines the Doppler shifted frequencies along the propagation path.

    ppath_doppler [ nf + np ]

    \param   ppath_f          Out: Doppler shifted f_grid
    \param   ppath            Propagation path.
    \param   f_grid           Original f_grid.
    \param   atmosphere_dim   As the WSV.
    \param   rte_alonglos_v   As the WSV.
    \param   ppath_wind       See get_ppath_atmvars.

    \author Patrick Eriksson 
    \date   2013-02-21
*/
void get_ppath_f( 
        Matrix&    ppath_f,
  const Ppath&     ppath,
  ConstVectorView  f_grid, 
  const Index&     atmosphere_dim,
  const Numeric&   rte_alonglos_v,
  ConstMatrixView  ppath_wind )
{
  // Sizes
  const Index   nf = f_grid.nelem();
  const Index   np = ppath.np;

  ppath_f.resize(nf,np);

  // Doppler relevant velocity
  //
  for( Index ip=0; ip<np; ip++ )
    {
      // Start by adding rte_alonglos_v (most likely sensor effects)
      Numeric v_doppler = rte_alonglos_v;

      // Include wind
      if( ppath_wind(1,ip) != 0  ||  ppath_wind(0,ip) != 0  ||  
                                     ppath_wind(2,ip) != 0  )
        {
          // The dot product below is valid for the photon direction. Winds
          // along this direction gives a positive contribution.
          v_doppler += dotprod_with_los( ppath.los(ip,joker), ppath_wind(0,ip),
                          ppath_wind(1,ip), ppath_wind(2,ip), atmosphere_dim );
        }

      // Determine frequency grid
      if( v_doppler == 0 )
        { ppath_f(joker,ip) = f_grid; }
      else
        { 
          // Positive v_doppler means that sensor measures lower rest
          // frequencies
          const Numeric a = 1 - v_doppler / SPEED_OF_LIGHT;
          for( Index iv=0; iv<nf; iv++ )
            { ppath_f(iv,ip) = a * f_grid[iv]; }
        }
    }
}


//! get_ppath_f_partials
/*!
 *   Determines the derivative of the Doppler shifted frequencies along 
 *   the propagation path for wind.
 * 
 *   ppath_doppler [ nf + np ]
 * 
 *   \param   ppath_f_partials  Out: Partial derivative of Doppler shifted f_grid with respect to...
 *   \param   component         In:  Component of the shift for partial derivation
 *   \param   ppath             In:  Propagation ppath.
 *   \param   f_grid            In:  As the WSV
 *   \param   atmosphere_dim    In:  As the WSV.
 *   \param   ppath_wind        In:  See get_ppath_atmvars.
 * 
 *   \author Richard Larsson
 *   \date   2015-12-10
 */
void get_ppath_f_partials( 
Matrix&    ppath_f_partials,
const Index& component,
const Ppath&     ppath,
ConstVectorView  f_grid, 
const Index&     atmosphere_dim)
{
    // component 0 means total speed
    // component 1 means u speed
    // component 2 means v speed
    // component 3 means w speed
    
    // Sizes
    const Index   nf = f_grid.nelem();
    const Index   np = ppath.np;
    
    ppath_f_partials.resize(nf,np);
    
    // Doppler relevant velocity
    //
    for( Index ip=0; ip<np; ip++ )
    {
        // initialize
        Numeric dv_doppler_dx=0.0;
        
        switch( component )
        {
            case 0:// this is total and is already initialized to avoid compiler warnings
                dv_doppler_dx = 1.0;
                break;
            case 1:// this is the u-component
                dv_doppler_dx = (dotprod_with_los(ppath.los(ip,joker), 1, 0, 0, atmosphere_dim));
                break;
            case 2:// this is v-component
                dv_doppler_dx = (dotprod_with_los(ppath.los(ip,joker), 0, 1, 0, atmosphere_dim));
                break;
            case 3:// this is w-component
                dv_doppler_dx = (dotprod_with_los(ppath.los(ip,joker), 0, 0, 1, atmosphere_dim));
                break;
            default:
                throw std::runtime_error("This being seen means that there is a development bug in interactions with get_ppath_df_dW.\n");
                break;
        }
        
        // Determine frequency grid
        if( dv_doppler_dx == 0.0 )
        { ppath_f_partials(joker,ip) = 0.0; }
        else
        { 
            const Numeric a = - dv_doppler_dx / SPEED_OF_LIGHT;
            for( Index iv=0; iv<nf; iv++ )
            { ppath_f_partials(iv,ip) = a * f_grid[iv]; }
        }
    }
}


//! get_ppath_trans
/*!
    Determines the transmission in different ways for a clear-sky RT
    integration.

    The argument trans_partial holds the transmission for each propagation path
    step. It has np-1 columns.

    The structure of average extinction matrix for each step is returned in
    extmat_case. The dimension of this variable is [np-1,nf]. For the coding
    see *ext2trans*.

    The argument trans_cumalat holds the transmission between the path point
    with index 0 and each propagation path point. The transmission is valid for
    the photon travelling direction. It has np columns.

    The output variables are sized inside the function. The dimension order is 
       [ frequency, stokes, stokes, ppath point ]

    The scalar optical thickness is calculated in parallel.

    \param   trans_partial  Out: Transmission for each path step.
    \param   extmat_case    Out: Corresponds to *icase* of *ext2trans*.
    \param   trans_cumulat  Out: Transmission to each path point.
    \param   scalar_tau     Out: Total (scalar) optical thickness of path
    \param   ppath          As the WSV.    
    \param   ppath_ext      See get_ppath_ext.
    \param   f_grid         As the WSV.    
    \param   stokes_dim     As the WSV.

    \author Patrick Eriksson 
    \date   2012-08-15
*/
void get_ppath_trans( 
        Tensor4&               trans_partial,
        ArrayOfArrayOfIndex&   extmat_case,
        Tensor4&               trans_cumulat,
        Vector&                scalar_tau,
  const Ppath&                 ppath,
  ConstTensor4View&            ppath_ext,
  ConstVectorView              f_grid, 
  const Index&                 stokes_dim )
{
  // Sizes
  const Index   nf = f_grid.nelem();
  const Index   np = ppath.np;

  // Init variables
  //
  trans_partial.resize( nf, stokes_dim, stokes_dim, np-1 );
  trans_cumulat.resize( nf, stokes_dim, stokes_dim, np );
  //
  extmat_case.resize(np-1);
  for( Index i=0; i<np-1; i++ )
    { extmat_case[i].resize(nf); }
  //
  scalar_tau.resize( nf );
  scalar_tau = 0;

  // Loop ppath points (in the anti-direction of photons)  
  //
  for( Index ip=0; ip<np; ip++ )
    {
      // If first point, calculate sum of absorption and set transmission
      // to identity matrix.
      if( ip == 0 )
        { 
          for( Index iv=0; iv<nf; iv++ )
            { id_mat( trans_cumulat(iv,joker,joker,ip) ); }
        }

      else
        {
          for( Index iv=0; iv<nf; iv++ )
            {
              // Transmission due to absorption
              Matrix ext_mat(stokes_dim,stokes_dim);
              for( Index is1=0; is1<stokes_dim; is1++ ) {
                for( Index is2=0; is2<stokes_dim; is2++ ) {
                  ext_mat(is1,is2) = 0.5 * ( ppath_ext(iv,is1,is2,ip-1) + 
                                             ppath_ext(iv,is1,is2,ip  ) );
                } }
              scalar_tau[iv] += ppath.lstep[ip-1] * ext_mat(0,0); 
              extmat_case[ip-1][iv] = 0;
              ext2trans( trans_partial(iv,joker,joker,ip-1), 
                         extmat_case[ip-1][iv], ext_mat, ppath.lstep[ip-1] );

              // Cumulative transmission
              // (note that multiplication below depends on ppath loop order)
              mult( trans_cumulat(iv,joker,joker,ip), 
                    trans_cumulat(iv,joker,joker,ip-1), 
                    trans_partial(iv,joker,joker,ip-1) );
            }
        }
    }
}


//! get_ppath_trans_and_dppath_trans_dx
/*!
 *   Determines the transmission in different ways for a clear-sky RT
 *   integration.
 * 
 *   The argument trans_partial holds the transmission for each propagation path
 *   step. It has np-1 columns.
 * 
 *   The structure of average extinction matrix for each step is returned in
 *   extmat_case. The dimension of this variable is [np-1,nf]. For the coding
 *   see *ext2trans*.
 * 
 *   The argument trans_cumalat holds the transmission between the path point
 *   with index 0 and each propagation path point. The transmission is valid for
 *   the photon travelling direction. It has np columns.
 * 
 *   The output variables are sized inside the function. The dimension order is 
 *      [ frequency, stokes, stokes, ppath point ]
 * 
 *   The scalar optical thickness is calculated in parallel.
 * 
 *   \param   trans_partial                     Out: Transmission for each path step.
 *   \param   dtrans_partial_dx_from_above      Out: Partial transmission for upper p-level.
 *   \param   dtrans_partial_dx_from_below      Out: Partial transmission for lower p-level.
 *   \param   extmat_case                       Out: Corresponds to *icase* of *ext2trans*.
 *   \param   trans_cumulat                     Out: Transmission to each path point.
 *   \param   scalar_tau                        Out: Total (scalar) optical thickness of path
 *   \param   ppath                             In:  As the WSV.    
 *   \param   ppath_ext                         In:  See get_ppath_ext.
 *   \param   dppath_ext_dx                     In:  See get_ppath_ext_and_dppath_ext_dx.
 *   \param   jacobian_quantities               In:  As the WSV.    
 *   \param   f_grid                            In:  As the WSV.    
 *   \param   for_distance_integration          In:  Indicates integration along r.
 *   \param   stokes_dim                        In:  As the WSV.
 * 
 *   \author Patrick Eriksson 
 *   \date   2012-08-15
 * 
 *   Added support for partial derivatives (copy-change of original function)
 *   \author Richard Larsson
 *   \date   2015-10-06
 */
void get_ppath_trans_and_dppath_trans_dx( 
        Tensor4&               trans_partial,
        Tensor5&               dtrans_partial_dx_from_above,
        Tensor5&               dtrans_partial_dx_from_below,
        ArrayOfArrayOfIndex&   extmat_case,
        Tensor4&               trans_cumulat,
        Vector&                scalar_tau,
  const Ppath&                 ppath,
  ConstTensor4View&            ppath_ext,
  ConstTensor5View&            dppath_ext_dx,
  const ArrayOfRetrievalQuantity& jacobian_quantities,
  ConstVectorView              f_grid, 
  const ArrayOfIndex&          for_distance_integration,
  const Index&                 stokes_dim )
{
    
    // Sizes
    const Index   nf = f_grid.nelem();
    const Index   np = ppath.np;
    const Index   nq = jacobian_quantities.nelem();
    
    // Init variables
    //
    trans_partial.resize( nf, stokes_dim, stokes_dim, np-1 );
    trans_cumulat.resize( nf, stokes_dim, stokes_dim, np );
    
    dtrans_partial_dx_from_above.resize( nq, nf, stokes_dim, stokes_dim, np-1 );
    dtrans_partial_dx_from_below.resize( nq, nf, stokes_dim, stokes_dim, np-1 );
    //
    extmat_case.resize(np-1);
    for( Index i=0; i<np-1; i++ )
    { extmat_case[i].resize(nf); }
    //
    scalar_tau.resize( nf );
    scalar_tau = 0;
    
    // Loop ppath points (in the anti-direction of photons)  
    //
    for( Index ip=0; ip<np; ip++ )
    {
        // If first point, calculate sum of absorption and set transmission
        // to identity matrix.
        if( ip == 0 )
        { 
            for( Index iv=0; iv<nf; iv++ )
            { id_mat( trans_cumulat(iv,joker,joker,ip) ); }
        }
        
        else
        {
            for( Index iv=0; iv<nf; iv++ )
            {
                // Transmission due to absorption
                Matrix ext_mat(stokes_dim,stokes_dim);
                Tensor3 dext_mat_dx_from_above(nq,stokes_dim,stokes_dim),
                        dext_mat_dx_from_below(nq,stokes_dim,stokes_dim);
                for( Index is1=0; is1<stokes_dim; is1++ ) 
                {
                    for( Index is2=0; is2<stokes_dim; is2++ ) 
                    {
                        ext_mat(is1,is2) = 0.5 * ( ppath_ext(iv,is1,is2,ip-1) + 
                                                   ppath_ext(iv,is1,is2,ip  ) );
                        for( Index iq=0; iq<nq; iq++ )
                        {
                          if(for_distance_integration[iq] == JAC_IS_FLUX)
                          {
                            dext_mat_dx_from_above(iq,is1,is2) =  
                            0.5 / ppath.lstep[ip-1] * dppath_ext_dx(iq,iv,is1,is2,ip);
                            dext_mat_dx_from_below(iq,is1,is2) =  
                            0.5 / ppath.lstep[ip-1] * dppath_ext_dx(iq,iv,is1,is2,ip-1);
                          }
                          else 
                          {
                            // Upper and lower level influence on the dependency at layer
                            dext_mat_dx_from_above(iq,is1,is2) = 
                            0.5 * dppath_ext_dx(iq,iv,is1,is2,ip);
                            dext_mat_dx_from_below(iq,is1,is2) = 
                            0.5 * dppath_ext_dx(iq,iv,is1,is2,ip-1);
                          }
                        } 
                    } 
                    
                }
                scalar_tau[iv] += ppath.lstep[ip-1] * ext_mat(0,0); 
                extmat_case[ip-1][iv] = 0;
                ext2trans_and_ext2dtrans_dx( 
                            trans_partial(iv,joker,joker,ip-1), 
                            dtrans_partial_dx_from_above(joker, iv,joker,joker,ip-1), 
                            dtrans_partial_dx_from_below(joker, iv,joker,joker,ip-1), 
                            extmat_case[ip-1][iv], ext_mat, 
                            dext_mat_dx_from_above, dext_mat_dx_from_below,
                            ppath.lstep[ip-1] );
                
                // Cumulative transmission
                // (note that multiplication below depends on ppath loop order)
                mult( trans_cumulat(iv,joker,joker,ip), 
                        trans_cumulat(iv,joker,joker,ip-1), 
                        trans_partial(iv,joker,joker,ip-1) );
            }
        }
    }
}



//! get_ppath_trans2
/*!
    Determines the transmission in different ways for a cloudy RT integration.

    This function works as get_ppath_trans, but considers also particle
    extinction. See get_ppath_trans for format of output data.

    \param   trans_partial    Out: Transmission for each path step.
    \param   trans_cumulat    Out: Transmission to each path point.
    \param   scalar_tau       Out: Total (scalar) optical thickness of path
    \param   ppath            As the WSV.    
    \param   ppath_ext        See get_ppath_ext.
    \param   f_grid           As the WSV.    
    \param   stokes_dim       As the WSV.
    \param   clear2cloudbox   See get_ppath_ext.
    \param   pnd_ext_mat      See get_ppath_ext.

    \author Patrick Eriksson 
    \date   2012-08-23
*/
void get_ppath_trans2( 
        Tensor4&               trans_partial,
        ArrayOfArrayOfIndex&   extmat_case,
        Tensor4&               trans_cumulat,
        Vector&                scalar_tau,
  const Ppath&                 ppath,
  ConstTensor4View&            ppath_ext,
  ConstVectorView              f_grid, 
  const Index&                 stokes_dim,
  const ArrayOfIndex&          clear2cloudbox,
  ConstTensor4View             pnd_ext_mat )
{
  // Sizes
  const Index   nf = f_grid.nelem();
  const Index   np = ppath.np;

  // Init variables
  //
  trans_partial.resize( nf, stokes_dim, stokes_dim, np-1 );
  trans_cumulat.resize( nf, stokes_dim, stokes_dim, np );
  //
  extmat_case.resize(np-1);
  for( Index i=0; i<np-1; i++ )
    { extmat_case[i].resize(nf); }
  //
  scalar_tau.resize( nf );
  scalar_tau  = 0;
  
  // Loop ppath points (in the anti-direction of photons)  
  //
  Tensor3 extsum_old, extsum_this( nf, stokes_dim, stokes_dim );
  //
  for( Index ip=0; ip<np; ip++ )
    {
      // If first point, calculate sum of absorption and set transmission
      // to identity matrix.
      if( ip == 0 )
        { 
          for( Index iv=0; iv<nf; iv++ ) 
            {
              for( Index is1=0; is1<stokes_dim; is1++ ) {
                for( Index is2=0; is2<stokes_dim; is2++ ) {
                  extsum_this(iv,is1,is2) = ppath_ext(iv,is1,is2,ip);
                } } 
              id_mat( trans_cumulat(iv,joker,joker,ip) );
            }
          // First point should not be "cloudy", but just in case:
          if( clear2cloudbox[ip] >= 0 )
            {
              const Index ic = clear2cloudbox[ip];
              for( Index iv=0; iv<nf; iv++ ) {
                for( Index is1=0; is1<stokes_dim; is1++ ) {
                  for( Index is2=0; is2<stokes_dim; is2++ ) {
                    extsum_this(iv,is1,is2) += pnd_ext_mat(iv,is1,is2,ic);
                } } }
            }
        }

      else
        {
          const Index ic = clear2cloudbox[ip];
          //
          for( Index iv=0; iv<nf; iv++ ) 
            {
              // Transmission due to absorption and scattering
              Matrix ext_mat(stokes_dim,stokes_dim);  // -1*tau
              for( Index is1=0; is1<stokes_dim; is1++ ) {
                for( Index is2=0; is2<stokes_dim; is2++ ) {
                  extsum_this(iv,is1,is2) = ppath_ext(iv,is1,is2,ip);
                  if( ic >= 0 )
                    { extsum_this(iv,is1,is2) += pnd_ext_mat(iv,is1,is2,ic); }

                  ext_mat(is1,is2) = 0.5 * ( extsum_old(iv,is1,is2) + 
                                             extsum_this(iv,is1,is2) );
                } }
              scalar_tau[iv] += ppath.lstep[ip-1] * ext_mat(0,0); 
              extmat_case[ip-1][iv] = 0;
              ext2trans( trans_partial(iv,joker,joker,ip-1), 
                         extmat_case[ip-1][iv], ext_mat, ppath.lstep[ip-1] );

              // Note that multiplication below depends on ppath loop order
              mult( trans_cumulat(iv,joker,joker,ip), 
                    trans_cumulat(iv,joker,joker,ip-1), 
                    trans_partial(iv,joker,joker,ip-1) );
            }
        }

      extsum_old = extsum_this;
    }
}


//! get_ppath_trans2
/*!
 *   Determines the transmission in different ways for a cloudy RT integration.
 * 
 *   This function works as get_ppath_trans, but considers also particle
 *   extinction. See get_ppath_trans for format of output data.
 * 
 *   \param   trans_partial    Out: Transmission for each path step.
 *   \param   dtrans_partial_dx_from_above      Out: Partial transmission for upper p-level.
 *   \param   dtrans_partial_dx_from_below      Out: Partial transmission for lower p-level.
 *   \param   trans_cumulat    Out: Transmission to each path point.
 *   \param   scalar_tau       Out: Total (scalar) optical thickness of path
 *   \param   ppath            As the WSV.    
 *   \param   ppath_ext        See get_ppath_ext.
 *   \param   dppath_ext_dx                     In:  See get_ppath_ext_and_dppath_ext_dx.
 *   \param   jacobian_quantities               In:  As the WSV.    
 *   \param   f_grid           As the WSV.    
 *   \param   stokes_dim       As the WSV.
 *   \param   clear2cloudbox   See get_ppath_ext.
 *   \param   pnd_ext_mat      See get_ppath_ext.
 * 
 *   \author Patrick Eriksson 
 *   \date   2012-08-23
 * 
 *   \author Richard Larsson (adapted for partial derivation)
 *   \date   2015-12-18
 */
void get_ppath_trans2_and_dppath_trans_dx(  Tensor4&               trans_partial,
                                            Tensor5&               dtrans_partial_dx_from_above,
                                            Tensor5&               dtrans_partial_dx_from_below,
                                            ArrayOfArrayOfIndex&   extmat_case,
                                            Tensor4&               trans_cumulat,
                                            Vector&                scalar_tau,
                                            const Ppath&                 ppath,
                                            ConstTensor4View&            ppath_ext,
                                            ConstTensor5View&            dppath_ext_dx,
                                            const ArrayOfRetrievalQuantity& jacobian_quantities,
                                            ConstVectorView              f_grid, 
                                            const Index&                 stokes_dim,
                                            const ArrayOfIndex&          clear2cloudbox,
                                            ConstTensor4View             pnd_ext_mat )
{
    // Sizes
    const Index   nf = f_grid.nelem();
    const Index   np = ppath.np;
    const Index   nq = jacobian_quantities.nelem();
    
    // Init variables
    //
    trans_partial.resize( nf, stokes_dim, stokes_dim, np-1 );
    trans_cumulat.resize( nf, stokes_dim, stokes_dim, np );
    
    dtrans_partial_dx_from_above.resize( nq, nf, stokes_dim, stokes_dim, np-1 );
    dtrans_partial_dx_from_below.resize( nq, nf, stokes_dim, stokes_dim, np-1 );
    //
    extmat_case.resize(np-1);
    for( Index i=0; i<np-1; i++ )
    { extmat_case[i].resize(nf); }
    //
    scalar_tau.resize( nf );
    scalar_tau  = 0;
    
    // Loop ppath points (in the anti-direction of photons)  
    //
    Tensor3 extsum_old, 
            extsum_this( nf, stokes_dim, stokes_dim), 
            dext_mat_dx_from_above(nq,stokes_dim,stokes_dim),
            dext_mat_dx_from_below(nq,stokes_dim,stokes_dim);
    //
    for( Index ip=0; ip<np; ip++ )
    {
        // If first point, calculate sum of absorption and set transmission
        // to identity matrix.
        if( ip == 0 )
        { 
            for( Index iv=0; iv<nf; iv++ ) 
            {
                for( Index is1=0; is1<stokes_dim; is1++ ) {
                    for( Index is2=0; is2<stokes_dim; is2++ ) {
                        extsum_this(iv,is1,is2) = ppath_ext(iv,is1,is2,ip);
                    } } 
                    id_mat( trans_cumulat(iv,joker,joker,ip) );
            }
            // First point should not be "cloudy", but just in case:
            if( clear2cloudbox[ip] >= 0 )
            {
                const Index ic = clear2cloudbox[ip];
                for( Index iv=0; iv<nf; iv++ ) {
                    for( Index is1=0; is1<stokes_dim; is1++ ) {
                        for( Index is2=0; is2<stokes_dim; is2++ ) {
                            extsum_this(iv,is1,is2) += pnd_ext_mat(iv,is1,is2,ic);
                        } } }
            }
        }
        
        else
        {
            const Index ic = clear2cloudbox[ip];
            //
            for( Index iv=0; iv<nf; iv++ ) 
            {
                // Transmission due to absorption and scattering
                Matrix ext_mat(stokes_dim,stokes_dim);  // -1*tau
                for( Index is1=0; is1<stokes_dim; is1++ )
                {
                    for( Index is2=0; is2<stokes_dim; is2++ ) 
                    {
                        extsum_this(iv,is1,is2) = ppath_ext(iv,is1,is2,ip);
                        if( ic >= 0 )
                            extsum_this(iv,is1,is2) += pnd_ext_mat(iv,is1,is2,ic);
                        
                        ext_mat(is1,is2) = 0.5 * ( extsum_old(iv,is1,is2) + 
                        extsum_this(iv,is1,is2) );
                        for( Index iq=0; iq<nq; iq++ )
                        {
                            // Upper and lower level influence on the dependency at layer
                            dext_mat_dx_from_above(iq,is1,is2) = 
                            0.5 * dppath_ext_dx(iq,iv,is1,is2,ip);
                            dext_mat_dx_from_below(iq,is1,is2) = 
                            0.5 * dppath_ext_dx(iq,iv,is1,is2,ip-1);
                        }
                    } 
                }
                    scalar_tau[iv] += ppath.lstep[ip-1] * ext_mat(0,0); 
                    extmat_case[ip-1][iv] = 0;
                    ext2trans_and_ext2dtrans_dx( trans_partial(iv,joker,joker,ip-1), 
                                                 dtrans_partial_dx_from_above(joker, iv,joker,joker,ip-1), 
                                                 dtrans_partial_dx_from_below(joker, iv,joker,joker,ip-1), 
                                                 extmat_case[ip-1][iv], ext_mat, 
                                                 dext_mat_dx_from_above, dext_mat_dx_from_below,
                                                 ppath.lstep[ip-1] );
                    
                    // Note that multiplication below depends on ppath loop order
                    mult( trans_cumulat(iv,joker,joker,ip), 
                          trans_cumulat(iv,joker,joker,ip-1), 
                          trans_partial(iv,joker,joker,ip-1) );
            }
        }
        
        extsum_old = extsum_this;
    }
}


//! get_rowindex_for_mblock
/*!
    Returns the "range" of *y* corresponding to a measurement block

    \return  The range.
    \param   sensor_response    As the WSV.
    \param   mblock_index            Index of the measurement block.

    \author Patrick Eriksson 
    \date   2009-10-16
*/
Range get_rowindex_for_mblock( 
  const Sparse&   sensor_response, 
  const Index&    mblock_index )
{
  const Index   n1y = sensor_response.nrows();
  return Range( n1y*mblock_index, n1y );
}


void iyb_calc_body(
        bool&                       failed,
        String&                     fail_msg,
        ArrayOfArrayOfTensor4&      iy_aux_array,
        Workspace&                  ws,
        Ppath&                      ppath,
        Vector&                     iyb,
        ArrayOfMatrix&              diyb_dx,
  const Index&                      mblock_index,
  const Index&                      atmosphere_dim,
  ConstTensor3View                  t_field,
  ConstTensor3View                  z_field,
  ConstTensor4View                  vmr_field,
  const Index&                      cloudbox_on,
  const Index&                      stokes_dim,
  ConstVectorView                   f_grid,
  ConstMatrixView                   sensor_pos,
  ConstMatrixView                   sensor_los,
  ConstMatrixView                   transmitter_pos,
  ConstMatrixView                   mblock_dlos_grid,
  const String&                     iy_unit,  
  const Agenda&                     iy_main_agenda,
  const Index&                      j_analytical_do,
  const ArrayOfRetrievalQuantity&   jacobian_quantities,
  const ArrayOfArrayOfIndex&        jacobian_indices,
  const ArrayOfString&              iy_aux_vars,
  const Index&                      ilos,
  const Index&                      nf )
{
    // The try block here is necessary to correctly handle
    // exceptions inside the parallel region.
    try
    {
      //--- LOS of interest
      //
      Vector los( sensor_los.ncols() );
      //
      los     = sensor_los( mblock_index, joker );
      los[0] += mblock_dlos_grid(ilos,0);
      if( mblock_dlos_grid.ncols() > 1 )
        { map_daa( los[0], los[1], los[0], los[1],
                   mblock_dlos_grid(ilos,1) ); }
      else
        { adjust_los( los, atmosphere_dim ); }
      //--- rtp_pos 1 and 2
      //
      Vector rtp_pos, rtp_pos2(0);
      //
      rtp_pos = sensor_pos( mblock_index, joker );
      if( transmitter_pos.nrows() )
        { rtp_pos2 = transmitter_pos( mblock_index, joker ); }

      // Calculate iy and associated variables
      //
      Matrix         iy;
      ArrayOfTensor3 diy_dx;
      Tensor3        iy_transmission(0,0,0);
      const Index    iy_agenda_call1 = 1;
      const Index    iy_id = (Index)1e6*(mblock_index+1) + (Index)1e3*(ilos+1);
      //
      iy_main_agendaExecute(ws, iy, iy_aux_array[ilos], ppath, diy_dx, 
                            iy_agenda_call1, iy_unit, iy_transmission, iy_aux_vars,
                            iy_id, cloudbox_on, j_analytical_do, t_field, z_field,
                            vmr_field, f_grid, rtp_pos, los,
                            rtp_pos2, iy_main_agenda );

      // Check that aux data can be handled and has correct size
      for( Index q=0; q<iy_aux_array[ilos].nelem(); q++ )
        {
          if( iy_aux_array[ilos][q].ncols() != 1  ||
              iy_aux_array[ilos][q].nrows() != 1 )
            {
              throw runtime_error( "For calculations using yCalc, "
                                   "*iy_aux_vars* can not include\nvariables of "
                                   "along-the-path or extinction matrix type.");
            }
          assert( iy_aux_array[ilos][q].npages() == 1  ||
                  iy_aux_array[ilos][q].npages() == stokes_dim );
          assert( iy_aux_array[ilos][q].nbooks() == 1  ||
                  iy_aux_array[ilos][q].nbooks() == nf  );
        }

      // Start row in iyb etc. for present LOS
      //
      const Index row0 = ilos * nf * stokes_dim;

      // Jacobian part
      //
      if( j_analytical_do )
        {
          FOR_ANALYTICAL_JACOBIANS_DO(
                                      for( Index ip=0; ip<jacobian_indices[iq][1] -
                                             jacobian_indices[iq][0]+1; ip++ )
                                        {
                                          for( Index is=0; is<stokes_dim; is++ )
                                            {
                                              diyb_dx[iq](Range(row0+is,nf,stokes_dim),ip)=
                                                diy_dx[iq](ip,joker,is);
                                            }
                                        }
                                      )
        }

      // iy : copy to iyb
      for( Index is=0; is<stokes_dim; is++ )
        { iyb[Range(row0+is,nf,stokes_dim)] = iy(joker,is); }

    }  // End try

    catch (runtime_error e)
    {
#pragma omp critical (iyb_calc_fail)
        { fail_msg = e.what(); failed = true; }
    }
}


//! iyb_calc
/*!
    Calculation of pencil beam monochromatic spectra for 1 measurement block.

    All in- and output variables as the WSV with the same name.

    \author Patrick Eriksson 
    \date   2009-10-16
*/
void iyb_calc(
        Workspace&                  ws,
        Vector&                     iyb,
        ArrayOfVector&              iyb_aux,
        ArrayOfMatrix&              diyb_dx,
        Matrix&                     geo_pos_matrix,
  const Index&                      mblock_index,
  const Index&                      atmosphere_dim,
  ConstTensor3View                  t_field,
  ConstTensor3View                  z_field,
  ConstTensor4View                  vmr_field,
  const Index&                      cloudbox_on,
  const Index&                      stokes_dim,
  ConstVectorView                   f_grid,
  ConstMatrixView                   sensor_pos,
  ConstMatrixView                   sensor_los,
  ConstMatrixView                   transmitter_pos,
  ConstMatrixView                   mblock_dlos_grid,
  const String&                     iy_unit,  
  const Agenda&                     iy_main_agenda,
  const Agenda&                     geo_pos_agenda,
  const Index&                      j_analytical_do,
  const ArrayOfRetrievalQuantity&   jacobian_quantities,
  const ArrayOfArrayOfIndex&        jacobian_indices,
  const ArrayOfString&              iy_aux_vars,
  const Verbosity&                  verbosity)
{
  CREATE_OUT3;

  // Sizes
  const Index   nf   = f_grid.nelem();
  const Index   nlos = mblock_dlos_grid.nrows();
  const Index   niyb = nf * nlos * stokes_dim;
  // Set up size of containers for data of 1 measurement block.
  // (can not be made below due to parallalisation)
  iyb.resize( niyb );
  //
  if( j_analytical_do )
    {
      diyb_dx.resize( jacobian_indices.nelem() );
      FOR_ANALYTICAL_JACOBIANS_DO(
        diyb_dx[iq].resize( niyb, jacobian_indices[iq][1] -
                                  jacobian_indices[iq][0] + 1 );
      )
    }
  else
    { diyb_dx.resize( 0 ); }
  // Assume that geo_pos_agenda returns empty geo_pos.
  geo_pos_matrix.resize( nlos, atmosphere_dim );
  geo_pos_matrix = NAN;

  // For iy_aux we don't know the number of quantities, and we have to store
  // all outout
  ArrayOfArrayOfTensor4  iy_aux_array( nlos );

  // We have to make a local copy of the Workspace and the agendas because
  // only non-reference types can be declared firstprivate in OpenMP
  Workspace l_ws (ws);
  Agenda l_iy_main_agenda (iy_main_agenda);
  Agenda l_geo_pos_agenda (geo_pos_agenda);

  String fail_msg;
  bool failed = false;
  if (nlos >= arts_omp_get_max_threads() || nlos*10 >= nf)
    {
      out3 << "  Parallelizing los loop (" << nlos << " iterations, "
      << nf << " frequencies)\n";

      // Start of actual calculations
#pragma omp parallel for                   \
if (!arts_omp_in_parallel()) \
firstprivate(l_ws, l_iy_main_agenda, l_geo_pos_agenda)
      for( Index ilos=0; ilos<nlos; ilos++ )
        {
          // Skip remaining iterations if an error occurred
          if (failed) continue;
          
          Ppath ppath;
          iyb_calc_body( failed, fail_msg, iy_aux_array, l_ws,
                         ppath, iyb, diyb_dx,
                         mblock_index, atmosphere_dim, t_field, z_field,
                         vmr_field, cloudbox_on, stokes_dim, f_grid,
                         sensor_pos, sensor_los, transmitter_pos,
                         mblock_dlos_grid, iy_unit, 
                         l_iy_main_agenda, j_analytical_do, 
                         jacobian_quantities, jacobian_indices,
                         iy_aux_vars, ilos, nf );
          
          // Skip remaining iterations if an error occurred
          if (failed) continue;

          // Note that this code is found in two places inside the function
          Vector geo_pos;
          try
          {
              geo_pos_agendaExecute( l_ws, geo_pos, ppath, l_geo_pos_agenda );
              if( geo_pos.nelem() )
                {
                  if( geo_pos.nelem() != atmosphere_dim )
                      throw runtime_error( "Wrong size of *geo_pos* obtained "
                                          "from *geo_pos_agenda*.\nThe length of *geo_pos* must "
                                          "be zero or equal to *atmosphere_dim*." );

                  geo_pos_matrix(ilos,joker) = geo_pos;
                }
          }
          catch (runtime_error e)
          {
#pragma omp critical (iyb_calc_fail)
              { fail_msg = e.what(); failed = true; }
          }
        }
    }
  else
    {
      out3 << "  Not parallelizing los loop (" << nlos << " iterations, "
      << nf << " frequencies)\n";

      for( Index ilos=0; ilos<nlos; ilos++ )
        {
          // Skip remaining iterations if an error occurred
          if (failed) continue;

          Ppath ppath;
          iyb_calc_body( failed, fail_msg, iy_aux_array, l_ws, 
                         ppath, iyb, diyb_dx,
                         mblock_index, atmosphere_dim, t_field, z_field,
                         vmr_field, cloudbox_on, stokes_dim, f_grid,
                         sensor_pos, sensor_los, transmitter_pos,
                         mblock_dlos_grid, iy_unit, 
                         l_iy_main_agenda, j_analytical_do, 
                         jacobian_quantities, jacobian_indices,
                         iy_aux_vars, ilos, nf );

          // Skip remaining iterations if an error occurred
          if (failed) continue;

          // Note that this code is found in two places inside the function
          Vector geo_pos;
          try
          {
              geo_pos_agendaExecute( l_ws, geo_pos, ppath, l_geo_pos_agenda );
              if( geo_pos.nelem() )
                {
                  if( geo_pos.nelem() != atmosphere_dim )
                      throw runtime_error( "Wrong size of *geo_pos* obtained "
                                          "from *geo_pos_agenda*.\nThe length of *geo_pos* must "
                                          "be zero or equal to *atmosphere_dim*." );

                  geo_pos_matrix(ilos,joker) = geo_pos;
                }
          }
          catch (runtime_error e)
          {
#pragma omp critical (iyb_calc_fail)
              { fail_msg = e.what(); failed = true; }
          }
        }
    }

  if( failed )
    throw runtime_error("Run-time error in function: iyb_calc\n" + fail_msg);

  // Compile iyb_aux
  //
  const Index nq = iy_aux_array[0].nelem();
  iyb_aux.resize( nq );
  //
  for( Index q=0; q<nq; q++ )
    {
      iyb_aux[q].resize( niyb );
      //
      for( Index ilos=0; ilos<nlos; ilos++ )
        {
          const Index row0 = ilos * nf * stokes_dim;
          for( Index iv=0; iv<nf; iv++ )
            { 
              const Index row1 = row0 + iv*stokes_dim;
              const Index i1 = min( iv, iy_aux_array[ilos][q].nbooks()-1 );
              for( Index is=0; is<stokes_dim; is++ )
                { 
                  Index i2 = min( is, iy_aux_array[ilos][q].npages()-1 );
                  iyb_aux[q][row1+is] = iy_aux_array[ilos][q](i1,i2,0,0);
                }
            }
        }
    }
}



//! iy_transmission_mult
/*!
    Multiplicates iy_transmission with (vector) transmissions.

    That is, a multiplication of *iy_transmission* with another
    variable having same structure and holding transmission values.

    The "new path" is assumed to be further away from the sensor than 
    the propagtion path already included in iy_transmission. That is,
    the operation can be written as:
    
       Ttotal = Told * Tnew

    where Told is the transmission corresponding to *iy_transmission*
    and Tnew corresponds to *tau*.

    *iy_trans_new* is sized by the function.

    \param   iy_trans_total    Out: Updated version of *iy_transmission*
    \param   iy_trans_old      A variable matching *iy_transmission.
    \param   iy_trans_new      A variable matching *iy_transmission.

    \author Patrick Eriksson 
    \date   2009-10-06
*/
void iy_transmission_mult( 
       Tensor3&      iy_trans_total,
  ConstTensor3View   iy_trans_old,
  ConstTensor3View   iy_trans_new )
{
  const Index nf = iy_trans_old.npages();
  const Index ns = iy_trans_old.ncols();

  assert( ns == iy_trans_old.nrows() );
  assert( nf == iy_trans_new.npages() );
  assert( ns == iy_trans_new.nrows() );
  assert( ns == iy_trans_new.ncols() );

  iy_trans_total.resize( nf, ns, ns );

  for( Index iv=0; iv<nf; iv++ )
    {
      mult( iy_trans_total(iv,joker,joker), iy_trans_old(iv,joker,joker),
                                            iy_trans_new(iv,joker,joker) );
    } 
}



//! iy_transmission_mult_scalar_tau
//! los3d
/*!
    Converts any LOS vector to the implied 3D LOS vector.

    The output argument, *los3d*, is a vector with length 2, with azimuth angle
    set and zenith angle always >= 0. 

    \param   los3d             Out: The line-of-sight in 3D
    \param   los               A line-of-sight
    \param   atmosphere_dim    As the WSV.

    \author Patrick Eriksson 
    \date   2012-07-10
*/
void los3d(
        Vector&     los3d,
  ConstVectorView   los, 
  const Index&      atmosphere_dim )
{
  los3d.resize(2);
  //
  los3d[0] = abs( los[0] ); 
  //
  if( atmosphere_dim == 1 )
    { los3d[1] = 0; }
  else if( atmosphere_dim == 2 )
    {
      if( los[0] >= 0 )
        { los3d[1] = 0; }
      else
        { los3d[1] = 180; }
    }
  else if( atmosphere_dim == 3 )
    { los3d[1] = los[1]; }
}    



//! mirror_los
/*!
    Determines the backward direction for a given line-of-sight.

    This function can be used to get the LOS to apply for extracting single
    scattering properties, if the propagation path LOS is given.

    A viewing direction of aa=0 is assumed for 1D. This corresponds to 
    positive za for 2D.

    \param   los_mirrored      Out: The line-of-sight for reversed direction.
    \param   los               A line-of-sight
    \param   atmosphere_dim    As the WSV.

    \author Patrick Eriksson 
    \date   2011-07-15
*/
void mirror_los(
        Vector&     los_mirrored,
  ConstVectorView   los, 
  const Index&      atmosphere_dim )
{
  los_mirrored.resize(2);
  //
  if( atmosphere_dim == 1 )
    { 
      los_mirrored[0] = 180 - los[0]; 
      los_mirrored[1] = 180; 
    }
  else if( atmosphere_dim == 2 )
    {
      los_mirrored[0] = 180 - fabs( los[0] ); 
      if( los[0] >= 0 )
        { los_mirrored[1] = 180; }
      else
        { los_mirrored[1] = 0; }
    }
  else if( atmosphere_dim == 3 )
    { 
      los_mirrored[0] = 180 - los[0]; 
      los_mirrored[1] = los[1] + 180; 
      if( los_mirrored[1] > 180 )
        { los_mirrored[1] -= 360; }
    }
}    



//! pos2true_latlon
/*!
    Determines the true alt and lon for an "ARTS position"

    The function disentangles if the geographical position shall be taken from
    lat_grid and lon_grid, or lat_true and lon_true.

    \param   lat              Out: True latitude.
    \param   lon              Out: True longitude.
    \param   atmosphere_dim   As the WSV.
    \param   lat_grid         As the WSV.
    \param   lat_true         As the WSV.
    \param   lon_true         As the WSV.
    \param   pos              A position, as defined for rt calculations.

    \author Patrick Eriksson 
    \date   2011-07-15
*/
void pos2true_latlon( 
          Numeric&     lat,
          Numeric&     lon,
    const Index&       atmosphere_dim,
    ConstVectorView    lat_grid,
    ConstVectorView    lat_true,
    ConstVectorView    lon_true,
    ConstVectorView    pos )
{
  assert( pos.nelem() == atmosphere_dim );

  if( atmosphere_dim == 1 )
    {
      assert( lat_true.nelem() == 1 );
      assert( lon_true.nelem() == 1 );
      //
      lat = lat_true[0];
      lon = lon_true[0];
    }

  else if( atmosphere_dim == 2 )
    {
      assert( lat_true.nelem() == lat_grid.nelem() );
      assert( lon_true.nelem() == lat_grid.nelem() );
      GridPos   gp;
      Vector    itw(2);
      gridpos( gp, lat_grid, pos[1] );
      interpweights( itw, gp );
      lat = interp( itw, lat_true, gp );
      lon = interp( itw, lon_true, gp );
    }

  else 
    {
      lat = pos[1];
      lon = pos[2];
    }
}


//! vectorfield2los
/*!
    Calculates the size and direction of a vector field defined as u, v and w
    components.

    \param   l      Size/magnitude of the vector.
    \param   los    Out: The direction, as a LOS vector
    \param   u      Zonal component of the vector field
    \param   v      N-S component of the vector field
    \param   w      Vertical component of the vector field

    \author Patrick Eriksson 
    \date   2012-07-10
*/
void vectorfield2los(
        Numeric&    l,
        Vector&     los,
  const Numeric&    u,
  const Numeric&    v,
  const Numeric&    w )
{
  l= sqrt( u*u + v*v + w*w );
  //
  los.resize(2);
  //
  los[0] = acos( w / l );
  los[1] = atan2( u, v );   
}    



