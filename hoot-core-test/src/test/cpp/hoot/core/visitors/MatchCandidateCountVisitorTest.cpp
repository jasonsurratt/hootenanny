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
 * @copyright Copyright (C) 2014 DigitalGlobe (http://www.digitalglobe.com/)
 */

// Hoot
#include <hoot/core/OsmMap.h>
#include <hoot/core/conflate/MatchFactory.h>
#include <hoot/core/MapReprojector.h>
#include <hoot/core/elements/Way.h>
#include <hoot/core/io/OsmReader.h>
#include <hoot/core/io/OsmWriter.h>
#include <hoot/core/visitors/MatchCandidateCountVisitor.h>
using namespace hoot;

// Boost
using namespace boost;

// CPP Unit
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>

// Qt
#include <QDebug>
#include <QDir>
#include <QBuffer>
#include <QByteArray>

#include "../TestUtils.h"

namespace hoot
{

class MatchCandidateCountVisitorTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE(MatchCandidateCountVisitorTest);
  CPPUNIT_TEST(runMatchCandidateCountTest);
  CPPUNIT_TEST_SUITE_END();

public:

  void tearDown()
  {
    TestUtils::resetEnvironment();
  }

  void runMatchCandidateCountTest()
  {
    OsmReader reader;
    shared_ptr<OsmMap> map(new OsmMap());
    OsmMap::resetCounters();
    reader.setDefaultStatus(Status::Unknown1);
    reader.read("test-files/conflate/unified/AllDataTypesA.osm", map);
    reader.setDefaultStatus(Status::Unknown2);
    reader.read("test-files/conflate/unified/AllDataTypesB.osm", map);
    MapReprojector::reprojectToPlanar(map);

    QStringList matchCreators;

    matchCreators.clear();
    matchCreators.append("hoot::BuildingMatchCreator");
    MatchFactory::getInstance().reset();
    MatchFactory::_setMatchCreators(matchCreators);
    MatchCandidateCountVisitor uut(MatchFactory::getInstance().getCreators());
    map->visitRo(uut);
    CPPUNIT_ASSERT_EQUAL((int)18, (int)uut.getStat());
    QMap<QString, long> matchCandidateCountsByMatchCreator =
      any_cast<QMap<QString, long> >(uut.getData());
    CPPUNIT_ASSERT_EQUAL(1, matchCandidateCountsByMatchCreator.size());
    CPPUNIT_ASSERT_EQUAL((long)18, matchCandidateCountsByMatchCreator["hoot::BuildingMatchCreator"]);

    matchCreators.clear();
    matchCreators.append("hoot::HighwayMatchCreator");
    MatchFactory::getInstance().reset();
    MatchFactory::_setMatchCreators(matchCreators);
    MatchCandidateCountVisitor uut2(MatchFactory::getInstance().getCreators());
    map->visitRo(uut2);
    CPPUNIT_ASSERT_EQUAL((int)8, (int)uut2.getStat());
    matchCandidateCountsByMatchCreator = any_cast<QMap<QString, long> >(uut2.getData());
    CPPUNIT_ASSERT_EQUAL(1, matchCandidateCountsByMatchCreator.size());
    CPPUNIT_ASSERT_EQUAL((long)8, matchCandidateCountsByMatchCreator["hoot::HighwayMatchCreator"]);

    matchCreators.clear();
    matchCreators.append("hoot::BuildingMatchCreator");
    matchCreators.append("hoot::HighwayMatchCreator");
    matchCreators.append("hoot::PlacesPoiMatchCreator");
    matchCreators.append("hoot::CustomPoiMatchCreator");
    matchCreators.append("hoot::ScriptMatchCreator,LineStringGenericTest.js");
    MatchFactory::getInstance().reset();
    MatchFactory::_setMatchCreators(matchCreators);
    MatchCandidateCountVisitor uut3(MatchFactory::getInstance().getCreators());
    map->visitRo(uut3);
    CPPUNIT_ASSERT_EQUAL((int)76, (int)uut3.getStat());
    matchCandidateCountsByMatchCreator = any_cast<QMap<QString, long> >(uut3.getData());
    CPPUNIT_ASSERT_EQUAL(5, matchCandidateCountsByMatchCreator.size());
    //TODO: These don't add up to the total...is there some overlap here?
    CPPUNIT_ASSERT_EQUAL((long)18, matchCandidateCountsByMatchCreator["hoot::BuildingMatchCreator"]);
    CPPUNIT_ASSERT_EQUAL((long)8, matchCandidateCountsByMatchCreator["hoot::HighwayMatchCreator"]);
    CPPUNIT_ASSERT_EQUAL((long)21, matchCandidateCountsByMatchCreator["hoot::PlacesPoiMatchCreator"]);
    CPPUNIT_ASSERT_EQUAL((long)21, matchCandidateCountsByMatchCreator["hoot::CustomPoiMatchCreator"]);
    CPPUNIT_ASSERT_EQUAL(
      (long)0,
      matchCandidateCountsByMatchCreator["hoot::hoot::ScriptMatchCreator,LineStringGenericTest.js"]);
  }
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MatchCandidateCountVisitorTest, "quick");
//CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MatchCandidateCountVisitorTest, "current");

}
