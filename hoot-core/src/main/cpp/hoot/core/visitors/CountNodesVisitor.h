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
 * @copyright Copyright (C) 2013, 2015 DigitalGlobe (http://www.digitalglobe.com/)
 */
#ifndef COUNT_NODES_VISITOR_H
#define COUNT_NODES_VISITOR_H

// hoot
#include <hoot/core/elements/ElementId.h>
#include <hoot/core/elements/ElementVisitor.h>

namespace hoot
{
using namespace std;

/**
 * Counts the number of nodes.
 */
class CountNodesVisitor : public ElementVisitor
{
public:

  CountNodesVisitor() : _count(0) {}

  virtual ~CountNodesVisitor() {}

  int getCount() const { return _count; }

  virtual void visit(ElementType type, long /*id*/) { if (type == ElementType::Node) _count++; }

private:
  int _count;
};

}

#endif // GETELEMENTSVISITOR_H
