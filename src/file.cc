/* Copyright (C) 2000-2012 Stefan Buehler <sbuehler@ltu.se>

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


////////////////////////////////////////////////////////////////////////////
//   File description
////////////////////////////////////////////////////////////////////////////
/**
   \file  file.cc

   This file contains basic functions to handle ASCII data files.

   \author Patrick Eriksson
   \date 2000-10-28 
*/



////////////////////////////////////////////////////////////////////////////
//   External declarations
////////////////////////////////////////////////////////////////////////////

#include "arts.h"

#include <stdexcept>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <limits>

// For getdir
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "matpackI.h"
#include "array.h"
#include "messages.h"
#include "parameters.h"
#include "file.h"


////////////////////////////////////////////////////////////////////////////
//   Default file names
////////////////////////////////////////////////////////////////////////////

/**
   Gives the default file name for the ASCII formats.

   The default name is only used if the file name is empty.

   \param   filename Output:     file name
   \param    varname      variable name

   \author Patrick Eriksson              
   \date   2000-11-01
*/
void filename_ascii(
              String&  filename,
        const String&  varname )
{
  if ( "" == filename )
  {
    extern const String out_basename;                       
    filename = out_basename+"."+varname+".aa";
  }
}


////////////////////////////////////////////////////////////////////////////
//   Functions to open and read ASCII files
////////////////////////////////////////////////////////////////////////////

/**
   Open a file for writing. If the file cannot be opened, the
   exception IOError is thrown. 
   @param     file File pointer 
   @param     name Name of the file to open
   @author    Stefan Buehler
   @version   1
   @exception ios_base::failure Could for example mean that the
                      directory is read only. */
void open_output_file(ofstream& file, const String& name)
{
  String ename = add_basedir(name);
  
  try
  {
    // Tell the stream that it should throw exceptions.
    // Badbit means that the entire stream is corrupted, failbit means
    // that the last operation has failed, but the stream is still
    // valid. We don't want either to happen!
    // FIXME: This does not yet work in  egcs-2.91.66, try again later.
    file.exceptions(ios::badbit |
                    ios::failbit);
    
    // c_str explicitly converts to c String.
    file.open(ename.c_str() );
    
    // See if the file is ok.
    // FIXME: This should not be necessary anymore in the future, when
    // g++ stream exceptions work properly. (In that case we would not
    // get here if there really was a problem, because of the exception
    // thrown by open().)
  }
  catch (exception e)
  {
    ostringstream os;
    os << "Cannot open output file: " << ename << '\n'
    << "Maybe you don't have write access "
    << "to the directory or the file?";
    throw runtime_error(os.str());
  }
}


/**
 Closes the file. If it is empty, the file is deleted.
 @param     file File pointer 
 @author    Oliver Lemke
 @exception ios_base::failure Could for example mean that the
 directory is read only. */
#ifdef HAVE_REMOVE
void cleanup_output_file(ofstream& file, const String& name)
{
  if (file.is_open())
  {
    streampos fpos = file.tellp();
    file.close();
    if (!fpos) unlink(expand_path(name).c_str());
  } 
}
#else
void cleanup_output_file(ofstream&, const String&) {}
#endif


/**
   Open a file for reading. If the file cannot be opened, the
   exception IOError is thrown. 
   @param     file File pointer 
   @param     name Name of the file to open
   @author    Stefan Buehler
   @version   1
   @exception ios_base::failure Somehow the file cannot be opened. */
void open_input_file(ifstream& file, const String& name)
{
  String ename = expand_path(name);
  
  // Tell the stream that it should throw exceptions.
  // Badbit means that the entire stream is corrupted.
  // On the other hand, end of file will not lead to an exception, you
  // have to check this manually!
  file.exceptions(ios::badbit);

  // c_str explicitly converts to c String.
  file.open(ename.c_str() );

  // See if the file is ok.
  // FIXME: This should not be necessary anymore in the future, when
  // g++ stream exceptions work properly.
  if (!file)
    {
      ostringstream os;
      os << "Cannot open input file: " << ename << '\n'
         << "Maybe the file does not exist?";
      throw runtime_error(os.str());
    }
}


/**
   Read an ASCII stream and append the contents to the String array
   text.  TEXT IS NOT OVERWRITTEN, BUT APPENDED!

   @param text Output. The contents fo the file
   @param is Stream from which to read
   @exception IOError Some error occured during the read
   @version   1
   @author Stefan Buehler */
void read_text_from_stream(ArrayOfString& text, istream& is)
{
  String linebuffer;

  // Read as long as `is' is good.
  // Contary to what I understood from the book, the explicit check
  // for eof is necessary here, otherwise the last line is read twice
  // if it is not terminated by a newline character!
  while (is && is.good() && !is.eof())
    {
      // Read line from file into linebuffer:
      getline(is,linebuffer);
      
      // Append to end of text:
      text.push_back(linebuffer);
    }
  
  // Check for error:
  // FIXME: This should not be necessary anymore when stream
  // exceptions work properly.
  if ( !is.eof() ) {
    ostringstream os;
    os << "Read Error. Last line read:\n" << linebuffer;
    throw runtime_error(os.str());
  }

}


/**
   Reads an ASCII file and appends the contents to the String vector
   text. This uses the function @see read_text_from_stream. TEXT IS
   NOT OVERWRITTEN, BUT APPENDED!  

   \param text  Output. The contents fo the file
   \param name  Name of file to read
   \exception IOError
   \version   1
   \author Stefan Buehler */
void read_text_from_file(ArrayOfString& text, const String& name)
{
  ifstream ifs;

  // Open input stream:
  open_input_file(ifs, name);
  // No need to check for error, because open_input_file throws a
  // runtime_error with an appropriate error message.

  // Read the text from the stream. Here we catch the exception,
  // because then we can issue a nicer error message that includes the 
  // filename.
  try
    {
      read_text_from_stream(text,ifs);
    }
  catch (runtime_error x)
    {
      ostringstream os;
      os << "Error reading file: " << name << '\n'
         << x.what();
      throw runtime_error(os.str());
    }
}


/**
    Replace all occurances of `what' in `s' with `with'.

    @param s Output. The String to act on.
    @param what The String to replace.
    @param with The replacement.

    @author Stefan Buehler */
void replace_all(String& s, const String& what, const String& with)
{
  Index j = s.find(what);
  while ( j != s.npos )
    {
      s.replace(j,1,with);
      j = s.find(what,j+with.size());
    }
}


/**
  Checks if there is exactly one newline character
  at the end of the string.

  @param s The String to check.

  @return Error code (0=ok, 1=empty, 2=missing, 3=extra newline

  @author Oliver Lemke
*/
int check_newline(const String& s)
{
  String d = s;
  int result = 0;

  // Remove all whitespaces except \n
  replace_all (d, " ", "");
  replace_all (d, "\t", "");
  replace_all (d, "\r", "");

  const char *cp = d.c_str ();
  while ((*cp == '\n') && *cp) cp++;

  if (!(*cp))
    result = 1;

  if (!result && d[d.length () - 1] != '\n')
    result = 2;
  else if (!result && d.length () > 2
           && d[d.length () - 1] == '\n' && d[d.length () - 2] == '\n')
      result = 3;

  return result;
}



/**
  Checks if the given file exists.

  @param filename File to check.

  @return Error code (true = file exists, false = file doesn't exist)

  @author Oliver Lemke
*/
bool file_exists(const String& filename)
{
    bool exists = false;
    fstream fin;
    fin.open(filename.c_str(), ios::in);
    if (fin.is_open())
    {
      exists=true;
    }
    fin.close();
    return exists;
}


/**
  Find the given file. If it doesn't exist in the current directory, also
  search the include path.

  @param filename File to check.
  @param extension Extension tried to be appended to the filename.

  @return Error code (true = file found, false = file not found)

  @author Oliver Lemke
*/
bool find_file(String& filename, const String extension, const ArrayOfString& paths)
{
  bool exists = true;

  // filename contains full path
  if (!paths.nelem() || (filename.nelem() && filename[0] == '/'))
  {
    if (!file_exists(filename))
    {
      // Full path + extension
      if (file_exists (filename + extension))
        filename += extension;
      else
        exists = false;
    }
  }
  // filename contains no or relative path
  else
  {
    ArrayOfString::const_iterator path;
    for (path = paths.begin(); path != paths.end(); path++)
    {
      String fullpath;
      fullpath = expand_path(*path) + "/" + filename;
      if (file_exists (fullpath))
      {
        filename = fullpath;
        break;
      }
      else if (file_exists (fullpath + extension))
      {
        filename = fullpath + extension;
        break;
      }
    }
    if (path == paths.end())
    {
      exists = false;
    }
  }

  return exists;
}


/*!
 Expands the ~ to home directory location in given path.
 
 \param[in] path  String with path
 
 \return Expanded path.
 
 \author Oliver Lemke              
 \date   2010-04-30
 */
String expand_path(const String& path)
{
  if ((path.nelem() == 1 && path[0] == '~')
      || (path.nelem() > 1 && path[0] == '~' && path[1] == '/'))
  {
    return String(getenv ("HOME")) + String(path, 1);
  }
  else
  {
    return path;
  }
}


/*!
 Adds base directory to the given path if it is a relative path.
 
 \param[in] path  String with path
 
 \return Expanded path.
 
 \author Oliver Lemke              
 \date   2012-05-01
 */
String add_basedir(const String& path)
{
  extern Parameters parameters;
  String expanded_path = expand_path(path);
  
  if (parameters.outdir.nelem() && path.nelem() && path[0] != '/')
  {
    expanded_path = parameters.outdir + '/' + expanded_path;
  }
  
  return expanded_path;
}


//! Return the parent directory of a path.
/** 
 Extracts the parent directory part of the given path.
 
 \param[out]  dirname  Parent directory of path
 \param[in]   path     Path
 
 \author Oliver Lemke
 */
void get_dirname(String& dirname, const String& path)
{
  dirname = "";
  if (!path.nelem()) return;
  
  ArrayOfString fileparts;
  path.split(fileparts, "/");
  if (path[0] == '/') dirname = "/";
  if (fileparts.nelem() > 1)
  {
    for(Index i = 0; i < fileparts.nelem()-1; i++)
    {
      dirname += fileparts[i];
      if (i < fileparts.nelem()-2) dirname += "/";
    }
  }
}


//! Return list of files in directory.
/**
 Returns list of all files in the given path.

 \param[out] files    List of files
 \param[in]  dirname  Parent directory of path

 \author Oliver Lemke
 */
void list_directory(ArrayOfString& files, String dirname)
{
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dirname.c_str())) == NULL)
    {
        ostringstream os;
        os << "Error(" << errno << ") opening " << dirname << endl;
        throw runtime_error(os.str());
    }

    while ((dirp = readdir(dp)) != NULL)
    {
        files.push_back(String(dirp->d_name));
    }

    closedir(dp);
}

////////////////////////////////////////////////////////////////////////////
//   IO manipulation classes for parsing nan and inf
////////////////////////////////////////////////////////////////////////////

double_istream&
double_istream::parse_on_fail (double& x, bool neg) {
    const char *exp[] = { "", "inf", "Inf", "nan", "NaN" };
    const char *e = exp[0];
    int l = 0;
    char inf[4];
    char *c = inf;
    if (neg) *c++ = '-';
    in.clear();
    if (!(in >> *c).good()) return *this;
    switch (*c) {
        case 'i': e = exp[l=1]; break;
        case 'I': e = exp[l=2]; break;
        case 'n': e = exp[l=3]; break;
        case 'N': e = exp[l=4]; break;
    }
    while (*c == *e) {
        if ((e-exp[l]) == 2) break;
        ++e; if (!(in >> *++c).good()) break;
    }
    if (in.good() && *c == *e) {
        switch (l) {
            case 1:
            case 2: x = std::numeric_limits<double>::infinity(); break;
            case 3:
            case 4: x = std::numeric_limits<double>::quiet_NaN(); break;
        }
        if (neg) x = -x;
        return *this;
    } else if (!in.good()) {
        if (!in.fail()) return *this;
        in.clear(); --c;
    }
    do { in.putback(*c); } while (c-- != inf);
    in.setstate(std::ios_base::failbit);
    return *this;
}


const double_imanip&
operator>>(std::istream& in, const double_imanip& dm) {
    dm.in = &in;
    return dm;
}
