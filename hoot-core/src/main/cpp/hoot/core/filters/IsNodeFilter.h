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
 * @copyright Copyright (C) 2013, 2014 DigitalGlobe (http://www.digitalglobe.com/)
 */
#ifndef ISNODEFILTER_H
#define ISNODEFILTER_H

// Qt
#include <QString>

// hoot
#include <hoot/core/elements/Element.h>

#include "ElementCriterion.h"

namespace hoot
{

class IsNodeFilter : public BaseElementFilter
{
public:
  IsNodeFilter(FilterType type) { _type = type; }

  virtual bool isFiltered(const Element& e) const
  {
    bool match = false;
    if (e.getElementType() == ElementType::Node)
    {
      match = true;
    }

    if (_type == KeepMatches)
    {
      return !match;
    }
    else
    {
      return match;
    }
  }

private:
  FilterType _type;
};

}

#endif // ISNODEFILTER_H
