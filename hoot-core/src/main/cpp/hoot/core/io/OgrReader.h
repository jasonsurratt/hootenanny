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
 * @copyright Copyright (C) 2012, 2013, 2014 DigitalGlobe (http://www.digitalglobe.com/)
 */

#ifndef __OGR_READER_H__
#define __OGR_READER_H__

// Hoot
#include <hoot/core/OsmMap.h>
#include <hoot/core/elements/ElementIterator.h>
#include <hoot/core/elements/Tags.h>
#include <hoot/core/util/Progress.h>

// Qt
#include <QHash>
#include <QString>
#include <QStringList>
#include <QXmlDefaultHandler>
class QString;

// Standard
#include <vector>

namespace hoot
{

class OgrReaderInternal;

/**
 * This class is broken out into an internal and external class to avoid issues with Python's
 * include file approach.
 */
class OgrReader
{
public:

  /**
   * Returns true if this appears to be a reasonable path without actually attempting to open the
   * data source.
   */
  static bool isReasonablePath(QString path);

  OgrReader();

  ~OgrReader();

  ElementIterator* createIterator(QString path, QString layer) const;

  QStringList getLayerNames(QString path);

  QStringList getFilteredLayerNames(QString path);

  void read(QString path, QString layer, shared_ptr<OsmMap> map, Progress progress);

  void setDefaultCircularError(Meters circularError);

  void setDefaultStatus(Status s);

  void setLimit(long limit);

  void setTranslationFile(QString translate);

  long getFeatureCount(QString path, QString layer);

protected:
  OgrReaderInternal* _d;
};

} // hoot


#endif // __OGR_READER_H__
