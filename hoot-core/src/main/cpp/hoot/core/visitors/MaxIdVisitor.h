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
 * @copyright Copyright (C) 2015 DigitalGlobe (http://www.digitalglobe.com/)
 */

#ifndef MAXIDVISITOR_H
#define MAXIDVISITOR_H

// hoot
#include <hoot/core/elements/ElementId.h>
#include <hoot/core/elements/ElementVisitor.h>

// Standard
#include <limits>

#include "SingleStatistic.h"

namespace hoot
{
using namespace std;

/**
 * Counts the number of elements.
 */
class MaxIdVisitor : public ElementVisitor, public SingleStatistic
{
public:

  MaxIdVisitor() : _maxId(-numeric_limits<long>::max()) {}

  virtual ~MaxIdVisitor() {}

  long getMinId() const { return _maxId; }

  double getStat() const { return _maxId; }

  virtual void visit(ElementType /*type*/, long id)
  {
    _maxId = std::max(id, _maxId);
  }

private:
  long _maxId;
};

}
#endif // MAXIDVISITOR_H
