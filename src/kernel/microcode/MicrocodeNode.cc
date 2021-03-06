/*-
 * Copyright (C) 2010-2014, Centre National de la Recherche Scientifique,
 *                          Institut Polytechnique de Bordeaux,
 *                          Universite de Bordeaux.
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
#include <kernel/annotations/SolvedJmpAnnotation.hh>
#include <kernel/microcode/MicrocodeNode.hh>

#include <stdio.h>
#include <cassert>
#include <sstream>
#include <list>
#include <set>

using namespace std;

vector<Expr **> * MicrocodeNode::expr_list()
{
  vector<Expr **> * exprs = new vector<Expr **>;
  MicrocodeNode_iterate_successors(*this, succ)
  {
    exprs->push_back(&((*succ)->condition));

    if ((*succ)->is_dynamic())
      exprs->push_back(&(((DynamicArrow *)(*succ))->target));

    vector<Expr **> * stmt_exprs = (*succ)->get_stmt()->expr_list();
    for (vector<Expr **>::iterator e = stmt_exprs->begin(); e != stmt_exprs->end(); e++)
      exprs->push_back(*e);
    delete stmt_exprs;
  }

  return exprs;
}

/**********************************************************************/
/* MicrocodeNode                                                      */
/**********************************************************************/
MicrocodeNode::MicrocodeNode(MicrocodeAddress loc) :
  Annotable(),
  loc(loc),
  predecessors(NULL)
{
  successors = new vector<StmtArrow *>;
}

MicrocodeNode::MicrocodeNode(MicrocodeAddress loc,
			     vector<StmtArrow *> * successors) :
  Annotable(),
  loc(loc),
  successors(successors),
  predecessors(NULL)
{}

MicrocodeNode::MicrocodeNode(MicrocodeAddress loc,
			     StmtArrow *unique_succ) :
  Annotable(),
  loc(loc),
  predecessors(NULL)
{
  successors = new vector<StmtArrow *>;
  successors->push_back(unique_succ);
}

MicrocodeNode::MicrocodeNode(MicrocodeAddress loc,
			     StmtArrow *succ1,
			     StmtArrow *succ2) :
  Annotable(),
  loc(loc),
  predecessors(NULL)
{
  successors = new vector<StmtArrow *>;
  successors->push_back(succ1);
  successors->push_back(succ2);
}

MicrocodeNode::MicrocodeNode(const MicrocodeNode &snode) :
  Annotable(snode),
  predecessors(NULL)
{
  loc = snode.loc;
  successors = new vector<StmtArrow *>;
  MicrocodeNode_iterate_successors(snode, arr)
  successors->push_back((*arr)->clone());
}

MicrocodeNode * MicrocodeNode::clone() const
{
  MicrocodeNode *res = new MicrocodeNode(*this);
  return res;
}

MicrocodeNode::~MicrocodeNode()
{
  MicrocodeNode_iterate_successors(*this, arr)
    delete *arr;
  delete predecessors;
  delete successors;
}

void
MicrocodeNode::add_predecessor (StmtArrow * arr)
{
  if (predecessors == NULL)
    predecessors = new std::vector<StmtArrow *> ();
  for (int k = 0; k < (int) predecessors->size (); k++)
    if (*((*predecessors)[k]) == (*arr))
      return;
  predecessors->push_back(arr);
}

const MicrocodeAddress &MicrocodeNode::get_loc() const {
  return loc;
}
std::vector<StmtArrow *> * MicrocodeNode::get_successors() const { return successors; }
std::vector<StmtArrow *> * MicrocodeNode::get_predecessors() const { return predecessors; }

std::vector<MicrocodeNode *>
MicrocodeNode::get_global_parents () const
{
  set<const MicrocodeNode *> done;
  list<const MicrocodeNode *> todo;
  vector<MicrocodeNode *> result;

  todo.push_back (this);
  done.insert (this);

  while (!todo.empty ())
    {
      const MicrocodeNode *n = todo.front ();
      todo.pop_front ();
      vector<StmtArrow *> *preds = n->get_predecessors ();
      if (preds == NULL)
	continue;

      for (vector<StmtArrow *>::const_iterator i = preds->begin ();
	   i != preds->end (); i++)
	{
	  MicrocodeNode *src = (*i)->get_src ();
	  if (src->get_loc ().getGlobal () == loc.getGlobal ())
	    {
	      if (src->get_loc ().getLocal () == 0)
		result.push_back (src);
	      if (done.find (src) == done.end ())
		{
		  done.insert (src);
		  todo.push_back (src);
		}
	    }
	}
    }

  return result;
}

static void
s_stmtarrow_to_string(std::ostream &out, const StmtArrow* stmtarrow)
{
  /* Condition */
  Expr * condition = stmtarrow->get_condition ();

  if (condition)
    {
      if (condition->is_Constant())
	{
	  Constant *c = (Constant *) condition;
	  if (c->get_val() == 0)
	    out << "<< False >> ";
	}
      else
	{
	  out << "<< "<<  *condition << " >> ";
	}
    }

  /* Statement */
  if (stmtarrow->is_static ())
    out << stmtarrow->get_stmt ()->pp();
  else
    out << "Jmp";

  /* Arrow */
  out << " --> ";

  /* Target location */
  if (stmtarrow->is_static ())
    {
      /* StaticArrow */
      out << ((StaticArrow *) stmtarrow)->get_target ();
    }
  else
    {
      /* DynamicArrow */
      ((DynamicArrow *) stmtarrow)->get_target ()->output_text (out);
    }
}

string MicrocodeNode::pp() const
{
  ostringstream oss;

  oss << "[0x" << hex << loc.getGlobal()
      << ","   << dec << loc.getLocal() << "]";

  /* Annotation */
  if (is_annotated ())
    {
      oss << " @";
      output_annotations (oss);
      oss << "@";
    }

  MicrocodeNode_iterate_successors(*this, succ)
    {
      oss << " ";
      s_stmtarrow_to_string(oss, (*succ));
      oss << ";";
    }

  return oss.str();
}

StmtArrow *
MicrocodeNode::add_successor(Expr *condition, Expr *target, Statement *st)
{
  StmtArrow *arr = new DynamicArrow(this, target, st, 0, condition);
  successors->push_back(arr);
  return arr;
}

StmtArrow *
MicrocodeNode::add_successor(Expr *condition, MicrocodeNode *tgt, Statement *st)
{
  StmtArrow *arr = new StaticArrow(this, tgt, st, 0, condition);
  successors->push_back(arr);
  tgt->add_predecessor (arr);

  return arr;
}

bool MicrocodeNode::operator==(const MicrocodeNode &o) const
{
  return loc.equals(o.loc);
}

bool MicrocodeNode::operator<(const MicrocodeNode &e) const
{
  return this->get_loc().lessThan(e.get_loc());
}

/**********************************************************************/

StmtArrow::StmtArrow(MicrocodeNode *src, Statement *stmt,
		     const AnnotationMap *annotations, Expr *condition) :
  Annotable(annotations),
  src(src),
  stmt(stmt),
  condition(condition)
{
}

StmtArrow::StmtArrow(const StmtArrow &arr): Annotable(arr)
{
  stmt = arr.stmt->clone();
  condition = NULL;
  if (arr.condition)
    condition = arr.condition->ref ();
}

StmtArrow::~StmtArrow()
{
  /* Do not forget that origin object must be killed by its owner
   * which is not this arrow */
  delete stmt;
  if (condition)
    condition->deref ();
}

void StmtArrow::set_src(MicrocodeNode * n) { src = n; }

MicrocodeAddress StmtArrow::get_origin() const
{
  return get_src ()->get_loc ();
}

MicrocodeNode * StmtArrow::get_src() const
{
  return src;
}

Statement * StmtArrow::get_stmt() const
{
  return stmt;
}

Expr * StmtArrow::get_condition() const
{
  return condition;
}

bool StmtArrow::is_dynamic() const
{
  StmtArrow *noconst_this = const_cast<StmtArrow *>(this);
  return dynamic_cast<DynamicArrow *>(noconst_this);
}

bool StmtArrow::is_static() const
{
  StmtArrow *noconst_this = const_cast<StmtArrow *>(this);
  return dynamic_cast<StaticArrow *>(noconst_this);
}

bool StmtArrow::operator==(const StmtArrow &other)
{
  if (!(get_src () == other.get_src ())) return false;
  if (!(condition == other.condition)) return false;
  if (! Statement::equal(stmt, other.stmt)) return false;

  if (this->is_dynamic() && other.is_dynamic())
    return (((DynamicArrow *) this)->get_target() ==
	    ((DynamicArrow *) &other)->get_target());

  if ((!this->is_dynamic()) && !other.is_dynamic())
    return ((StaticArrow *) this)->get_concrete_target().equals(
		((StaticArrow *) this)->get_concrete_target());
  return false;
}

DynamicArrow::~DynamicArrow() {
  if (target)
    target->deref ();
}

static void
s_dynamicarrow_to_string(std::ostream &out, const MicrocodeAddress &origin,
			 const Expr *condition, Statement *stmt)
{
  out << origin << " ";
  if (condition)
    {
      if (condition->is_Constant ())
	{
	  Constant *c = (Constant *) condition;
	  if (c->get_val() == 0)
	    out << "<< False >>";
	}
      else
	{
	  out << "<< " <<  *condition << " >>";
	}
    }

  out << " --> " << stmt->pp ();
}

string DynamicArrow::pp () const
{
  ostringstream oss;

  oss << " ";
  s_dynamicarrow_to_string(oss, get_origin (), condition, stmt);
  oss << target->to_string ();

  /* Annotation */
  if (is_annotated ())
    {
      oss << " @";
      output_annotations (oss);
      oss << "@";
    }

  return oss.str();
}

StaticArrow::StaticArrow(MicrocodeNode * src, MicrocodeNode * tgt,
			 Statement *stmt, const AnnotationMap *annotations,
			 Expr *condition) :
  StmtArrow(src, stmt, annotations, condition),
  tgt(tgt)
{
  target = tgt->get_loc();
}


StaticArrow::StaticArrow(const StaticArrow &other) :
  StmtArrow(other),
  target(other.target)
{}

StaticArrow::~StaticArrow() {}

StmtArrow * StaticArrow::clone() {
  return new StaticArrow(*this);
}

Option<MicrocodeAddress> StaticArrow::extract_target() const
{
  return Option<MicrocodeAddress>(target);
}

MicrocodeAddress StaticArrow::get_target() const
{
  return target;
}

MicrocodeAddress StaticArrow::get_concrete_target() const
{
  return target;
}

void StaticArrow::set_concrete_target(MicrocodeAddress nvo)
{
  target = nvo;
}

void StaticArrow::set_tgt(MicrocodeNode * n) {
	tgt = n;
}

static void
s_staticarrow_to_string(std::ostream &out, const MicrocodeAddress &origin,
			const Expr *condition, Statement *stmt)
{
  out << origin << " ";
  if (condition)
    {
      if (condition->is_Constant ())
	{
	  Constant *c = (Constant *) condition;
	  if (c->get_val() == 0)
	    out << "<< False >> ";
	}
      else
	{
	  out << "<< "<<  *condition << " >> ";
	}
    }

  out << stmt->pp () << " --> ";
}

string StaticArrow::pp() const
{
  ostringstream oss;
  s_staticarrow_to_string(oss, get_origin (), condition, stmt);
  oss << target.to_string ();

  return oss.str ();
}

DynamicArrow::DynamicArrow(MicrocodeNode *src,
			   Expr *target, Statement *stmt,
			   const AnnotationMap *annotations, Expr *condition) :
  StmtArrow(src, stmt, annotations, condition),
  target(target)
{
}

DynamicArrow::DynamicArrow(const DynamicArrow &other) :
  StmtArrow(other),
  target(other.target->ref ())
{}

StmtArrow * DynamicArrow::clone()
{
  return new DynamicArrow(*this);
}

Expr * DynamicArrow::get_target() const
{
  return target;
}

Option<MicrocodeAddress> DynamicArrow::extract_target() const
{
  if (target->is_Constant())
    return Option<MicrocodeAddress>(MicrocodeAddress(((Constant *) target)->get_val()));
  else
    return Option<MicrocodeAddress>();
}

void
DynamicArrow::add_solved_jump (MicrocodeAddress tgt)
{
  SolvedJmpAnnotation *sja;

  if (! has_annotation (SolvedJmpAnnotation::ID))
    {
      sja = new SolvedJmpAnnotation ();
      add_annotation (SolvedJmpAnnotation::ID, sja);
    }
  else
    {
      sja = (SolvedJmpAnnotation *) get_annotation (SolvedJmpAnnotation::ID);
    }

  bool has_it = false;
  for (SolvedJmpAnnotation::const_iterator j = sja->begin ();
       j != sja->end () && ! has_it; j++)
    has_it = (j->equals (tgt));

  if (! has_it)
    sja->add (tgt);
}

