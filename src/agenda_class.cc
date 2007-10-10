/* Copyright (C) 2002-2007
   Stefan Buehler <sbuehler@ltu.se>
   Oliver Lemke <olemke@core-dump.info>

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

/*!
  \file   agenda_class.cc
  \author Stefan Buehler <sbuehler@ltu.se>
  \date   Thu Mar 14 08:49:33 2002
  
  \brief  Implementation of agendas.
*/

#include "arts.h"
#include "agenda_class.h"
#include "agenda_record.h" // only for test functions
#include "methods.h"
#include "wsv_aux.h"
#include "messages.h"
#include "auto_wsv.h"
#include "workspace_ng.h"
#include <ostream>
#include <iterator>

//! Append a new method to end of list.
/*! 
  This is used by the parser to fill up the agenda.

  \param n New method to add.
*/
void Agenda::push_back(MRecord n)
{
  mml.push_back(n);
}

/** Print the error message and exit. */
void give_up(const String& message)
{
  out0 << message << '\n';
  arts_exit();
}

//! Execute an agenda.
/*! 
  This executes the methods specified in tasklist on the given
  workspace. It also checks for errors during the method execution and
  stops the program if an error has occured.

  The workspace itself is made visible by an external declaration.
*/
void Agenda::execute(bool silent) const
{
  // The workspace:
  extern Workspace workspace;

  // The method description lookup table:
  extern const Array<MdRecord> md_data;

  // The workspace variable lookup table:
  extern const Array<WsvRecord> wsv_data;
  
  // The array holding the pointers to the getaway functions:
  extern const void (*getaways[])(Workspace&, const MRecord&);

    // The messages level. We will manipulate it in this function, if
  // silent execution is desired.
  extern Messages messages;

  // Backup for the original message level:
  Messages messages_original( messages );

  // Manipulate the message level, to allow silent execution:
  if (silent)
    {
      // Level 4 means that the output should remain visible even for
      // silent execution.  
      if ( messages.screen < 4 )
        messages.screen = 0;
      if ( messages.file < 4 )
        messages.file   = 0;
    }

  out1 << "Executing " << name() << "\n"
       << "{\n";

//   for (Index i=0; i<mml.nelem(); ++i)
//     {
//       const MRecord&  mrr = mml[i];
//       cout << "id, input: " << mrr.Id() << ", ";
//       print_vector(mrr.Input());
//       cout << "id, output: " << mrr.Id() << ", ";
//       print_vector(mrr.Output());
//     }

  for (Index i=0; i<mml.nelem(); ++i)
    {
      // Runtime method data for this method:
      const MRecord&  mrr = mml[i];
      // Method data for this method:
      const MdRecord& mdd = md_data[mrr.Id()];
      
      try
        {
          out1 << "- " << mdd.Name() << '\n';
        
          { // Check if all specific input variables are initialized:
            const ArrayOfIndex& v(mdd.Input());
            for (Index s=0; s<v.nelem(); ++s)
              if (!workspace.is_initialized(v[s]))
                give_up("Method "+mdd.Name()+" needs input variable: "+
                        wsv_data[v[s]].Name());
          }

          { // Check if all generic input variables are initialized:
            const ArrayOfIndex& v(mrr.Input());
            //      cout << "v.nelem(): " << v.nelem() << endl;
            for (Index s=0; s<v.nelem(); ++s)
              if (!workspace.is_initialized(v[s]))
                give_up("Generic Method "+mdd.Name()+" needs input variable: "+
                        wsv_data[v[s]].Name());
          }

          // Call the getaway function:
          getaways[mrr.Id()]( workspace, mrr );

        }
      catch (runtime_error x)
        {
          ostringstream os;
          os << "Run-time error in method: " << mdd.Name() << '\n'
               << x.what();

          // We have to restore the original message level before
          // throwing the exception, otherwise no output will be
          // visible in case the exception is caught higher up and
          // execution continues.
          if (silent)
            {
              messages = messages_original;
            }

          throw runtime_error(os.str());
        }
    }

  out1 << "}\n";

  // Restore the original message level:
  if (silent)
    {
      messages = messages_original;
    }
}

//! Retrieve indexes of all input and output WSVs
/*!
  Builds arrays of WSM output variables which need to be
  duplicated or pushed on the WSV stack before the agenda
  is executed.
*/
void Agenda::set_outputs_to_push_and_dup ()
{
  extern const Array<MdRecord>  md_data;

  set<Index> inputs;
  set<Index> outputs;
  set<Index> outs2push;
  set<Index> outs2dup;

  for (Array<MRecord>::const_iterator method = mml.begin ();
       method != mml.end (); method++)
    {
      // Collect output WSVs
      const ArrayOfIndex& outs  = md_data[method->Id()].Output ();
      const ArrayOfIndex& gouts = method->Output ();

      // Put the outputs into a new set to sort them. Otherwise
      // set_intersection and set_difference screw up.
      set<Index> souts;
      souts.insert ( outs.begin (), outs.end ());
      souts.insert ( gouts.begin (), gouts.end ());

      // Add all outputs of this WSM to global list of outputs
      outputs.insert (souts.begin (), souts.end ());

      // Collect generic input WSVs
      const ArrayOfIndex& gins = method->Input ();
      inputs.insert (gins.begin (), gins.end ());

      // Collect input WSVs
      const ArrayOfIndex& ins = md_data[method->Id()].Input();
      inputs.insert (ins.begin (), ins.end ());

      // Find out all output WSVs of current WSM which were
      // already used as input. We have to place a copy of them on
      // the WSV stack.
      set_intersection (souts.begin (), souts.end (),
                        inputs.begin (), inputs.end (),
                        insert_iterator< set<Index> >(outs2dup,
                                                      outs2dup.begin ()));

    }

  // Find all outputs which are not in the list of WSVs to duplicate
  set_difference (outputs.begin (), outputs.end (),
                  outs2dup.begin (), outs2dup.end (),
                  insert_iterator< set<Index> >(outs2push,
                                                outs2push.begin ()));

  extern map<String, Index> AgendaMap;
  extern const Array<AgRecord> agenda_data;

  const AgRecord& agr = agenda_data[AgendaMap.find (name ())->second];
  const ArrayOfIndex& aout = agr.Output ();
  const ArrayOfIndex& ain = agr.Input ();

  // We have to build a new set of agenda input and output because the
  // set_difference function only works properly on sorted input.
  set<Index> saout;
  set<Index> sain;

  saout.insert ( aout.begin (), aout.end ());
  sain.insert ( ain.begin (), ain.end ());

  moutput_push.clear ();
  moutput_dup.clear ();

  // Remove the WSVs which are agenda input from the list of
  // output variables for which we have to create an new
  // entry on the stack. This is already done for agenda inputs.
  set<Index> outs2push_without_agenda_input;
  set_difference (outs2push.begin (), outs2push.end (),
                  sain.begin (), sain.end (),
                  insert_iterator< set<Index> >(outs2push_without_agenda_input,
                                                outs2push_without_agenda_input.begin ()));

  // Same for agenda output variables.
  set_difference (outs2push_without_agenda_input.begin (),
                  outs2push_without_agenda_input.end (),
                  saout.begin (), saout.end (),
                  insert_iterator<ArrayOfIndex>(moutput_push,
                                                moutput_push.begin ()));

  // Remove the WSVs which are agenda input from the list of
  // output variables for which we have to create a duplicate
  // on the stack. This is already done for agenda inputs.
  set<Index> outs2dup_without_agenda_input;
  set_difference (outs2dup.begin (), outs2dup.end (),
                  sain.begin (), sain.end (),
                  insert_iterator< set<Index> >(outs2dup_without_agenda_input,
                                                outs2dup_without_agenda_input.begin ()));

  // Same for agenda output variables.
  set_difference (outs2dup_without_agenda_input.begin (),
                  outs2dup_without_agenda_input.end (),
                  saout.begin (), saout.end (),
                  insert_iterator<ArrayOfIndex>(moutput_dup,
                                                moutput_dup.begin ()));

  // Special case: Variables which are defined in the agenda only
  // as output but are used first as input in one of the WSMs
  // For those the current WSV value must be copied to the agenda
  // input variable
  set<Index> saout_only;

  set_difference (saout.begin (), saout.end (),
                  sain.begin (), sain.end (),
                  insert_iterator< set<Index> >(saout_only,
                                                saout_only.begin ()));

  ArrayOfIndex agenda_only_out_wsm_in;
  set_intersection (outs2dup.begin (), outs2dup.end (),
                    saout_only.begin (), saout_only.end (),
                    insert_iterator<ArrayOfIndex>(agenda_only_out_wsm_in,
                                                  agenda_only_out_wsm_in.begin ()));

  // Special case: Variables which are defined in the agenda only
  // as input but are used as output in one of the WSMs
  // For those the current WSV value must be copied to the agenda
  // input variable
  set<Index> sain_only;

  set_difference (sain.begin (), sain.end (),
                  saout.begin (), saout.end (),
                  insert_iterator< set<Index> >(sain_only,
                                                sain_only.begin ()));

  ArrayOfIndex agenda_only_in_wsm_out;
  set_intersection (outs2push.begin (), outs2push.end (),
                    sain_only.begin (), sain_only.end (),
                    insert_iterator<ArrayOfIndex>(agenda_only_in_wsm_out,
                                                  agenda_only_in_wsm_out.begin ()));

  out3 << "  [Agenda::pushpop]                 : " << name() << "\n";
  out3 << "  [Agenda::pushpop] - # Funcs in Ag : " << mml.nelem () << "\n";
  out3 << "  [Agenda::pushpop] - AgOut         : ";
  PrintWsvNames (out3, aout);
  out3 << "\n";
  out3 << "  [Agenda::pushpop] - AgIn          : ";
  PrintWsvNames (out3, ain);
  out3 << "\n";
  out3 << "  [Agenda::pushpop] - All WSM output: ";
  PrintWsvNames (out3, outputs);
  out3 << "\n";
  out3 << "  [Agenda::pushpop] - WSVs push     : ";
  PrintWsvNames (out3, moutput_push);
  out3 << "\n";
  out3 << "  [Agenda::pushpop] - WSVs dup      : ";
  PrintWsvNames (out3, moutput_dup);
  out3 << "\n";
  out3 << "  [Agenda::pushpop] - Ag inp dup    : ";
  PrintWsvNames (out3, agenda_only_in_wsm_out);
  out3 << "\n";
  out3 << "  [Agenda::pushpop] - Ag out dup    : ";
  PrintWsvNames (out3, agenda_only_out_wsm_in);
  out3 << "\n";

  if (agenda_only_in_wsm_out.nelem ())
    {
      ostringstream err;
      err << "At least one variable is only defined as input\n"
        << "in agenda " << name () << ", but\n"
        << "used as output in a WSM called by the agenda!!!\n"
        << "This is not allowed.\n"
        << "Variable(s): ";
      PrintWsvNames (err, agenda_only_in_wsm_out);
      throw runtime_error (err.str ());
    }
}

//! Check if given variable is agenda input.
/*! 
  A variable is agenda input if it is an input variable to any of the
  methods making up the agenda. 

  \param var The workspace variable to check.

  \return True if var is an input variable of this agenda.
*/
bool Agenda::is_input(Index var) const
{
  // Make global method data visible:
  extern const Array<MdRecord>  md_data;
  extern Workspace workspace;
  extern const ArrayOfString wsv_group_names;

  // Make sure that var is the index of a valid method:
  assert( 0<=var );
  assert( var<md_data.nelem() );

  // Determine the index of WsvGroup Agenda
  Index WsvAgendaGroupIndex = 0;
  for (Index i = 0; !WsvAgendaGroupIndex && i < wsv_group_names.nelem (); i++)
    {
      if (wsv_group_names[i] == "Agenda")
        WsvAgendaGroupIndex = i;
    }

  // Loop all methods in this agenda:
  for ( Index i=0; i<nelem(); ++i )
    {
      // Get a handle on this methods runtime data record:
      const MRecord& this_method = mml[i];
      
      // Is var a specific input?
      {
        // Get a handle on the Input list for the current method:
        const ArrayOfIndex& input = md_data[this_method.Id()].Input();

        for ( Index j=0; j<input.nelem(); ++j )
          {
            if ( var == input[j] ) return true;
          }
      }

      // Is var a generic input?
      {
        // Get a handle on the Input list:
        const ArrayOfIndex& input = this_method.Input();

        for ( Index j=0; j<input.nelem(); ++j )
          {
            if ( var == input[j] ) return true;
          }
      }

      // If a General Input variable of this method (e.g. AgendaExecute)
      // is of type Agenda, check its input recursively for matches
      for ( Index j = 0; j < md_data[this_method.Id ()].GInput().nelem(); j++)
        {
          if (md_data[this_method.Id ()].GInput()[j] == WsvAgendaGroupIndex)
            {
              Agenda *AgendaFromGeneralInput =
                (Agenda *)workspace[this_method.Input ()[j]];

              if ((*AgendaFromGeneralInput).is_input(var))
                {
                  return true;
                }
            }
        }
    }

  // Ok, that means var is no input at all.
  return false;
}

//! Check if given variable is agenda output.
/*! 
  A variable is agenda output if it is an output variable to any of the
  methods making up the agenda. 

  \param var The workspace variable to check.

  \return True if var is an output variable of this agenda.
*/
bool Agenda::is_output(Index var) const
{
  // Loop all methods in this agenda:
  for ( Index i=0; i<nelem(); ++i )
    {
      // Get a handle on this methods runtime data record:
      const MRecord& this_method = mml[i];
      
      // Is var a specific output?
      {
        // Make global method data visible:
        extern const Array<MdRecord>  md_data;

        // Get a handle on the Output list for the current method:
        const ArrayOfIndex& output = md_data[this_method.Id()].Output();

        for ( Index j=0; j<output.nelem(); ++j )
          {
            if ( var == output[j] ) return true;
          }
      }

      // Is var a generic output?
      {
        // Get a handle on the Output list:
        const ArrayOfIndex& output = this_method.Output();

        for ( Index j=0; j<output.nelem(); ++j )
          {
            if ( var == output[j] ) return true;
          }
      }
    }

  // Ok, that means var is no output at all.
  return false;
}

//! Set agenda name.
/*! 
  This sets the private member mname to the given string. 

  \param nname The name for the agenda.
*/
void Agenda::set_name(const String& nname)
{
  mname = nname;
}

//! Agenda name.
/*! 
  Returns the private member mname.

  \return The name of this agenda.
*/
String Agenda::name() const
{
  return mname;
}

//! Print an agenda.
/*!
  This prints an agenda, by printing the individual methods, just as
  they would appear in the controlfile.

  \param os     Output stream.
  \param indent How many characters of indentation.

  \author Stefan Buehler
  \date   2002-12-02
*/
void Agenda::print( ostream& os,
                    const String& indent ) const
{
  for ( Index i=0; i<mml.nelem(); ++i )
    {
      // Print member methods with 3 characters more indentation:
      mml[i].print(os,indent);
    }
}

//! Output operator for Agenda.
/*! 
  This is useful for debugging.
  
  \param os Output stream.
  \param a The Agenda to write.

  \return Output stream.

  \author Stefan Buehler
  \date   2002-12-02
*/
ostream& operator<<(ostream& os, const Agenda& a)
{
  // Print agenda as it would apear in a controlfile.
  a.print(os, "");
  return os;
}

//--------------------------------
//     Functions for MRecord:
//--------------------------------

//! Print an MRecord.
/*!
  Since the MRecord contains all runtime information for one method,
  the best way to print it is exactly as it would appear in the
  controlfile. 

  This has to work in a recursive way, since the method can be an
  agenda method, which includes other methods, which can be agenda
  methods, ...

  Therefore, the indentation is increased more and more for recursive
  calls. 

  At the moment, this is used just for debugging.

  \param os     Output stream.
  \param indent How many characters of indentation.

  \author Stefan Buehler
  \date   2002-12-02
*/
void MRecord::print( ostream& os,
                     const String& indent ) const
{
  extern const Array<WsvRecord> wsv_data;
  extern const Array<MdRecord>  md_data;

  // Get a handle on the right record:
  const MdRecord tmd = md_data[Id()];

  os << indent << tmd.Name();

  // Is this a generic method? -- Then we need round braces.
  if ( 0 != tmd.GOutput().nelem()+tmd.GInput().nelem() )
    {
      // First entry needs no leading comma:
      bool first=true;

      os << '(';

      for (Index i=0; i<Output().nelem(); ++i)
        {
          if (first) first=false;
          else os << ",";

          os << wsv_data[Output()[i]];
        }

      for (Index i=0; i<Input().nelem(); ++i)
        {
          if (first) first=false;
          else os << ",";

          os << wsv_data[Input()[i]];
        }

      os << ')';
    }

  os << "{\n";

  // Is this an agenda method?
  if ( 0 != Tasks().nelem() )
    {
      // Assert that the keyword list really is empty:
      assert ( 0 == tmd.Keywords().nelem() );

      Tasks().print(os,indent+"   ");
    }
  else
    {
      // We must have a plain method.

      // Print the keywords:

      // Determine the length of the longest keyword:
      Index maxsize = 0;
      for (Index i=0; i<tmd.Keywords().nelem(); ++i)
        if ( tmd.Keywords()[i].nelem() > maxsize )
          maxsize = tmd.Keywords()[i].nelem();

      // The number of actual parameters must match the number of
      // keywords: 
      assert( tmd.Keywords().nelem() == Values().nelem() );

      for (Index i=0; i<tmd.Keywords().nelem(); ++i)
        {
          os << indent << "   " << setw(maxsize)
             << tmd.Keywords()[i] << " = "
             << Values()[i] << "\n";
        }
    }

  os << indent << "}";
}

//! Output operator for MRecord.
/*! 
  This is useful for debugging.
  
  \param os Output stream.
  \param a The method runtime data record to write.

  \return Output stream.

  \author Stefan Buehler
  \date   2002-12-02
*/
ostream& operator<<(ostream& os, const MRecord& a)
{
  a.print(os,"");
  return os;
}

