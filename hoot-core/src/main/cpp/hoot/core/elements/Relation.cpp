/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
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
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. DigitalGlobe
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2013, 2014, 2015 DigitalGlobe (http://www.digitalglobe.com/)
 */
#include "Relation.h"

// geos
#include <geos/algorithm/CGAlgorithms.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/LineString.h>
#include <geos/geom/MultiPolygon.h>
#include <geos/geom/Polygon.h>
#include <geos/util/TopologyException.h>

// hoot
#include <hoot/core/OsmMap.h>
#include <hoot/core/elements/ElementVisitor.h>
#include <hoot/core/schema/OsmSchema.h>
#include <hoot/core/util/GeometryUtils.h>

#include "Way.h"

namespace hoot
{

QString Relation::INNER = "inner";
QString Relation::MULTILINESTRING = "multilinestring";
QString Relation::MULTIPOLYGON = "multipolygon";
QString Relation::OUTER = "outer";

Relation::Relation(const Relation& from) :
  Element(from.getStatus())
{
  _relationData = from._relationData;
}

Relation::Relation(Status s, long id, Meters circularError, QString type) :
  Element(s)
{
  _relationData.reset(new RelationData(id));
  _relationData->setCircularError(circularError);
  _relationData->setType(type);
}

void Relation::addElement(const QString& role, const shared_ptr<const Element>& e)
{
  addElement(role, e->getElementType(), e->getId());
}

void Relation::addElement(const QString& role, ElementId eid)
{
  _preGeometryChange();
  _makeWritable();
  _relationData->addElement(role, eid);
  _postGeometryChange();
}

void Relation::addElement(const QString& role, ElementType t, long id)
{
  addElement(role, ElementId(t, id));
}

void Relation::clear()
{
  _preGeometryChange();
  _makeWritable();
  _relationData->clear();
  _postGeometryChange();
}

bool Relation::contains(ElementId eid) const
{
  const vector<RelationData::Entry>& members = getMembers();

  for (size_t i = 0; i < members.size(); i++)
  {
    if (members[i].getElementId() == eid)
    {
      return true;
    }
  }
  return false;
}

Envelope* Relation::getEnvelope(const shared_ptr<const ElementProvider> &ep) const
{
  Envelope* result = new Envelope();
  const vector<RelationData::Entry>& members = getMembers();

  for (size_t i = 0; i < members.size(); i++)
  {
    const RelationData::Entry& m = members[i];
    // if any of the elements don't exist then return an empty envelope.
    if (ep->containsElement(m.getElementId()) == false)
    {
      result->setToNull();
      return result;
    }
    const shared_ptr<const Element> e = ep->getElement(m.getElementId());
    auto_ptr<Envelope> childEnvelope(e->getEnvelope(ep));

    if (childEnvelope->isNull())
    {
      result->setToNull();
      return result;
    }

    result->expandToInclude(childEnvelope.get());
  }

  return result;
}

void Relation::_makeWritable()
{
  // make sure we're the only one with a reference to the data before we modify it.
  if(_relationData.use_count() > 1)
  {
    _relationData.reset(new RelationData(*_relationData));
  }
}

void Relation::removeElement(const QString& role, const shared_ptr<const Element>& e)
{
  removeElement(role, e->getElementId());
}

void Relation::removeElement(const QString& role, ElementId eid)
{
  _preGeometryChange();
  _makeWritable();
  _relationData->removeElement(role, eid);
  _postGeometryChange();
}

void Relation::removeElement(ElementId eid)
{
  _preGeometryChange();
  _makeWritable();
  _relationData->removeElement(eid);
  _postGeometryChange();
}

void Relation::replaceElement(const shared_ptr<const Element>& from,
  const shared_ptr<const Element>& to)
{
  _preGeometryChange();
  _makeWritable();
  _relationData->replaceElement(from->getElementId(), to->getElementId());
  _postGeometryChange();
}

void Relation::setMembers(const vector<RelationData::Entry>& members)
{
  _preGeometryChange();
  _makeWritable();
  _relationData->setMembers(members);
  _postGeometryChange();
}

void Relation::setType(const QString& type)
{
  _makeWritable();
  _relationData->setType(type);
}

QString Relation::toString() const
{
  stringstream ss(stringstream::out);
  ss << "relation(" << getId() << ")" << endl;
  ss << "type: " << getType() << endl;
  ss << "members: ";
  for (size_t i = 0; i < getMembers().size(); i++)
  {
    ss << "  " << getMembers()[i].toString().toUtf8().data() << endl;
  }
  ss << endl;
  ss << "tags: " << getTags().toString().toUtf8().data();
  ss << "status: " << getStatusString().toUtf8().data();
  return QString::fromUtf8(ss.str().data());
}

void Relation::visitRo(const OsmMap& map, ElementVisitor &filter) const
{
  filter.visit(map.getRelation(getId()));

  const vector<RelationData::Entry>& members = getMembers();

  for (size_t i = 0; i < members.size(); i++)
  {
    const RelationData::Entry& m = members[i];
    if (map.containsElement(m.getElementId()))
    {
      if (m.getElementId().getType() == ElementType::Node)
      {
        map.getNode(m.getElementId().getId())->visitRo(map, filter);
      }
      else if (m.getElementId().getType() == ElementType::Way)
      {
        map.getWay(m.getElementId().getId())->visitRo(map, filter);
      }
      else if (m.getElementId().getType() == ElementType::Relation)
      {
        map.getRelation(m.getElementId().getId())->visitRo(map, filter);
      }
      else
      {
        assert(false);
      }
    }
  }
}

void Relation::visitRw(OsmMap& map, ElementVisitor& filter)
{
  filter.visit(map.getRelation(getId()));

  const vector<RelationData::Entry> members = getMembers();

  for (size_t i = 0; i < members.size(); i++)
  {
    const RelationData::Entry& m = members[i];
    if (map.containsElement(m.getElementId()))
    {
      if (m.getElementId().getType() == ElementType::Node &&
        map.containsNode(m.getElementId().getId()))
      {
        map.getNode(m.getElementId().getId())->visitRw(map, filter);
      }
      else if (m.getElementId().getType() == ElementType::Way &&
        map.containsWay(m.getElementId().getId()))
      {
        map.getWay(m.getElementId().getId())->visitRw(map, filter);
      }
      else if (m.getElementId().getType() == ElementType::Relation &&
        map.containsRelation(m.getElementId().getId()))
      {
        map.getRelation(m.getElementId().getId())->visitRw(map, filter);
      }
      else
      {
        assert(false);
      }
    }
  }
}

}
