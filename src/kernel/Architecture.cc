/*-
 * Copyright (C) 2010-2012, Centre National de la Recherche Scientifique,
 *                          Institut Polytechnique de Bordeaux,
 *                          Universite Bordeaux 1.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "Architecture.hh"

#include <cassert>

#include <kernel/Architecture_ARM.hh>
#include <kernel/Architecture_X86_32.hh>

using namespace std;

RegisterDesc::RegisterDesc (int index, const std::string &label, int regsize)
{
  new(this) RegisterDesc(index, label, regsize, 0, regsize);
}

RegisterDesc::RegisterDesc (int index, const std::string &label, int regsize, 
			    int winoffset,  int winsize)
{
  this->index = index;
  this->label = label;
  this->register_size = regsize;
  this->window_offset = winoffset;
  this->window_size = winsize;

  assert (0 <= window_offset+ window_size - 1 &&
	  window_offset+ window_size - 1 < regsize);
}

void
RegisterDesc::output_text(ostream &os) const
{
  os << this->label << "{" << this->window_offset 
     << ";" << this->window_size << "}";
}

int 
RegisterDesc::get_index () const 
{
  return index;
}

int 
RegisterDesc::get_register_size () const 
{
  return register_size;
}

int 
RegisterDesc::get_window_offset () const
{
  return window_offset;
}

int 
RegisterDesc::get_window_size () const 
{
  return window_size;
}

const std::string &
RegisterDesc::get_label () const
{
  return label;
}

int 
RegisterDesc::hashcode () const 
{
  return (3 * get_index () +
	  5 * get_register_size () + 
	  7 * get_window_size () + 13 * get_window_offset () + 
	  19 * std::tr1::hash<std::string>() (label));
}

int 
RegisterDesc::is_alias () const
{
  return window_offset > 0 || window_size != register_size;
}

void
Architecture::add_register(const std::string &id, int regsize)
{  
  assert (registerspecs->find(id) == registerspecs->end());

  int index = registerspecs->size ();
  (*registerspecs)[id] = new RegisterDesc (index, id, regsize);
}

void
Architecture::add_register_alias (const std::string &name, 
				  const std::string &refname,
				  int size, int offset)
{  
  assert (registerspecs->find(name) == registerspecs->end());
  assert (registerspecs->find(refname) != registerspecs->end());
  RegisterDesc *reg = (*registerspecs)[refname];
  
  (*registerspecs)[name] = 
    new RegisterDesc (reg->get_index (), refname, reg->get_register_size (), 
		      offset, size);
}

bool 
Architecture::has_register(const std::string &label) const
{
  return (registerspecs->find(label) != registerspecs->end());
}

const RegisterDesc *
Architecture::get_register(const string &label) const 
{
  if (registerspecs->find(label) == registerspecs->end())
    throw RegisterDescNotFound();

  return (*registerspecs)[label];
}

const RegisterSpecs *
Architecture::get_registers() const
{
  return registerspecs;
}

Architecture::Architecture()
{
  word_size = sizeof (word_t);
  address_range = sizeof(address_t);
  registerspecs = new RegisterSpecs ();
}

Architecture::~Architecture()
{
  for (RegisterSpecs::iterator iter = registerspecs->begin();
       iter != registerspecs->end(); iter++)
    {
      delete iter->second;
    }

  delete registerspecs;
}

Architecture **Architecture::architectures = NULL;

void 
Architecture::init ()
{ 
  int nb_architectures = (int) Unknown * (int) UnknownEndian;
  architectures = new Architecture *[nb_architectures];
  for (int i = 0; i < nb_architectures; i++)
    architectures[i] = NULL;
}

void 
Architecture::terminate ()
{
  int nb_architectures = (int) Unknown * (int) UnknownEndian;
  for (int i = 0; i < nb_architectures; i++)
    delete architectures[i];

  delete[] architectures;
}

static int 
s_arch_index (Architecture::processor_t proc, Architecture::endianness_t endian)
{
  return proc * Architecture::UnknownEndian + endian;
}

const Architecture * 
Architecture::getArchitecture (const processor_t proc, 
			       const endianness_t endian)
{
  int index = s_arch_index (proc, endian);
  Architecture *arch = architectures [index];

  if (arch == NULL)
    {
      switch (proc)
	{
	case Architecture::ARM:
	  arch = new Architecture_ARM(endian);
	  break;
	  
	case Architecture::X86_32:
	  if (endian == Architecture::LittleEndian)
	    {
	      arch = new Architecture_X86_32();
	      break;
	    }
	  
	default:
	  throw UnsupportedArch();
	}
      architectures [index] = arch;
    }

  return arch;
}

const Architecture * 
Architecture::getArchitecture (const processor_t proc)
{
  const Architecture *arch;
  
  switch (proc)
    {
    case Architecture::X86_32:
      arch = getArchitecture (proc, LittleEndian);
      break;

    default:
      throw UnsupportedArch();
    }

  return arch;
}
