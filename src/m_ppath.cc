/* Copyright (C) 2002 Patrick Eriksson <Patrick.Eriksson@rss.chalmers.se>
                            
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



/*****************************************************************************
 ***  File description 
 *****************************************************************************/

/*!
  \file   m_ppath.cc
  \author Patrick Eriksson <Patrick.Eriksson@rss.chalmers.se>
  \date   2002-05-08 

  \brief  Workspace functions releated to propagation paths variables.

  The file includes special functions to set the sensor position and LOS,
  and functions for calculation of propagation paths.

  These functions are listed in the doxygen documentation as entries of the
  file auto_md.h.
*/



/*****************************************************************************
 *** External declarations
 *****************************************************************************/

#include <math.h>
#include "arts.h"
#include "auto_md.h"
#include "check_input.h"
#include "math_funcs.h"
#include "ppath.h"



/*****************************************************************************
 *** The functions (in alphabetical order)
 *****************************************************************************/

//! a_posAddGeoidWGS84
/*! 
   See the the online help (arts -d FUNCTION_NAME)

   \author Patrick Eriksson
   \date   2002-05-16
*/
void a_posAddGeoidWGS84(
        // WS Output:
              Vector&    a_pos,
        // WS Input:
        const Index&     atmosphere_dim,
        const Numeric&   latitude_1d,
        const Numeric&   azimuth_angle_1d )
{
  // Check input
  chk_if_in_range( "atmosphere_dim", atmosphere_dim, 1, 3 );
  chk_vector_length( "a_pos", a_pos, atmosphere_dim );

  // Use *sensor_posAddGeoidWGS84* to perform the calculations.
  Matrix m(1,a_pos.nelem());
  m(0,Range(joker)) = a_pos;
  sensor_posAddGeoidWGS84( m, atmosphere_dim, latitude_1d, azimuth_angle_1d);
  a_pos[0] = m(0,0);
}



//! ppathCalc
/*! 
   See the the online help (arts -d FUNCTION_NAME)

   \author Patrick Eriksson
   \date   2002-05-16
*/
void ppathCalc(
        // WS Output:
              Ppath&          ppath,
        // WS Input:
        const Index&          atmosphere_dim,
        const Vector&         p_grid,
        const Vector&         lat_grid,
        const Vector&         lon_grid,
        const Tensor3&        z_field,
        const Matrix&         r_geoid,
        const Matrix&         z_ground,
        const Index&          blackbody_ground,
        const Index&          cloudbox_on, 
        const ArrayOfIndex&   cloudbox_limits,
        const Vector&         a_pos,
        const Vector&         a_los )
{
  //--- Check input -----------------------------------------------------------

  // Check that data sizes of grids, z_field and ground variables match the 
  // atmospheric dimensionality.
  //
  chk_if_in_range( "atmosphere_dim", atmosphere_dim, 1, 3 );
  chk_atm_grids( atmosphere_dim, p_grid, lat_grid, lon_grid );
  chk_atm_field( "z_field", z_field, atmosphere_dim, p_grid, lat_grid, 
                                                                    lon_grid );
  chk_atm_surface( "r_geoid", r_geoid, atmosphere_dim, lat_grid, lon_grid );
  chk_atm_surface( "z_ground", z_ground, atmosphere_dim, lat_grid, lon_grid );

  // Check that latitude and longitude grids are inside OK ranges for 3D
  if( atmosphere_dim == 3 )
    {
      if( lat_grid[0] < -90 )
	throw runtime_error( 
                  "The latitude grid cannot extend below -90 degrees for 3D" );
      if( last(lat_grid) > 90 )
	throw runtime_error( 
                  "The latitude grid cannot extend above 90 degrees for 3D" );
      if( lon_grid[0] < -360 )
	throw runtime_error( 
                "The longitude grid cannot extend below -360 degrees for 3D" );
      if( last(lon_grid) > 360 )
	throw runtime_error( 
                "The longitude grid cannot extend above 360 degrees for 3D" );
    }

  // Check that z_field has strictly increasing pages.
  //
  for( Index row=0; row<z_field.nrows(); row++ )
    {
      for( Index col=0; col<z_field.ncols(); col++ )
	{
	  // I could not get the compliler to accept a solution without dummy!!
	  Vector dummy(z_field.npages());
	  dummy = z_field(Range(joker),row,col);
	  ostringstream os;
	  os << "z_field (for latitude nr " << row << " and longitude nr " 
             << col << ")";
	  chk_if_increasing( os.str(), dummy ); 
	}
    }

  // Check that there is no gap between the ground and lowest pressure surface
  //
  for( Index row=0; row<z_ground.nrows(); row++ )
    {
      for( Index col=0; col<z_ground.ncols(); col++ )
	{
	  if( z_ground(row,col)<z_field(0,row,col) || 
                       z_ground(row,col)>=z_field(z_field.npages()-1,row,col) )
	    {
	      ostringstream os;
	      os << "The ground altitude (*z_ground*) cannot be outside of the"
		 << " altitudes in *z_field*.";
		if( atmosphere_dim > 1 )
		  os << "\nThis was found to be the case for:\n" 
		     << "latitude " << lat_grid[row];
		if( atmosphere_dim > 2 )
		  os << "\nlongitude " << lon_grid[col];
	      throw runtime_error( os.str() );
	    }
	}
    }

  // Blackbody ground and cloud box
  //
  chk_if_bool(  "blackbody_ground", blackbody_ground );
  chk_cloudbox( atmosphere_dim, p_grid, lat_grid, lon_grid, blackbody_ground, 
		cloudbox_on, cloudbox_limits );

  // Sensor position and LOS
  //
  chk_vector_length( "a_pos", a_pos, atmosphere_dim );
  chk_if_over_0( "sensor radius", a_pos[0] );
  if( atmosphere_dim == 1 )
    {
      chk_vector_length( "a_los", a_los, 1 );
      chk_if_in_range( "sensor zenith angle", a_los[0], 0, 180 );
    }
  else if( atmosphere_dim == 2 )
    {
      chk_vector_length( "a_los", a_los, 1 );
      chk_if_in_range( "sensor zenith angle", a_los[0], -180, 180 );
    }
  else
    {
      chk_if_in_range( "sensor latitude", a_pos[1], -90, 90 );
      chk_if_in_range( "sensor longitude", a_pos[2], -360, 360 );
      chk_vector_length( "a_los", a_los, 2 );
      chk_if_in_range( "sensor zenith angle", a_los[0], 0, 180 );
      chk_if_in_range( "sensor azimuth angle", a_los[1], -180, 180 );
    }
  
  //--- End: Check input ------------------------------------------------------


  // Initiate the partial Ppath structure. 
  // The function doing the work sets ppath_step to the last point of the
  // path inside the atmosphere, if the path is at all inside the atmosphere.
  // If the background field is set by the function this flags that there is no
  // path to follow (for example when the sensor is inside the cloud box).
  // The function checks also that the sensor and the last point of the path 
  // are at allowed locations.
  //
  Ppath ppath_step;
  //
  ppath_start_stepping( ppath_step, atmosphere_dim, p_grid, lat_grid, 
                        lon_grid, z_field, r_geoid, z_ground, blackbody_ground,
                        cloudbox_on, cloudbox_limits, a_pos, a_los );

  // Perform propagation path steps until the starting point is found, which
  // is flagged by ppath_step by setting the background field.
  //
  // The results of each step, returned by ppath_step_agenda as a new 
  // ppath_step, are stored as an array of Ppath structures.
  //
  Array<Ppath>   ppath_array;
  ppath_array.push_back( ppath_step );
  // 
  Index   np = ppath_step.np;   // Counter for number of points of the path
  //
  const Index imax_p   = p_grid.nelem() - 1;
  const Index imax_lat = lat_grid.nelem() - 1;
  const Index imax_lon = lon_grid.nelem() - 1;
  //
  while( !ppath_what_background( ppath_step ) )
    {
      // Call ppath_step agenda

      // For the moment, a hard-coded version:
      ppath_stepGeometric( ppath_step, atmosphere_dim, p_grid, lat_grid, 
		      lon_grid, z_field, r_geoid, z_ground, blackbody_ground );


      // Number of points in returned path step
      const Index n = ppath_step.np;

      // Increase the total number
      np += n - 1;

      // Put new ppath_step in ppath_array
      ppath_array.push_back( ppath_step );

      // Check if the top of the atmosphere is reached
      if( is_gridpos_at_index_i( ppath_step.gp_p[n-1], imax_p ) )
	{ ppath_set_background( ppath_step, 1 ); }

      // Check that path does not exit at a latitude or longitude end face
      if( atmosphere_dim >= 2 )
	{
	  if( is_gridpos_at_index_i( ppath_step.gp_lat[n-1], 0 ) )
	    {
	      ostringstream os;
	      os << "The path enters the atmosphere through the lower latitude"
		 << "end face,\nat altitude " << ppath_step.z[n-1]/1e3 
                 << " km.";
	      throw runtime_error( os.str() );
	    }
	  if( is_gridpos_at_index_i( ppath_step.gp_lat[n-1], imax_lat ) )
	    {
	      ostringstream os;
	      os << "The path enters the atmosphere through the upper latitude"
		 << "end face,\nat altitude " << ppath_step.z[n-1]/1e3 
                 << " km.";
	      throw runtime_error( os.str() );
	    }

	  if( atmosphere_dim == 3 )
	    {
	      if( is_gridpos_at_index_i( ppath_step.gp_lon[n-1], 0 ) )
		{
		  ostringstream os;
		  os << "The path enters the atmosphere through the lower " 
		     << "longitude end face,\nat altitude " 
		     << ppath_step.z[n-1]/1e3 << " km.";
		  throw runtime_error( os.str() );
		}
	      if( is_gridpos_at_index_i( ppath_step.gp_lon[n-1], imax_lon ))
		{
		  ostringstream os;
		  os << "The path enters the atmosphere through the upper "
                     << "longitude end face,\nat altitude " 
                     << ppath_step.z[n-1]/1e3 << " km.";
		  throw runtime_error( os.str() );
		}
	    }
	}

      // Check if there is an intersection with an active cloud box
      if( cloudbox_on )
	{
	  Numeric ipos = Numeric( ppath_step.gp_p[n-1].idx ) + 
                                                    ppath_step.gp_p[n-1].fd[0];
	  if( ipos >= Numeric( cloudbox_limits[0] )  && 
	                                ipos <= Numeric( cloudbox_limits[1] ) )
	    {
	      if( atmosphere_dim == 1 )
		{ ppath_set_background( ppath_step, 3 ); }
	      else
		{
		  ipos = Numeric( ppath_step.gp_lat[n-1].idx ) + 
                                                  ppath_step.gp_lat[n-1].fd[0];
		  if( ipos >= Numeric( cloudbox_limits[2] )  && 
	                                ipos <= Numeric( cloudbox_limits[3] ) )
		    {
		      if( atmosphere_dim == 2 )
			{ ppath_set_background( ppath_step, 3 ); }
		      else
			{
			  ipos = Numeric( ppath_step.gp_lon[n-1].idx ) + 
                                                  ppath_step.gp_lon[n-1].fd[0];
			  if( ipos >= Numeric( cloudbox_limits[4] )  && 
	                                ipos <= Numeric( cloudbox_limits[5] ) )
			    { ppath_set_background( ppath_step, 3 ); } 
			}
		    }
		}
	    }
	}
    } // End path steps

  
  // Combine all structures in ppath_array to form the return Ppath structure.
  //
  ppath_init_structure( ppath, atmosphere_dim, np );
  //
  np = 0;   // Now used as counter for points moved to ppath
  //
  for( Index i=0; i<ppath_array.nelem(); i++ )
    {
      // For the first structure, the first point shall be included, but the
      // first structure can also be empty. 
      // For later structures, the first point shall not be included, but
      // there will always be at least two points.
      // Note that only the first structure can be empty.

      Index n = ppath_array[i].np;

      if( n )
	{
	  // First index to include
	  Index i1 = 1;
	  if( i == 0 )
	    { i1 = 0; }

	  // Vectors and matrices that can be handled by ranges.
	  ppath.z[ Range(np,n-i1) ] = ppath_array[i].z[ Range(i1,n-i1) ];
	  ppath.pos( Range(np,n-i1), Range(joker) ) = 
                            ppath_array[i].pos( Range(i1,n-i1), Range(joker) );
	  ppath.los( Range(np,n-i1), Range(joker) ) = 
	                    ppath_array[i].los( Range(i1,n-i1), Range(joker) );

	  // For i==1, there is no defined l_step. For higher i, all 
	  // values in l_step shall be copied.
	  if( i > 0 )
	    { ppath.l_step[ Range(np-1,n-1) ] = ppath_array[i].l_step; }

	  // Grid positions must be handled by a loop
	  for( Index j=i1; j<n; j++ )
	    { ppath.gp_p[np+j-i1] = ppath_array[i].gp_p[j]; }
	  if( atmosphere_dim >= 2 )
	    {
	      for( Index j=i1; j<n; j++ )
		{ ppath.gp_lat[np+j-i1] = ppath_array[i].gp_lat[j]; }
	    }
	  if( atmosphere_dim == 3 )
	    {
	      for( Index j=i1; j<n; j++ )
		{ ppath.gp_lon[np+j-i1] = ppath_array[i].gp_lon[j]; }
	    }

	  // Fields just set once
	  if( ppath_array[i].ground )
	    {
	      ppath.ground                    = ppath_array[i].ground;
	      ppath.i_ground                  = np + ppath_array[i].i_ground;
	    }
	  if( ppath_array[i].tan_pos.nelem() )
	    {
	      ppath.tan_pos.resize( ppath_array[i].tan_pos.nelem() );
	      ppath.tan_pos                   = ppath_array[i].tan_pos; 
	    }
	  if( ppath_array[i].symmetry )
	    {
	      ppath.symmetry                  = ppath_array[i].symmetry;
	      ppath.i_symmetry                = np + ppath_array[i].i_symmetry;
	    }

	  // Increase number of points done
	  np += n - i1;
	}
    }  
  ppath.method     = ppath_step.method;
  ppath.refraction = ppath_step.refraction;
  ppath.constant   = ppath_step.constant;
  ppath.background = ppath_step.background;


  // Print the structure
  if( 1 )
    {
      PrintIndex( ppath.dim, "dim" );
      PrintIndex( ppath.np, "np" );
      PrintString( ppath.method, "method" );
      PrintIndex( ppath.refraction, "refraction" );
      PrintNumeric( ppath.constant, "constant" );
      PrintString( ppath.background, "background" );
      PrintIndex( ppath.ground, "ground" );
      if( ppath.ground )
        PrintIndex( ppath.i_ground, "i_ground" );
      PrintIndex( ppath.symmetry, "symmetry" );
      if( ppath.symmetry )
        PrintIndex( ppath.i_symmetry, "i_symmetry" );
      if( ppath.tan_pos.nelem() )
	PrintVector( ppath.tan_pos, "tan_pos" );
      PrintMatrix( ppath.pos, "pos" );
      PrintVector( ppath.z, "z" );
      PrintVector( ppath.l_step, "l_step" );
      PrintMatrix( ppath.los, "los" );
    }
}



//! ppath_stepGeometric
/*!
   See the the online help (arts -d FUNCTION_NAME)

   \author Patrick Eriksson
   \date   2002-05-28
*/
void ppath_stepGeometric(
        // WS Output:
              Ppath&     ppath_step,
        // WS Input:
        const Index&     atmosphere_dim,
        const Vector&    p_grid,
        const Vector&    lat_grid,
        const Vector&    lon_grid,
        const Tensor3&   z_field,
        const Matrix&    r_geoid,
        const Matrix&    z_ground,
        const Index&     blackbody_ground )
{
  // Input checks here would be rather costly as this function is called
  // many times. So we perform asserts in the sub-functions, but no checks 
  // here. This commented in the on-line information.

  // Note that lmax is here set to 9999 km

  if( atmosphere_dim == 1 )
    { ppath_step_geom_1d( ppath_step, atmosphere_dim, p_grid, 
               z_field(Range(joker),0,0), r_geoid(0,0), z_ground(0,0),
                                                    blackbody_ground, 9999e3 );
    }
  else
    {
      throw runtime_error(
                     "2D and 3D propagation path steps are not yet handled." );
    }
}



//! ppath_stepGeometricWithLmax
/*!
   See the the online help (arts -d FUNCTION_NAME)

   \author Patrick Eriksson
   \date   2002-05-28
*/
void ppath_stepGeometricWithLmax(
        // WS Output:
              Ppath&     ppath_step,
        // WS Input:
        const Index&     atmosphere_dim,
        const Vector&    p_grid,
        const Vector&    lat_grid,
        const Vector&    lon_grid,
        const Tensor3&   z_field,
        const Matrix&    r_geoid,
        const Matrix&    z_ground,
        const Index&     blackbody_ground,
        // Control Parameters:
        const Numeric&   lmax)

{
  // Input checks here would be rather costly as this function is called
  // many times. So we perform asserts in the sub-functions, but no checks 
  // here. This commented in the on-line information for ppath_stepGeometric.

  if( atmosphere_dim == 1 )
    { ppath_step_geom_1d( ppath_step, atmosphere_dim, p_grid, 
               z_field(Range(joker),0,0), r_geoid(0,0), z_ground(0,0),
                                                      blackbody_ground, lmax );
    }
  else
    {
      throw runtime_error(
                     "2D and 3D propagation path steps are not yet handled." );
    }
}



//! sensor_posAddGeoidWGS84
/*!
   See the the online help (arts -d FUNCTION_NAME)

   \author Patrick Eriksson
   \date   2002-05-16
*/
void sensor_posAddGeoidWGS84(
        // WS Output:
              Matrix&    sensor_pos,
        // WS Input:
        const Index&     atmosphere_dim,
        const Numeric&   latitude_1d,
        const Numeric&   azimuth_angle_1d )
{
  // Check input
  chk_if_in_range( "atmosphere_dim", atmosphere_dim, 1, 3 );
  chk_matrix_ncols( "sensor_pos", sensor_pos, atmosphere_dim );

  // Number of positions
  const Index npos = sensor_pos.nrows();
  if( npos == 0 )
    throw runtime_error("The number of positions is 0, must be at least 1.");

  // The function *r_geoidWGS84 is used to get the geoid radius, but some
  // tricks are needed as this a WSF to set *r_geoid* for all crossings of the
  // latitude and longitude grids.
  // For 2D and 3D, the function is always called with the atmospheric 
  // dimensionality set to 2, and the latitudes of concern are put into 
  // the latitude grid. An extra dummy value is needed if there is only one 
  // position in *sensor_pos*.

  if( atmosphere_dim == 1 )
    {
      Vector lats(0);

      // The size of the r-matrix is set inside the function.
      Matrix r;
      r_geoidWGS84( r, 1, lats, Vector(0), latitude_1d, azimuth_angle_1d );
      
      // Add the geoid radius to the geometric altitudes
      sensor_pos(Range(joker),0) += r(0,0);
    }

  else
    {
      Vector lats;
      if( npos == 1 )
	{
	  lats.resize(2);
	  lats[0] = sensor_pos(0,1);
	  lats[1] = lats[0] + 1;
	}
      else
	{
	  lats.resize(npos);
          lats = sensor_pos( Range(joker), 1 );
	}

      // The size of the r-matrix is set inside the function.
      Matrix r;
      r_geoidWGS84( r, 2, lats, Vector(0), -999, -999 );
      
      // Add the geoid radius to the geometric altitudes
      for( Index i=0; i<npos; i++ )
	sensor_pos(i,0) += r(i,0);
    }
}



