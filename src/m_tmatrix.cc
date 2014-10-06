/* Copyright (C) 2013 Oliver Lemke
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*!
  \file   m_tmatrix.cc
  \author Oliver Lemke
  \date   2013-06-25

  \brief  T-Matrix related workspace methods.
*/

#include <stdexcept>
#include <cmath>
#include "messages.h"
#include "tmatrix.h"
#include "check_input.h"
#include "make_array.h"
#include "refraction.h"
#include "special_interp.h"

extern const Numeric PI;  



/* Workspace method: Doxygen documentation will be auto-generated */
void ParticleDmaxFromDe(
          Numeric&                dmax,
          Numeric&                volume,
    const String&                 shape,
    const Numeric&                de,
    const Numeric&                aratio,
    const Verbosity&)
{
  volume = ( 4.*PI*pow(de/2.,3) ) / 3.;

  if( shape == "spheroidal" )
    {
      if( aratio > 1 ) // oblate spheriod
        {   
          //rotational axis
          const Numeric a = pow( (3.*volume*aratio)/(4*PI), 1./3.);
          dmax = 2.*a;
        }
      else if( aratio < 1) // prolate spheroid
        {  
          //non-rotational axis (perpendicular to a)
          const Numeric b = pow( (3.*volume)/(4.*PI*pow(aratio,2)),1./3.);
          dmax = 2.*b;
        }
      else
        {
          throw runtime_error( "For spheriodal particles, the aspect ratio "
                               "is not allowed to be exactly 1 (due to "
                               "potential numerical problems)." );
        }
    }
  
  else if (shape == "cylindrical")
    {
      //aratio=D/L
      const Numeric D = pow( (volume*4*aratio)/PI, 1./3.);
      const Numeric L = D / aratio;
      dmax = pow( pow(D,2) + pow(L,2), 1./2. );
    }
  
  else
    {
      ostringstream os;
      os << "Unknown particle shape: " << shape << "\n"
         << "Must be spheroidal or cylindrical";
      throw runtime_error( os.str() );
    }
}



/* Workspace method: Doxygen documentation will be auto-generated */
void ParticleDeFromDmax(
          Numeric&                de,
          Numeric&                volume,
    const String&                 shape,
    const Numeric&                dmax,
    const Numeric&                aratio,
    const Verbosity&)
{
  if( shape == "spheroidal" )
    {
      if( aratio > 1 ) // oblate spheriod
        {   
          const Numeric a = dmax/2.;
          volume = (pow(a,3.)*4.*PI)/(3.*aratio);
        }
      else if( aratio < 1) // prolate spheroid
        {  
          const Numeric b = dmax/2.;
          volume = (pow(b,3.)*4.*PI*pow(aratio,2.))/3.;
        }
      else
        {
          throw runtime_error( "For spheriodal particles, the aspect ratio "
                               "is not allowed to be exactly 1 (due to "
                               "numerical problems)." );
        }
    }
  
  else if (shape == "cylindrical")
    {
      volume = pow( dmax/pow((pow(aratio,2.)+1.),1./2.),3)*pow(aratio,2.)*PI/4.;
    }
  
  else
    {
      ostringstream os;
      os << "Unknown particle shape: " << shape << "\n"
         << "Must be spheroidal or cylindrical";
      throw runtime_error( os.str() );
    }

  de = pow( (24.*volume) / (4.*PI), 1./3. );
}



/* Workspace method: Doxygen documentation will be auto-generated */
void scat_dataTmatrix(
          SingleScatteringData&   scat_data,
    const GriddedField3&          complex_refr_index,
    const String&                 shape,
    const Numeric&                de,
    const Numeric&                aratio,
    const String&                 ptype,
    const Vector&                 scat_f_grid,
    const Vector&                 scat_t_grid,
    const Vector&                 scat_za_grid,
    const Vector&                 scat_aa_grid,
    const Numeric&                precision,
    const Verbosity&)
{
  // Add grids to scat_data
  //
  scat_data.f_grid  = scat_f_grid;
  scat_data.T_grid  = scat_t_grid;
  scat_data.za_grid = scat_za_grid;
  scat_data.aa_grid = scat_aa_grid;

  // Index coding for shape
  Index np;
  if( shape == "spheroidal" )
    { 
      np=-1; 
      if( aratio == 1 )
        throw runtime_error( "For spheriodal particles, the aspect ratio "
                             "is not allowed to be exactly 1 (due to "
                             "numerical problems)." );
    }
  else if( shape == "cylindrical" )
    { np=-2; }
  else
    {
      ostringstream os;
      os << "Unknown particle shape: " << shape << "\n"
         << "Must be spheroidal or cylindrical";
      throw runtime_error( os.str() );
    }

  // Get internal coding for ptype
  scat_data.ptype = PTypeFromString( ptype );

  // Set description
  {   
    ostringstream os;
    os << "T-matrix calculation for " << shape 
       << " with De=" << 1e6*de << "um and aspect ratio of " << aratio;
    scat_data.description = os.str();
  }

  // Interpolate refractive index to relevant grids
  //
  const Index nf = scat_f_grid.nelem();
  const Index nt = scat_t_grid.nelem();
  //
  Tensor3 ncomp( nf, nt, 2 );
  complex_n_interp( ncomp(joker,joker,0), ncomp(joker,joker,1),
                    complex_refr_index, "complex_refr_index", 
                    scat_f_grid, scat_t_grid );

  // Run T-matrix and we are ready (T-matrix takes de as radius in um)
  calcSingleScatteringDataProperties( scat_data,
                                      ncomp(joker,joker,0), 
                                      ncomp(joker,joker,1),
                                      0.5e6*de, np, aratio, precision );
}







void TMatrixTest(const Verbosity& verbosity)
{
    tmatrix_tmd_test(verbosity);
    tmatrix_ampld_test(verbosity);
    calc_ssp_random_test(verbosity);
    calc_ssp_fixed_test(verbosity);
}

//-----------------------------------


void scat_meta_arrayInit(// WS Output:
                 ArrayOfScatteringMetaData& scat_meta_array,
                 //WS Input
                 const Verbosity&)
{
    scat_meta_array.resize(0);
}

//-----------------------------------

                 
void scat_meta_arrayAddTmatrix(// WS Output:
                             ArrayOfScatteringMetaData& scat_meta_array,
                             // WS Input:
                             const GriddedField3& complex_refr_index,
                             // WS Generic input:
                             const String& description,
                             const String& material,
                             const String& shape,
                             const String& ptype,
                             const Numeric& density,
                             const Vector& aspect_ratio_grid,
                             const Vector& diameter_max_grid,
                             const Vector& scat_f_grid,
                             const Vector& scat_T_grid,
                             const Verbosity&) 

{
  for(Index k=0; k<diameter_max_grid.nelem(); k++)
    {
     for ( Index i=0; i < aspect_ratio_grid.nelem(); i++ )
      {
        Numeric volume;
        
        if (shape == "spheroidal")
        {
            if (aspect_ratio_grid[i]>1) // If oblate spheroid
            {   
                Numeric a; //rotational axis (perpendicular to a)
                
                a=diameter_max_grid[k]/2.;
                volume=pow(a,3.)*4.*PI/(3.*aspect_ratio_grid[i]);
            }
        
            else if (aspect_ratio_grid[i]<1) // If prolate spheroid
            {  
                Numeric b; //non-rotational axis (perpendicular to a)
                
                b=diameter_max_grid[k]/2.;
                volume=pow(b,3.)*4.*PI*pow(aspect_ratio_grid[i],2.)/3.;
            }
        
            else
            {
                ostringstream os;
                os << "Incorrect aspect ratio: " << aspect_ratio_grid[i] << "\n"
                << "Can not be equal to one";
                throw std::runtime_error(os.str());
            }
        }
        else if (shape == "cylindrical")
        {
            // Formulas to determine cylindrical volume from diamter_max:
            
            // D/L=aspect_ratio
            // diameter_max=sqrt(D²+L²)
            // volume=D³*pi/(4*aspect_ratio)
            
            volume=pow(diameter_max_grid[k]/pow((pow(aspect_ratio_grid[i], 2.)+1.), 1./2.), 3)*pow(aspect_ratio_grid[i], 2.)*PI/4.;
         }

        else
        {
            ostringstream os;
            os << "Unknown particle shape: " << shape << "\n"
            << "Must be spheroidal or cylindrical";
            throw std::runtime_error(os.str());
        }
        
        ScatteringMetaData smd;
        if (description=="")
        {   
            ostringstream os;
            os << shape<< " "<< material << " particle of type " << ptype<<
            ", with volume equivalent diameter "
            <<diameter_max_grid[k]<<" meters.";
            smd.description=os.str();
        }
        else 
            smd.description = description;
        
        smd.material = material;
        smd.shape = shape;
        smd.ptype = PTypeFromString(ptype);
        smd.ssd_method = PARTICLE_SSDMETHOD_TMATRIX;
        smd.density = density;
        smd.diameter_max =diameter_max_grid[k];
        smd.volume = volume;
        smd.area_projected = 0;
        smd.aspect_ratio = aspect_ratio_grid[i];
        smd.scat_f_grid = scat_f_grid;
        smd.scat_T_grid = scat_T_grid;
        smd.complex_refr_index = complex_refr_index;

        scat_meta_array.push_back(smd);
      }
    }
}

//-----------------------------------


void scat_data_arrayFromMeta(// WS Output:
                              ArrayOfSingleScatteringData& scat_data_array,
                              //WS Input
                              const ArrayOfScatteringMetaData& scat_meta_array,
                              // WS Generic input:
                              const Vector& za_grid,
                              const Vector& aa_grid,
                              const Numeric& precision,
                              const Verbosity&)
{
  for(Index ii=0;ii<scat_meta_array.nelem();ii++)
    {
        
    //Interpolation
        
    Tensor3 complex_refr_index;
        
        
    const Index nf = scat_meta_array[ii].scat_f_grid.nelem();
    const Index nt = scat_meta_array[ii].scat_T_grid.nelem();
       
    complex_refr_index.resize(nf,nt,2);
  
    complex_n_interp(complex_refr_index(joker, joker,0), complex_refr_index(joker, joker,1),
                     scat_meta_array[ii].complex_refr_index, "complex_refr_index", 
                     scat_meta_array[ii].scat_f_grid, scat_meta_array[ii].scat_T_grid);
        

      Index  np;

      SingleScatteringData sdd;
      sdd.f_grid = scat_meta_array[ii].scat_f_grid;
      sdd.T_grid = scat_meta_array[ii].scat_T_grid;
      sdd.za_grid = za_grid;
      sdd.aa_grid = aa_grid;
      sdd.ptype = scat_meta_array[ii].ptype;

      if (scat_meta_array[ii].shape == "spheroidal" )
        np=-1;

      else if (scat_meta_array[ii].shape == "cylindrical")
        np=-2;
      else
        {
          ostringstream os;
          os << "Unknown particle shape: " << scat_meta_array[ii].shape << "\n"
            << "Must be spheroidal or cylindrical";
          throw std::runtime_error(os.str());
        }

      calcSingleScatteringDataProperties(sdd,
                                         complex_refr_index(joker,joker,0),
                                         complex_refr_index(joker,joker,1),
                                         pow(scat_meta_array[ii].volume*1e18*3./(4.*PI),1./3.),
                                         np,
                                         scat_meta_array[ii].aspect_ratio,
                                         precision);

      scat_data_array.push_back(sdd);
    }
}











void scat_meta_arrayAddTmatrixOldVersion(// WS Output:
                             ArrayOfScatteringMetaData& scat_meta_array,
                             // WS Input:
                             const GriddedField3& complex_refr_index,
                             // WS Generic input:
                             const String& description,
                             const String& material,
                             const String& shape,
                             const String& ptype,
                             const Numeric& density,
                             const Numeric& aspect_ratio,
                             const Vector& diameter_grid,
                             const Vector& scat_f_grid,
                             const Vector& scat_T_grid,
                             const Verbosity&) 

{
  for(Index k=0; k<diameter_grid.nelem(); k++)
    {

      Numeric volume;
      
      volume=4./3.*PI*pow(diameter_grid[k]/2.,3);
      
      
      Numeric diameter_max;

      if (shape == "spheroidal")
        {
       if (aspect_ratio>1) // If oblate spheroid
            {   
            Numeric a; //rotational axis
            
            a=pow(3*volume*aspect_ratio/(4*PI), 1./3.);
            diameter_max=2*a;
            }
        
       else if (aspect_ratio<1) // If prolate spheroid
            {  
            Numeric b; //non-rotational axis (perpendicular to a)
            
            b=pow(3*volume/(4*PI*pow(aspect_ratio, 2)),1./3.);
            diameter_max=2*b;
            }
        
        else
            {
            ostringstream os;
            os << "Incorrect aspect ratio: " << aspect_ratio << "\n"
              << "Can not be equal to one";
            throw std::runtime_error(os.str());
            }
        }
      else if (shape == "cylindrical")
        {
            Numeric D;
            Numeric L;
            
            //aspect_ratio=D/L
            
            D=pow(volume*4./PI*aspect_ratio, 1./3.);
            L=D/aspect_ratio;
            diameter_max=pow((pow(D,2)+pow(L,2)), 1./2.);
         }

      else
        {
          ostringstream os;
          os << "Unknown particle shape: " << shape << "\n"
            << "Must be spheroidal or cylindrical";
          throw std::runtime_error(os.str());
        }

      ScatteringMetaData smd;
      if (description=="")
        {   
          ostringstream os;
          os << shape<< " "<< material << " particle of type " << ptype<<
            ", with volume equivalent diameter "
            <<diameter_grid[k]<<" meters.";
          smd.description=os.str();
        }
      else 
        smd.description = description;
 
      smd.material = material;
      smd.shape = shape;
      smd.ptype = PTypeFromString(ptype);
      smd.ssd_method = PARTICLE_SSDMETHOD_TMATRIX;
      smd.density = density;
      smd.diameter_max =diameter_max;
      smd.volume = volume;
      smd.area_projected = 0;
      smd.aspect_ratio = aspect_ratio;
      smd.scat_f_grid = scat_f_grid;
      smd.scat_T_grid = scat_T_grid;
      smd.complex_refr_index = complex_refr_index;

      scat_meta_array.push_back(smd);
    }
}

//-----------------------------------


