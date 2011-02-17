/****************************************************************************
 * Copyright (C) 2009-2011 GGA Software Services LLC
 * 
 * This file is part of Indigo toolkit.
 * 
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 3 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 * 
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ***************************************************************************/

#include <time.h>

#include "base_cpp/output.h"
#include "molecule/molecule.h"
#include "molecule/molfile_saver.h"
#include "graph/graph_highlighting.h"
#include "molecule/molecule_stereocenters.h"
#include "molecule/query_molecule.h"
#include "molecule/elements.h"

using namespace indigo;

MolfileSaver::MolfileSaver (Output &output) : highlighting(0),
reactionAtomMapping(0),
reactionAtomInversion(0),
reactionAtomExactChange(0),
reactionBondReactingCenter(0),
 _output(output),
TL_CP_GET(_atom_mapping),
TL_CP_GET(_bond_mapping)
{
   mode = MODE_AUTO;
   no_chiral = false;
}

void MolfileSaver::saveBaseMolecule (BaseMolecule &mol)
{
   _saveMolecule(mol, mol.isQueryMolecule());
}

void MolfileSaver::saveMolecule (Molecule &mol)
{
   _saveMolecule(mol, false);
}

void MolfileSaver::saveQueryMolecule (QueryMolecule &mol)
{
   _saveMolecule(mol, true);
}

void MolfileSaver::_saveMolecule (BaseMolecule &mol, bool query)
{
   QueryMolecule *qmol = 0;

   if (query)
      qmol = (QueryMolecule *)(&mol);

   if (mode == MODE_2000)
      _v2000 = true;
   else if (mode == MODE_3000)
      _v2000 = false;
   else
   {
      // auto-detect the format: save to v3000 molfile only
      // if v2000 is not enough
      _v2000 = true;

      if (highlighting != 0 && highlighting->numVertices() + highlighting->numVertices() > 0)
         _v2000 = false;
      if (!mol.stereocenters.haveAllAbsAny() && !mol.stereocenters.haveAllAndAny())
         _v2000 = false;
   }

   bool rg2000 = (_v2000 && qmol != 0 && qmol->rgroups.getRGroupCount() > 0);

   if (rg2000)
   {
      time_t tm = time(NULL);
      const struct tm *lt = localtime(&tm);
      _output.printfCR("$MDL  REV  1 %02d%02d%02d%02d%02d",
         lt->tm_mon + 1, lt->tm_mday, lt->tm_year % 100, lt->tm_hour, lt->tm_min);
      _output.writeStringCR("$MOL");
      _output.writeStringCR("$HDR");
   }
      
   _writeHeader(mol, _output, BaseMolecule::hasZCoord(mol));

   if (rg2000)
   {
      _output.writeStringCR("$END HDR");
      _output.writeStringCR("$CTAB");
   }

   if (_v2000)
   {
      _writeCtabHeader2000(_output, mol);
      _writeCtab2000(_output, mol, query);
   }
   else
   {
      _writeCtabHeader(_output);
      _writeCtab(_output, mol, query);
   }

   if (_v2000)
   {
      _writeRGroupIndices2000(_output, mol);
      _writeAttachmentValues2000(_output, mol);
   }
   
   if (rg2000)
   {
      int i, j;

      MoleculeRGroups &rgroups = qmol->rgroups;
      int n_rgroups = rgroups.getRGroupCount();

      for (i = 1; i <= n_rgroups; i++)
      {
         const RGroup &rgroup = rgroups.getRGroup(i);

         if (rgroup.fragments.size() == 0)
            continue;

         _output.printf("M  LOG  1 %3d %3d %3d  ", i, rgroup.if_then, rgroup.rest_h);

         QS_DEF(Array<char>, occ);
         ArrayOutput occ_out(occ);

         _writeOccurrenceRanges(occ_out, rgroup.occurrence);

         for (j = 0; j < 3 - occ.size(); j++)
            _output.writeChar(' ');

         _output.write(occ.ptr(), occ.size());
         _output.writeCR();
      }

      _output.writeStringCR("M  END");
      _output.writeStringCR("$END CTAB");

      for (i = 1; i <= n_rgroups; i++)
      {
         RGroup &rgroup = rgroups.getRGroup(i);

         if (rgroup.fragments.size() == 0)
            continue;

         _output.writeStringCR("$RGP");
         _output.printfCR("%4d", i);

         for (j = 0; j < rgroup.fragments.size(); j++)
         {
            QueryMolecule *fragment = rgroup.fragments[j];

            _output.writeStringCR("$CTAB");
            _writeCtabHeader2000(_output, *fragment);
            _writeCtab2000(_output, *fragment, true);
            _writeRGroupIndices2000(_output, *fragment);
            _writeAttachmentValues2000(_output, *fragment);

            _output.writeStringCR("M  END");
            _output.writeStringCR("$END CTAB");
         }
         _output.writeStringCR("$END RGP");
      }
      _output.writeStringCR("$END MOL");
   }
   else
      _output.writeStringCR("M  END");
}

void MolfileSaver::saveCtab3000 (Molecule &mol)
{
   _writeCtab(_output, mol, false);
}

void MolfileSaver::saveQueryCtab3000 (QueryMolecule &mol)
{
   _writeCtab(_output, mol, true);
}

void MolfileSaver::_writeHeader (BaseMolecule &mol, Output &output, bool zcoord)
{
   time_t tm = time(NULL);
   const struct tm *lt = localtime(&tm);

   const char *dim;

   if (zcoord)
      dim = "3D";
   else
      dim = "2D";

   if (mol.name.ptr() != 0)
      output.printfCR("%s", mol.name.ptr());
   else
      output.writeCR();
   output.printfCR("  -INDIGO-%02d%02d%02d%02d%02d%s", lt->tm_mon + 1, lt->tm_mday,
      lt->tm_year % 100, lt->tm_hour, lt->tm_min, dim);
   output.writeCR();
}

void MolfileSaver::_writeCtabHeader (Output &output)
{
   output.printfCR("%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d V3000",
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void MolfileSaver::_writeCtabHeader2000 (Output &output, BaseMolecule &mol)
{
   int chiral = 0;

   if (!no_chiral && mol.stereocenters.size() != 0 && mol.stereocenters.haveAllAbsAny())
      chiral = 1;

   output.printfCR("%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d V2000",
      mol.vertexCount(), mol.edgeCount(), 0, 0, chiral, 0, 0, 0, 0, 0, 999);
}

void MolfileSaver::_writeAtomLabel (Output &output, int label)
{
   output.writeString(Element::toString(label));
}

void MolfileSaver::_writeMultiString (Output &output, const char *string, int len)
{
   int limit = 70;
   while (1)
   {
      output.writeString("M  V30 ");

      if (len <= limit)
      {
         output.write(string, len);
         output.writeCR();
         break;
      }

      output.write(string, limit);
      output.writeStringCR("-");
      len -= limit;
      string += limit;
   }
}


void MolfileSaver::_writeCtab (Output &output, BaseMolecule &mol, bool query)
{
   QueryMolecule *qmol = 0;

   if (query)
      qmol = (QueryMolecule *)(&mol);

   output.writeStringCR("M  V30 BEGIN CTAB");
   output.printfCR("M  V30 COUNTS %d %d 0 0 0", mol.vertexCount(), mol.edgeCount());
   output.writeStringCR("M  V30 BEGIN ATOM");

   int i;
   int iw = 1;
   QS_DEF(Array<char>, buf);

   _atom_mapping.clear_resize(mol.vertexEnd());
   _bond_mapping.clear_resize(mol.edgeEnd());

   for (i = mol.vertexBegin(); i < mol.vertexEnd(); i = mol.vertexNext(i), iw++)
   {
      int atom_number = mol.getAtomNumber(i);
      int isotope = mol.getAtomIsotope(i);
      ArrayOutput out(buf);

      _atom_mapping[i] = iw;
      out.printf("%d ", iw);
      QS_DEF(Array<int>, list);
      int query_atom_type;

      if (atom_number == ELEM_H && isotope == 2)
      {
         out.writeChar('D');
         isotope = 0;
      }
      else if (atom_number == ELEM_H && isotope == 3)
      {
         out.writeChar('T');
         isotope = 0;
      }
      else if (mol.isPseudoAtom(i))
         out.writeString(mol.getPseudoAtom(i));
      else if (mol.isRSite(i))
         out.writeString("R#");
      else if (atom_number > 0)
      {
         _writeAtomLabel(out, atom_number);
      }
      else if (qmol != 0 &&
              (query_atom_type = QueryMolecule::parseQueryAtom(*qmol, i, list)) != -1)
      {
         if (query_atom_type == QueryMolecule::QUERY_ATOM_A)
            out.writeChar('A');
         else if (query_atom_type == QueryMolecule::QUERY_ATOM_Q)
            out.writeChar('Q');
         else if (query_atom_type == QueryMolecule::QUERY_ATOM_X)
            out.writeChar('X');
         else if (query_atom_type == QueryMolecule::QUERY_ATOM_LIST ||
                  query_atom_type == QueryMolecule::QUERY_ATOM_NOTLIST)
         {
            int k;

            if (query_atom_type == QueryMolecule::QUERY_ATOM_NOTLIST)
               out.writeString("NOT");
            
            out.writeChar('[');
            for (k = 0; k < list.size(); k++)
            {
               if (k > 0)
                  out.writeChar(',');
               _writeAtomLabel(out, list[k]);
            }
            out.writeChar(']');
         }
      }
      else if (atom_number == -1)
         out.writeChar('A');
      else
         throw Error("molfile 3000: can not save atom %d because of unsupported "
                     "query feature", i); 

      int aam = 0, ecflag = 0, irflag = 0;

      if (reactionAtomMapping != 0)
         aam = reactionAtomMapping->at(i);
      if (reactionAtomInversion != 0)
         irflag = reactionAtomInversion->at(i);
      if (reactionAtomExactChange != 0)
         ecflag = reactionAtomExactChange->at(i);

      Vec3f &xyz = mol.getAtomXyz(i);
      int charge = mol.getAtomCharge(i);
      int radical = 0;
      int valence = mol.getExplicitValence(i);

      if (!mol.isQueryMolecule())
         valence = mol.asMolecule().getExplicitOrUnusualValence(i);

      if (!mol.isRSite(i) && !mol.isPseudoAtom(i))
         radical = mol.getAtomRadical_NoThrow(i, 0);

      out.printf(" %f %f %f %d", xyz.x, xyz.y, xyz.z, aam);

      if ((mol.isQueryMolecule() && charge != CHARGE_UNKNOWN) || (!mol.isQueryMolecule() && charge != 0))
         out.printf(" CHG=%d", charge);
      if (!mol.isQueryMolecule())
      {
         if (mol.getAtomAromaticity(i) == ATOM_AROMATIC &&
                 ((atom_number != ELEM_C && atom_number != ELEM_O) || charge != 0))
         {
            int hcount = mol.asMolecule().getImplicitH_NoThrow(i, -1);

            if (hcount >= 0)
               out.printf(" HCOUNT=%d", hcount + 1);
         }
      }
      if (radical > 0)
         out.printf(" RAD=%d", radical);
      if (isotope > 0)
         out.printf(" MASS=%d", isotope);
      if (valence > 0)
         out.printf(" VAL=%d", valence);
      if (irflag > 0)
         out.printf(" INVRET=%d", irflag);
      if (ecflag > 0)
         out.printf(" EXACHG=%d", ecflag);

      if (mol.isRSite(i))
      {
         int k;

         QS_DEF(Array<int>, rg_list);
         mol.getAllowedRGroups(i, rg_list);

         if (rg_list.size() > 0)
         {
            out.printf(" RGROUPS=(%d", rg_list.size());
            for (k = 0; k < rg_list.size(); k++)
               out.printf(" %d", rg_list[k]);
            out.writeChar(')');

            if (!_checkAttPointOrder(mol, i))
            {
               const Vertex &vertex = mol.getVertex(i);
               
               out.printf(" ATTCHORD=(%d", vertex.degree() * 2);
               for (k = 0; k < vertex.degree(); k++)
                  out.printf(" %d %d", _atom_mapping[mol.getRSiteAttachmentPointByOrder(i, k)], k + 1);

               out.writeChar(')');
            }
         }
      }

      if (mol.attachmentPointCount() > 0)
      {
         int val = 0;

         for (int idx = 1; idx <= mol.attachmentPointCount(); idx++)
         {
            int j;

            for (j = 0; mol.getAttachmentPoint(idx, j) != -1; j++)
               if (mol.getAttachmentPoint(idx, j) == i)
               {
                  val |= 1 << (idx - 1);
                  break;
               }

            if (mol.getAttachmentPoint(idx, j) != -1)
               break;
         }

         if (val > 0)
            out.printf(" ATTCHPT=%d", val == 3 ? -1 : val);
      }

      _writeMultiString(output, buf.ptr(), buf.size());
   }

   output.writeStringCR("M  V30 END ATOM");
   output.writeStringCR("M  V30 BEGIN BOND");

   iw = 1;

   for (i = mol.edgeBegin(); i < mol.edgeEnd(); i = mol.edgeNext(i), iw++)
   {
      const Edge &edge = mol.getEdge(i);
      int bond_order = mol.getBondOrder(i);
      ArrayOutput out(buf);

      _bond_mapping[i] = iw;

      if (bond_order < 0 && qmol != 0)
      {
         int qb = QueryMolecule::getQueryBondType(qmol->getBond(i));

         if (qb == QueryMolecule::QUERY_BOND_SINGLE_OR_DOUBLE)
            bond_order = 5;
         else if (qb == QueryMolecule::QUERY_BOND_SINGLE_OR_AROMATIC)
            bond_order = 6;
         else if (qb == QueryMolecule::QUERY_BOND_DOUBLE_OR_AROMATIC)
            bond_order = 7;
         else if (qb == QueryMolecule::QUERY_BOND_ANY)
            bond_order = 8;
      }

      if (bond_order < 0)
         throw Error("unrepresentable query bond");

      out.printf("%d %d %d %d", iw, bond_order, _atom_mapping[edge.beg], _atom_mapping[edge.end]);

      int direction = mol.stereocenters.getBondDirection(i);

      switch (direction)
      {
         case MoleculeStereocenters::BOND_UP:     out.printf(" CFG=1"); break;
         case MoleculeStereocenters::BOND_EITHER: out.printf(" CFG=2"); break;
         case MoleculeStereocenters::BOND_DOWN:   out.printf(" CFG=3"); break;
         case 0:
            if (mol.cis_trans.isIgnored(i))
               out.printf(" CFG=2");
            break;
      }

      int reacting_center = 0;

      if(reactionBondReactingCenter != 0 && reactionBondReactingCenter->at(i) != 0)
         reacting_center = reactionBondReactingCenter->at(i);

      if (reacting_center != 0)
         out.printf(" RXCTR=%d", reacting_center);

      _writeMultiString(output, buf.ptr(), buf.size());
   }

   output.writeStringCR("M  V30 END BOND");

   MoleculeStereocenters &stereocenters = mol.stereocenters;

   if (stereocenters.begin() != stereocenters.end() || highlighting != 0)
   {
      output.writeStringCR("M  V30 BEGIN COLLECTION");

      QS_DEF(Array<int>, processed);

      processed.clear_resize(mol.vertexEnd());
      processed.zerofill();

      for (i = mol.vertexBegin(); i != mol.vertexEnd(); i = mol.vertexNext(i))
      {
         if (processed[i])
            continue;

         ArrayOutput out(buf);
         int j, type = stereocenters.getType(i);

         if (type == MoleculeStereocenters::ATOM_ABS)
            out.writeString("MDLV30/STEABS ATOMS=(");
         else if (type == MoleculeStereocenters::ATOM_OR)
            out.printf("MDLV30/STEREL%d ATOMS=(", stereocenters.getGroup(i));
         else if (type == MoleculeStereocenters::ATOM_AND)
            out.printf("MDLV30/STERAC%d ATOMS=(", stereocenters.getGroup(i));
         else
            continue;
            
         QS_DEF(Array<int>, list);

         list.clear();
         list.push(i);

         for (j = mol.vertexNext(i); j < mol.vertexEnd(); j = mol.vertexNext(j))
            if (stereocenters.sameGroup(i, j))
            {
               list.push(j);
               processed[j] = 1;
            }

         out.printf("%d", list.size());
         for (j = 0; j < list.size(); j++)
            out.printf(" %d", _atom_mapping[list[j]]);
         out.writeChar(')');

         _writeMultiString(output, buf.ptr(), buf.size());
      }

      if (highlighting != 0)
      {
         if (highlighting->numEdges() > 0)
         {
            const Array<int> &h_bonds = highlighting->getEdges();
            ArrayOutput out(buf);
            out.writeString("MDLV30/HILITE BONDS=(");

            out.printf("%d", highlighting->numEdges());
            for (i = mol.edgeBegin(); i != mol.edgeEnd(); i = mol.edgeNext(i))
               if (h_bonds[i])
                  out.printf(" %d", _bond_mapping[i]);
            out.writeChar(')');

            _writeMultiString(output, buf.ptr(), buf.size());
         }
         if (highlighting->numVertices() > 0)
         {
            const Array<int> &h_atoms = highlighting->getVertices();
            ArrayOutput out(buf);
            out.writeString("MDLV30/HILITE ATOMS=(");

            out.printf("%d", highlighting->numVertices());
            for (i = mol.vertexBegin(); i != mol.vertexEnd(); i = mol.vertexNext(i))
               if (h_atoms[i])
                  out.printf(" %d", _atom_mapping[i]);
            out.writeChar(')');

            _writeMultiString(output, buf.ptr(), buf.size());
         }
      }
      output.writeStringCR("M  V30 END COLLECTION");
   }

   output.writeStringCR("M  V30 END CTAB");

   if (qmol != 0)
   {
      int n_rgroups = qmol->rgroups.getRGroupCount();
      for (i = 1; i <= n_rgroups; i++)
         if (qmol->rgroups.getRGroup(i).fragments.size() > 0)
            _writeRGroup(output, *qmol, i);
   }
}

void MolfileSaver::_writeOccurrenceRanges (Output &out, const Array<int> &occurrences)
{
   for (int i = 0; i < occurrences.size(); i++)
   {
      int occurrence = occurrences[i];

      if ((occurrence & 0xFFFF) == 0xFFFF)
         out.printf(">%d", (occurrence >> 16) - 1);
      else if ((occurrence >> 16) == (occurrence & 0xFFFF))
         out.printf("%d", occurrence >> 16);
      else if ((occurrence >> 16) == 0)
         out.printf("<%d", (occurrence & 0xFFFF) + 1);
      else
         out.printf("%d-%d", occurrence >> 16, occurrence & 0xFFFF);

      if (i != occurrences.size() - 1)
         out.printf(", ");
   }
}

void MolfileSaver::_writeRGroup (Output &output, QueryMolecule &query, int rg_idx)
{
   QS_DEF(Array<char>, buf);
   ArrayOutput out(buf);
   RGroup &rgroup = query.rgroups.getRGroup(rg_idx);

   output.printfCR("M  V30 BEGIN RGROUP %d", rg_idx);

   out.printf("RLOGIC %d %d ", rgroup.if_then, rgroup.rest_h);

   _writeOccurrenceRanges(out, rgroup.occurrence);

   _writeMultiString(output, buf.ptr(), buf.size());

   for (int i = 0; i < rgroup.fragmentsCount(); i++)
      _writeCtab(output, *rgroup.fragments[i], true);

   output.writeStringCR("M  V30 END RGROUP");
}

void MolfileSaver::_writeCtab2000 (Output &output, BaseMolecule &mol, bool query)
{
   QueryMolecule *qmol = 0;

   if (query)
      qmol = (QueryMolecule *)(&mol);

   int i;
   QS_DEF(Array<int[2]>, radicals);
   QS_DEF(Array<int>, charges);
   QS_DEF(Array<int>, isotopes);
   QS_DEF(Array<int>, pseudoatoms);
   QS_DEF(Array<int>, atom_lists);

   _atom_mapping.clear_resize(mol.vertexEnd());
   _bond_mapping.clear_resize(mol.edgeEnd());

   radicals.clear();
   charges.clear();
   isotopes.clear();
   pseudoatoms.clear();
   atom_lists.clear();

   int iw = 1;

   for (i = mol.vertexBegin(); i < mol.vertexEnd(); i = mol.vertexNext(i), iw++)
   {
      char label[3] = {' ', ' ', ' '};
      int valence = 0, radical = 0, charge = 0, stereo_parity = 0,
         hydrogens_count = 0, stereo_care = 0,
         aam = 0, irflag = 0, ecflag = 0;

      int atom_number = mol.getAtomNumber(i);
      int atom_isotope = mol.getAtomIsotope(i);
      int atom_charge = mol.getAtomCharge(i);
      int atom_radical = 0;

      _atom_mapping[i] = iw;

      if (!mol.isRSite(i) && !mol.isPseudoAtom(i))
         atom_radical = mol.getAtomRadical_NoThrow(i, 0);

      if (mol.isRSite(i))
      {
         label[0] = 'R';
         label[1] = '#';
      }
      else if (mol.isPseudoAtom(i))
      {
         const char *pseudo = mol.getPseudoAtom(i);

         if (strlen(pseudo) <= 3)
            memcpy(label, pseudo, __min(strlen(pseudo), 3));
         else
         {
            label[0] = 'A';
            pseudoatoms.push(i);
         }
      }
      else if (atom_number == -1)
      {
         if (qmol == 0)
            throw Error("internal: atom number = -1, but qmol == 0");

         QS_DEF(Array<int>, list);

         int query_atom_type = QueryMolecule::parseQueryAtom(*qmol, i, list);

         if (query_atom_type == QueryMolecule::QUERY_ATOM_A)
            label[0] = 'A';
         else if (query_atom_type == QueryMolecule::QUERY_ATOM_Q)
            label[0] = 'Q';
         else if (query_atom_type == QueryMolecule::QUERY_ATOM_X)
            label[0] = 'X';
         else if (query_atom_type == QueryMolecule::QUERY_ATOM_LIST ||
                  query_atom_type == QueryMolecule::QUERY_ATOM_NOTLIST)
         {
            label[0] = 'L';
            atom_lists.push(i);
         }
         else
            label[0] = 'A';
            //throw Error("error saving atom #%d: unsupported query atom", i);
      }
      else if (atom_number == ELEM_H && atom_isotope == 2)
         label[0] = 'D';   
      else if (atom_number == ELEM_H && atom_isotope == 3)
         label[0] = 'T';
      else
      {
         const char *str = Element::toString(atom_number);

         label[0] = str[0];
         if (str[1] != 0)
            label[1] = str[1];

         if (atom_isotope > 0)
            isotopes.push(i);
      }

      if (reactionAtomMapping != 0)
         aam = reactionAtomMapping->at(i);
      if (reactionAtomInversion != 0)
         irflag = reactionAtomInversion->at(i);
      if (reactionAtomExactChange != 0)
         ecflag = reactionAtomExactChange->at(i);

      int explicit_valence = mol.getExplicitValence(i);

      if (!mol.isQueryMolecule())
         explicit_valence = mol.asMolecule().getExplicitOrUnusualValence(i);

      if (explicit_valence > 0 && explicit_valence < 14)
         valence = explicit_valence;

      if (atom_charge != CHARGE_UNKNOWN &&
          atom_charge >= -15 && atom_charge <= 15)
          charge = atom_charge;

      if (charge != 0)
         charges.push(i);

      if (atom_radical >= 0 && atom_radical <= 3)
         radical = atom_radical;

      if (radical != 0)
      {
         int *r = radicals.push();
         r[0] = i;
         r[1] = radical;
      }

      if (!mol.isQueryMolecule())
      {
         if (mol.getAtomAromaticity(i) == ATOM_AROMATIC &&
                 ((atom_number != ELEM_C && atom_number != ELEM_O) || atom_charge != 0))
            hydrogens_count = mol.asMolecule().getImplicitH_NoThrow(i, -1) + 1;
      }
      
      Vec3f &pos = mol.getAtomXyz(i);

      output.printfCR("%10.4f%10.4f%10.4f %c%c%c%2d"
                    "%3d%3d%3d%3d%3d"
                    "%3d%3d%3d%3d%3d%3d",
         pos.x, pos.y, pos.z, label[0], label[1], label[2], 0,
         0, stereo_parity, hydrogens_count, stereo_care, valence,
         0, 0, 0,
         aam, irflag, ecflag);
   }

   iw = 1;

   for (i = mol.edgeBegin(); i < mol.edgeEnd(); i = mol.edgeNext(i))
   {
      const Edge &edge = mol.getEdge(i);
      int bond_order = mol.getBondOrder(i);

      if (bond_order < 0 && qmol != 0)
      {
         int qb = QueryMolecule::getQueryBondType(qmol->getBond(i));

         if (qb == QueryMolecule::QUERY_BOND_SINGLE_OR_DOUBLE)
            bond_order = 5;
         else if (qb == QueryMolecule::QUERY_BOND_SINGLE_OR_AROMATIC)
            bond_order = 6;
         else if (qb == QueryMolecule::QUERY_BOND_DOUBLE_OR_AROMATIC)
            bond_order = 7;
         else if (qb == QueryMolecule::QUERY_BOND_ANY)
            bond_order = 8;
      }

      if (bond_order < 0)
         throw Error("unrepresentable query bond");

      int stereo = 0;
      int topology = 0;
      int reacting_center = 0;

      int direction = mol.stereocenters.getBondDirection(i);

      switch (direction)
      {
         case MoleculeStereocenters::BOND_UP: stereo = 1; break;
         case MoleculeStereocenters::BOND_DOWN: stereo = 6; break;
         case MoleculeStereocenters::BOND_EITHER: stereo = 4; break;
         case 0:
            if (mol.cis_trans.isIgnored(i))
               stereo = 3;
            break;
      }

      if(reactionBondReactingCenter != 0 && reactionBondReactingCenter->at(i) != 0)
         reacting_center = reactionBondReactingCenter->at(i);

      output.printfCR("%3d%3d%3d%3d%3d%3d%3d",
                _atom_mapping[edge.beg], _atom_mapping[edge.end],
                bond_order, stereo, 0, topology, reacting_center);
      _bond_mapping[i] = iw++;
   }

   if (charges.size() > 0)
   {
      int j = 0;
      while (j < charges.size())
      {
         output.printf("M  CHG%3d", __min(charges.size(), j + 8) - j);
         for (i = j; i < __min(charges.size(), j + 8); i++)
            output.printf(" %3d %3d", _atom_mapping[charges[i]], mol.getAtomCharge(charges[i]));
         output.writeCR();
         j += 8;
      }
   }

   if (radicals.size() > 0)
   {
      int j = 0;
      while (j < radicals.size())
      {
         output.printf("M  RAD%3d", __min(radicals.size(), j + 8) - j);
         for (i = j; i < __min(radicals.size(), j + 8); i++)
            output.printf(" %3d %3d", _atom_mapping[radicals[i][0]], radicals[i][1]);
         output.writeCR();
         j += 8;
      }
   }

   if (isotopes.size() > 0)
   {
      int j = 0;
      while (j < isotopes.size())
      {
         output.printf("M  ISO%3d", __min(isotopes.size(), j + 8) - j);
         for (i = j; i < __min(isotopes.size(), j + 8); i++)
            output.printf(" %3d %3d", _atom_mapping[isotopes[i]], mol.getAtomIsotope(isotopes[i]));
         output.writeCR();
         j += 8;
      }
   }

   for (i = 0; i < atom_lists.size(); i++)
   {
      int atom_idx = atom_lists[i];
      QS_DEF(Array<int>, list);

      int query_atom_type = QueryMolecule::parseQueryAtom(*qmol, atom_idx, list);

      if (query_atom_type != QueryMolecule::QUERY_ATOM_LIST &&
          query_atom_type != QueryMolecule::QUERY_ATOM_NOTLIST)
         throw Error("internal: atom list not recognized");

      if (list.size() < 1)
         throw Error("internal: atom list size is zero");

      output.printf("M  ALS %3d%3d %c ", _atom_mapping[atom_idx], list.size(),
         query_atom_type == QueryMolecule::QUERY_ATOM_NOTLIST ? 'T' : 'F');

      int j;

      for (j = 0; j < list.size(); j++)
      {
         char c1 = ' ', c2 = ' ';
         const char *str = Element::toString(list[j]);

         c1 = str[0];
         if (str[1] != 0)
            c2 = str[1];
         
         output.printf("%c%c ", c1, c2);
      }
      output.writeCR();
   }
   
   for (i = 0; i < pseudoatoms.size(); i++)
   {
      output.printfCR("A  %3d", _atom_mapping[pseudoatoms[i]]);
      output.writeString(mol.getPseudoAtom(pseudoatoms[i]));
      output.writeCR();
   }

   QS_DEF(Array<int>, sgroup_ids);
   QS_DEF(Array<int>, sgroup_types);
   QS_DEF(Array<int>, inv_mapping_ru);

   sgroup_ids.clear();
   sgroup_types.clear();
   inv_mapping_ru.clear_resize(mol.repeating_units.end());

   for (i = mol.superatoms.begin(); i != mol.superatoms.end(); i = mol.superatoms.next(i))
   {
      sgroup_ids.push(i);
      sgroup_types.push(_SGROUP_TYPE_SUP);
   }
   for (i = mol.data_sgroups.begin(); i != mol.data_sgroups.end(); i = mol.data_sgroups.next(i))
   {
      sgroup_ids.push(i);
      sgroup_types.push(_SGROUP_TYPE_DAT);
   }
   for (i = mol.repeating_units.begin(); i != mol.repeating_units.end(); i = mol.repeating_units.next(i))
   {
      inv_mapping_ru[i] = sgroup_ids.size();
      sgroup_ids.push(i);
      sgroup_types.push(_SGROUP_TYPE_SRU);
   }
   for (i = mol.multiple_groups.begin(); i != mol.multiple_groups.end(); i = mol.multiple_groups.next(i))
   {
      sgroup_ids.push(i);
      sgroup_types.push(_SGROUP_TYPE_MUL);
   }
   for (i = mol.generic_sgroups.begin(); i != mol.generic_sgroups.end(); i = mol.generic_sgroups.next(i))
   {
      sgroup_ids.push(i);
      sgroup_types.push(_SGROUP_TYPE_GEN);
   }

   if (sgroup_ids.size() > 0)
   {
      int j;
      for (j = 0; j < sgroup_ids.size(); j += 8)
      {
         output.printf("M  STY%3d", __min(sgroup_ids.size(), j + 8) - j);
         for (i = j; i < __min(sgroup_ids.size(), j + 8); i++)
         {
            const char *type;

            switch (sgroup_types[i])
            {
               case _SGROUP_TYPE_DAT: type = "DAT"; break;
               case _SGROUP_TYPE_MUL: type = "MUL"; break;
               case _SGROUP_TYPE_SRU: type = "SRU"; break;
               case _SGROUP_TYPE_SUP: type = "SUP"; break;
               case _SGROUP_TYPE_GEN: type = "GEN"; break;
               default: throw Error("internal: bad sgroup type");
            }

            output.printf(" %3d %s", i + 1, type);
         }
         output.writeCR();
      }
      for (j = 0; j < sgroup_ids.size(); j += 8)
      {
         output.printf("M  SLB%3d", __min(sgroup_ids.size(), j + 8) - j);
         for (i = j; i < __min(sgroup_ids.size(), j + 8); i++)
            output.printf(" %3d %3d", i + 1, i + 1);
         output.writeCR();
      }

      for (j = 0; j < mol.repeating_units.size(); j += 8)
      {
         output.printf("M  SCN%3d", __min(mol.repeating_units.size(), j + 8) - j);
         for (i = j; i < __min(mol.repeating_units.size(), j + 8); i++)
         {
            BaseMolecule::RepeatingUnit &ru = mol.repeating_units[i];

            output.printf(" %3d ", inv_mapping_ru[i] + 1);

            if (ru.connectivity == BaseMolecule::RepeatingUnit::HEAD_TO_HEAD)
               output.printf("HH  ");
            else if (ru.connectivity == BaseMolecule::RepeatingUnit::HEAD_TO_TAIL)
               output.printf("HT  ");
            else
               output.printf("EU  ");
         }
         output.writeCR();
      }

      for (i = 0; i < sgroup_ids.size(); i++)
      {
         BaseMolecule::SGroup *sgroup;

         switch (sgroup_types[i])
         {
            case _SGROUP_TYPE_DAT: sgroup = &mol.data_sgroups[sgroup_ids[i]]; break;
            case _SGROUP_TYPE_MUL: sgroup = &mol.multiple_groups[sgroup_ids[i]]; break;
            case _SGROUP_TYPE_SRU: sgroup = &mol.repeating_units[sgroup_ids[i]]; break;
            case _SGROUP_TYPE_SUP: sgroup = &mol.superatoms[sgroup_ids[i]]; break;
            case _SGROUP_TYPE_GEN: sgroup = &mol.generic_sgroups[sgroup_ids[i]]; break;
            default: throw Error("internal: bad sgroup type");
         }

         for (j = 0; j < sgroup->atoms.size(); j += 8)
         {
            int k;
            output.printf("M  SAL %3d%3d", i + 1, __min(sgroup->atoms.size(), j + 8) - j);
            for (k = j; k < __min(sgroup->atoms.size(), j + 8); k++)
               output.printf(" %3d", _atom_mapping[sgroup->atoms[k]]);
            output.writeCR();
         }
         for (j = 0; j < sgroup->bonds.size(); j += 8)
         {
            int k;
            output.printf("M  SBL %3d%3d", i + 1, __min(sgroup->bonds.size(), j + 8) - j);
            for (k = j; k < __min(sgroup->bonds.size(), j + 8); k++)
               output.printf(" %3d", _bond_mapping[sgroup->bonds[k]]);
            output.writeCR();
         }

         if (sgroup_types[i] == _SGROUP_TYPE_SUP)
         {
            BaseMolecule::Superatom &superatom = mol.superatoms[sgroup_ids[i]];

            if (superatom.subscript.size() > 1)
               output.printf("M  SMT %3d %s", i + 1, superatom.subscript.ptr());
            if (superatom.bond_idx >= 0)
            {
               output.printf("M  SBV %3d %3d %9.4f %9.4f", i + 1,
                       _bond_mapping[superatom.bond_idx], superatom.bond_dir.x, superatom.bond_dir.y);
            }
            output.writeCR();
         }
         else if (sgroup_types[i] == _SGROUP_TYPE_DAT)
         {
            BaseMolecule::DataSGroup &datasgroup = mol.data_sgroups[sgroup_ids[i]];
            int k = 30;

            output.printf("M  SDT %3d ", i + 1);
            if (datasgroup.description.size() > 1)
            {
               output.printf("%s", datasgroup.description.ptr());
               k -= datasgroup.description.size() - 1;
            }
            while (k-- > 0)
               output.writeChar(' ');
            output.writeChar('F');
            output.writeCR();

            output.printf("M  SDD %3d %10.4f%10.4f    %c%c%c   ALL  1       %1d  ",
                i + 1, datasgroup.display_pos.x, datasgroup.display_pos.y,
                datasgroup.attached ? 'A' : 'D',
                datasgroup.relative ? 'R' : 'A',
                datasgroup.display_units ? 'U' : ' ',
                datasgroup.dasp_pos);
            output.writeCR();

            k = datasgroup.data.size();
            char *ptr = datasgroup.data.ptr();
            while (k > 69)
            {
               output.printf("M  SCD %3d %69s", i + 1, ptr);
               ptr += 69;
               k -= 69;
               output.writeCR();
            }
            output.printf("M  SED %3d %.*s", i + 1, k, ptr);
            output.writeCR();
         }
         else if (sgroup_types[i] == _SGROUP_TYPE_MUL)
         {
            BaseMolecule::MultipleGroup &mg = mol.multiple_groups[sgroup_ids[i]];

            for (j = 0; j < mg.parent_atoms.size(); j += 8)
            {
               int k;
               output.printf("M  SPA %3d%3d", i + 1, __min(mg.parent_atoms.size(), j + 8) - j);
               for (k = j; k < __min(mg.parent_atoms.size(), j + 8); k++)
                  output.printf(" %3d", _atom_mapping[mg.parent_atoms[k]]);
               output.writeCR();
            }

            output.printf("M  SMT %3d %d\n", i + 1, mg.multiplier);
         }
         for (j = 0; j < sgroup->brackets.size(); j++)
         {
            output.printf("M  SDI %3d  4 %9.4f %9.4f %9.4f %9.4f\n", i + 1,
                    sgroup->brackets[j][0].x, sgroup->brackets[j][0].y,
                    sgroup->brackets[j][1].x, sgroup->brackets[j][1].y);
         }

      }
   }

}

void MolfileSaver::_writeRGroupIndices2000 (Output &output, BaseMolecule &mol)
{
   int i, j;

   QS_DEF(Array<int>, atom_ids);
   QS_DEF(Array<int>, rg_ids);

   atom_ids.clear();
   rg_ids.clear();

   for (i = mol.vertexBegin(); i < mol.vertexEnd(); i = mol.vertexNext(i))
   {
      if (!mol.isRSite(i))
         continue;

      QS_DEF(Array<int>, rg_list);

      mol.getAllowedRGroups(i, rg_list);

      for (j = 0; j < rg_list.size(); j++)
      {
         atom_ids.push(_atom_mapping[i]);
         rg_ids.push(rg_list[j]);
      }
   }

   if (atom_ids.size() > 0)
   {
      output.printf("M  RGP%3d", atom_ids.size());
      for (i = 0; i < atom_ids.size(); i++)
         output.printf(" %3d %3d", atom_ids[i], rg_ids[i]);
      output.writeCR();
   }

   for (i = mol.vertexBegin(); i < mol.vertexEnd(); i = mol.vertexNext(i))
   {
      if (!mol.isRSite(i))
         continue;
      
      if (!_checkAttPointOrder(mol, i))
      {
         const Vertex &vertex = mol.getVertex(i);
         int k;

         output.printf("M  AAL %3d%3d", _atom_mapping[i], vertex.degree());
         for (k = 0; k < vertex.degree(); k++)
            output.printf(" %3d %3d", _atom_mapping[mol.getRSiteAttachmentPointByOrder(i, k)], k + 1);

         output.writeCR();
      }
   }
}

void MolfileSaver::_writeAttachmentValues2000 (Output &output, BaseMolecule &fragment)
{
   if (fragment.attachmentPointCount() == 0)
      return;

   RedBlackMap<int, int> orders;
   int i;

   for (i = 1; i <= fragment.attachmentPointCount(); i++)
   {
      int j = 0;
      int idx;

      while ((idx = fragment.getAttachmentPoint(i, j++)) != -1)
      {
         int *val;

         if ((val = orders.at2(_atom_mapping[idx])) == 0)
            orders.insert(_atom_mapping[idx], 1 << (i - 1));
         else
            *val |= 1 << (i - 1);
      }

   }

   output.printf("M  APO%3d", orders.size());

   for (i = orders.begin(); i < orders.end(); i = orders.next(i))
      output.printf(" %3d %3d", orders.key(i), orders.value(i));

   output.writeCR();
}

bool MolfileSaver::_checkAttPointOrder (BaseMolecule &mol, int rsite)
{
   const Vertex &vertex = mol.getVertex(rsite);
   int i;

   for (i = 0; i < vertex.degree() - 1; i++)
   {
      int cur = mol.getRSiteAttachmentPointByOrder(rsite, i);
      int next = mol.getRSiteAttachmentPointByOrder(rsite, i + 1);

      if (cur == -1 || next == -1)
         return true; // here we treat "undefined" as "ok"

      if (cur > next)
         return false;
   }

   return true;
}
