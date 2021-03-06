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
#include "ServicesDb.h"

// hoot
#include <hoot/core/elements/Node.h>
#include <hoot/core/elements/Way.h>
#include <hoot/core/elements/Relation.h>
#include <hoot/core/io/db/SqlBulkInsert.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/HootException.h>
#include <hoot/core/util/Log.h>

// qt
#include <QStringList>
#include <QVariant>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>

// Standard
#include <math.h>
#include <cmath>
#include <fstream>

// tgs
#include <tgs/System/Time.h>

#include "db/InternalIdReserver.h"

namespace hoot
{

const Status ServicesDb::DEFAULT_ELEMENT_STATUS(Status::Invalid);

ServicesDb::ServicesDb()
{
  _init();
}

ServicesDb::~ServicesDb()
{
  LOG_VARD(_nodesInsertElapsed);
  LOG_VARD(_wayNodesInsertElapsed);
  LOG_VARD(_wayInsertElapsed);
  close();
}

Envelope ServicesDb::calculateEnvelope(long mapId) const
{
  Envelope result;

  // if you're having performance issues read this:
  // http://www.postgresql.org/docs/8.0/static/functions-aggregate.html
  QSqlQuery boundsQuery = _exec("SELECT MIN(latitude) as minLat, MAX(latitude) AS maxLat "
                             ", MIN(longitude) as minLon, MAX(longitude) AS maxLon"
                             " FROM " + _getNodesTableName(mapId));

  if (boundsQuery.next())
  {
    double minY = (double)boundsQuery.value(0).toLongLong() / (double)COORDINATE_SCALE;
    double maxY = (double)boundsQuery.value(1).toLongLong() / (double)COORDINATE_SCALE;
    double minX = (double)boundsQuery.value(2).toLongLong() / (double)COORDINATE_SCALE;
    double maxX = (double)boundsQuery.value(3).toLongLong() / (double)COORDINATE_SCALE;
    result = Envelope(minX, maxX, minY, maxY);
  }
  else
  {
    //QString error = "Error retrieving bounds of map with ID: " + mapId;
    QString error = QString("Error calculating bounds: %1").arg(boundsQuery.lastError().text());
    LOG_WARN(error);
    throw HootException(error);
  }

  return result;
}

void ServicesDb::_checkLastMapId(long mapId)
{
  if (_lastMapId != mapId)
  {
    _flushBulkInserts();
    _resetQueries();
    _nodeIdReserver.reset();
    _wayIdReserver.reset();
    _relationIdReserver.reset();
    _lastMapId = mapId;
  }
}

void ServicesDb::close()
{
  createPendingMapIndexes();
  _flushBulkInserts();

  _resetQueries();

  if (_inTransaction)
  {
    LOG_WARN("Closing database before transaction is committed. Rolling back transaction.");
    rollback();
  }

  // Seeing this? "Unable to free statement: connection pointer is NULL"
  // Make sure all queries are listed in _resetQueries.
  _db.close();
}

void ServicesDb::closeChangeSet(long mapId, long changeSetId, Envelope env, int numChanges)
{
  if (!changesetExists(mapId, changeSetId))
  {
    throw HootException("No changeset exists with ID: " + changeSetId);
  }

  _checkLastMapId(mapId);
  if (_closeChangeSet == 0)
  {
    _closeChangeSet.reset(new QSqlQuery(_db));
    _closeChangeSet->prepare(
      QString("UPDATE %1 SET min_lat=:min_lat, max_lat=:max_lat, min_lon=:min_lon, "
        "max_lon=:max_lon, closed_at=NOW(), num_changes=:num_changes WHERE id=:id")
         .arg(_getChangesetsTableName(mapId)));
  }
  _closeChangeSet->bindValue(":min_lat", env.getMinY());
  _closeChangeSet->bindValue(":max_lat", env.getMaxY());
  _closeChangeSet->bindValue(":min_lon", env.getMinX());
  _closeChangeSet->bindValue(":max_lon", env.getMaxX());
  _closeChangeSet->bindValue(":num_changes", numChanges);
  _closeChangeSet->bindValue(":id", (qlonglong)changeSetId);

  if (_closeChangeSet->exec() == false)
  {
    LOG_ERROR("query bound values: ");
    LOG_ERROR(_closeChangeSet->boundValues());
    LOG_ERROR("\n");
    throw HootException("Error executing close changeset: " + _closeChangeSet->lastError().text() +
                        " (SQL: " + _closeChangeSet->executedQuery() + ")" + " with envelope: " +
                        QString::fromStdString(env.toString()));
  }
}

void ServicesDb::commit()
{
  createPendingMapIndexes();
  _flushBulkInserts();
  _resetQueries();
  if (!_db.commit())
  {
    LOG_WARN("Error committing transaction.");
    throw HootException("Error committing transaction: " + _db.lastError().text());
  }
  _inTransaction = false;
}

void ServicesDb::_copyTableStructure(QString from, QString to)
{
  // inserting strings in this fashion is safe b/c it is private and we closely control the table
  // names.
  QString sql = QString("CREATE TABLE %1 (LIKE %2 INCLUDING DEFAULTS INCLUDING CONSTRAINTS "
      "INCLUDING INDEXES)").arg(to).arg(from);
  QSqlQuery q(_db);

  LOG_VARD(sql);

  if (q.exec(sql) == false)
  {
    QString error = QString("Error executing query: %1 (%2)").arg(q.lastError().text()).
        arg(sql);
    LOG_WARN(error);
    throw HootException(error);
  }
}

void ServicesDb::createPendingMapIndexes()
{
  for (int i = 0; i < _pendingMapIndexes.size(); i++)
  {
    long mapId = _pendingMapIndexes[i];

    _execNoPrepare(QString("ALTER TABLE %1 "
      "ADD CONSTRAINT current_nodes_changeset_id_fkey_%2 FOREIGN KEY (changeset_id) "
        "REFERENCES %3 (id) MATCH SIMPLE "
        "ON UPDATE NO ACTION ON DELETE NO ACTION ")
        .arg(_getNodesTableName(mapId))
        .arg(_getMapIdString(mapId))
        .arg(_getChangesetsTableName(mapId)));

    _execNoPrepare(QString("CREATE INDEX %1_tile_idx ON %2 USING btree (tile)")
        .arg(_getNodesTableName(mapId))
        .arg(_getNodesTableName(mapId)));

    _execNoPrepare(QString("ALTER TABLE %1 "
      "ADD CONSTRAINT current_relations_changeset_id_fkey_%2 FOREIGN KEY (changeset_id) "
        "REFERENCES %3 (id) MATCH SIMPLE "
        "ON UPDATE NO ACTION ON DELETE NO ACTION ")
        .arg(_getRelationsTableName(mapId))
        .arg(_getMapIdString(mapId))
        .arg(_getChangesetsTableName(mapId)));

    _execNoPrepare(QString("ALTER TABLE %1 "
      "ADD CONSTRAINT current_way_nodes_node_id_fkey_%2 FOREIGN KEY (node_id) "
        "REFERENCES %3 (id) MATCH SIMPLE "
        "ON UPDATE NO ACTION ON DELETE NO ACTION, "
      "ADD CONSTRAINT current_way_nodes_way_id_fkey_%2 FOREIGN KEY (way_id) "
        "REFERENCES %4 (id) MATCH SIMPLE "
        "ON UPDATE NO ACTION ON DELETE NO ACTION")
        .arg(_getWayNodesTableName(mapId))
        .arg(_getMapIdString(mapId))
        .arg(_getNodesTableName(mapId))
        .arg(_getWaysTableName(mapId)));

    _execNoPrepare(QString("ALTER TABLE %1 "
      "ADD CONSTRAINT current_ways_changeset_id_fkey_%2 FOREIGN KEY (changeset_id) "
        "REFERENCES %3 (id) MATCH SIMPLE "
        "ON UPDATE NO ACTION ON DELETE NO ACTION ")
        .arg(_getWaysTableName(mapId))
        .arg(_getMapIdString(mapId))
        .arg(_getChangesetsTableName(mapId)));
  }

  _pendingMapIndexes.clear();
}

void ServicesDb::deleteMap(long mapId)
{
  _dropTable(_getRelationMembersTableName(mapId));
  _dropTable(_getRelationsTableName(mapId));
  _dropTable(_getWayNodesTableName(mapId));
  _dropTable(_getWaysTableName(mapId));
  _dropTable(_getNodesTableName(mapId));
  _dropTable(_getChangesetsTableName(mapId));

  _execNoPrepare("DROP SEQUENCE IF EXISTS " + _getNodeSequenceName(mapId) + " CASCADE");
  _execNoPrepare("DROP SEQUENCE IF EXISTS " + _getWaySequenceName(mapId) + " CASCADE");
  _execNoPrepare("DROP SEQUENCE IF EXISTS " + _getRelationSequenceName(mapId) + " CASCADE");

  _exec("DELETE FROM maps WHERE id=:id", (qlonglong)mapId);
}

void ServicesDb::_dropTable(QString tableName)
{
  // inserting strings in this fashion is safe b/c it is private and we closely control the table
  // names.
  QString sql = QString("DROP TABLE IF EXISTS %1").arg(tableName);
  QSqlQuery q(_db);

  if (q.exec(sql) == false)
  {
    QString error = QString("Error executing query: %1 (%2)").arg(q.lastError().text()).
        arg(sql);
    LOG_WARN(error);
    throw HootException(error);
  }
}

void ServicesDb::deleteUser(long userId)
{
  QSqlQuery maps = _exec("SELECT id FROM maps WHERE user_id=:user_id", (qlonglong)userId);

  // delete all the maps owned by this user
  while (maps.next())
  {
    long mapId = maps.value(0).toLongLong();
    deleteMap(mapId);
  }

  _exec("DELETE FROM users WHERE id=:id", (qlonglong)userId);
}

QSqlQuery ServicesDb::_exec(QString sql, QVariant v1, QVariant v2, QVariant v3) const
{
  QSqlQuery q(_db);

  LOG_VARD(sql);

  if (q.prepare(sql) == false)
  {
    throw HootException(QString("Error preparing query: %1 (%2)").arg(q.lastError().text()).
                        arg(sql));
  }

  if (v1.isValid())
  {
    q.bindValue(0, v1);
  }
  if (v2.isValid())
  {
    q.bindValue(1, v2);
  }
  if (v3.isValid())
  {
    q.bindValue(2, v3);
  }

  if (q.exec() == false)
  {
    throw HootException(QString("Error executing query: %1 (%2)").arg(q.lastError().text()).
                        arg(sql));
  }

  return q;
}

QSqlQuery ServicesDb::_execNoPrepare(QString sql) const
{
  // inserting strings in this fashion is safe b/c it is private and we closely control the table
  // names.
  QSqlQuery q(_db);

  LOG_VARD(sql);

  if (q.exec(sql) == false)
  {
    QString error = QString("Error executing query: %1 (%2)").arg(q.lastError().text()).
        arg(sql);
    LOG_WARN(error);
    throw HootException(error);
  }

  return q;
}

QString ServicesDb::_escapeIds(const vector<long>& v) const
{
  QString str;
  str.reserve(v.size() * 6);
  QString comma(",");

  str.append("{");
  for (size_t i = 0; i < v.size(); i++)
  {
    if (i != 0)
    {
      str.append(comma);
    }
    str.append(QString::number(v[i]));
  }
  str.append("}");

  return str;
}

QString ServicesDb::_escapeTags(const Tags& tags) const
{
  QStringList l;
  static QChar f1('\\'), f2('"'), f3('\'');
  static QChar to('_');

  for (Tags::const_iterator it = tags.begin(); it != tags.end(); ++it)
  {
    // this doesn't appear to be working, but I think it is implementing the spec as described here:
    // http://www.postgresql.org/docs/9.0/static/hstore.html
    // The spec described above does seem to work on the psql command line. Curious.
    QString k = QString(it.key()).replace(f1, "\\\\").replace(f2, "\\\"");
    QString v = QString(it.value()).replace(f1, "\\\\").replace(f2, "\\\"");

    l << QString("\"%1\"=>\"%2\"").arg(k).arg(v);
  }
  return l.join(",");
}

QString ServicesDb::_execToString(QString sql, QVariant v1, QVariant v2, QVariant v3)
{
  QSqlQuery q = _exec(sql, v1, v2, v3);

  QStringList l;
  while (q.next())
  {
    QStringList row;
    for (int i = 0; i < q.record().count(); ++i)
    {
      row.append(q.value(i).toString());
    }
    l.append(row.join(";"));
  }

  q.finish();

  return l.join("\n");
}

void ServicesDb::_flushBulkInserts()
{
  if (_nodeBulkInsert != 0)
  {
    _nodeBulkInsert->flush();
  }
  if (_wayBulkInsert != 0)
  {
    _wayBulkInsert->flush();
  }
  if (_wayNodeBulkInsert != 0)
  {
    _wayNodeBulkInsert->flush();
  }
  if (_relationBulkInsert != 0)
  {
    _relationBulkInsert->flush();
  }
}

QString ServicesDb::getDbVersion()
{
  if (_selectDbVersion == 0)
  {
    _selectDbVersion.reset(new QSqlQuery(_db));
    _selectDbVersion->prepare("SELECT id || ':' || author AS version_id FROM databasechangelog "
                             "ORDER BY dateexecuted DESC LIMIT 1");
  }

  if (_selectDbVersion->exec() == false)
  {
    throw HootException(_selectDbVersion->lastError().text());
  }

  QString result;
  if (_selectDbVersion->next())
  {
    result = _selectDbVersion->value(0).toString();
  }
  else
  {
    throw HootException("Unable to retrieve the DB version.");
  }

  return result;
}

long ServicesDb::_getNextNodeId(long mapId)
{
  _checkLastMapId(mapId);
  if (_nodeIdReserver == 0)
  {
    _nodeIdReserver.reset(new InternalIdReserver(_db, _getNodeSequenceName(mapId)));
  }
  return _nodeIdReserver->getNextId();
}

long ServicesDb::_getNextRelationId(long mapId)
{
  _checkLastMapId(mapId);
  if (_relationIdReserver == 0)
  {
    _relationIdReserver.reset(new InternalIdReserver(_db, _getRelationSequenceName(mapId)));
  }
  return _relationIdReserver->getNextId();
}

long ServicesDb::_getNextWayId(long mapId)
{
  _checkLastMapId(mapId);
  if (_wayIdReserver == 0)
  {
    _wayIdReserver.reset(new InternalIdReserver(_db, _getWaySequenceName(mapId)));
  }
  return _wayIdReserver->getNextId();
}

bool ServicesDb::_hasTable(QString tableName)
{
  QString sql = "SELECT 1 from pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n ON "
      "n.oid = c.relnamespace WHERE c.relname = :name";
  QSqlQuery q = _exec(sql, tableName);

  return q.next();
}

void ServicesDb::_init()
{
  _inTransaction = false;

  int recordsPerBulkInsert = 500;

  // set it to something obsurd.
  _lastMapId = -numeric_limits<long>::max();

  _nodesInsertElapsed = 0;
  // 500 found experimentally on my desktop -JRS
  _nodesPerBulkInsert = recordsPerBulkInsert;

  _wayNodesInsertElapsed = 0;
  // arbitrary, needs benchmarking
  _wayNodesPerBulkInsert = recordsPerBulkInsert;

  _wayInsertElapsed = 0;
  // arbitrary, needs benchmarking
  _waysPerBulkInsert = recordsPerBulkInsert;

  // arbitrary, needs benchmarking
  _relationsPerBulkInsert = recordsPerBulkInsert;
}

long ServicesDb::insertChangeSet(long mapId, long userId, const Tags& tags,
  geos::geom::Envelope env)
{
  _checkLastMapId(mapId);
  if (_insertChangeSet == 0)
  {
    _insertChangeSet.reset(new QSqlQuery(_db));
    _insertChangeSet->prepare(
      QString("INSERT INTO %1 (user_id, created_at, min_lat, max_lat, min_lon, max_lon, "
          "closed_at, tags) "
      "VALUES (:user_id, NOW(), :min_lat, :max_lat, :min_lon, :max_lon, NOW(), :tags) "
      "RETURNING id")
        .arg(_getChangesetsTableName(mapId)));
  }
  _insertChangeSet->bindValue(":user_id", (qlonglong)userId);
  _insertChangeSet->bindValue(":min_lat", env.getMinY());
  _insertChangeSet->bindValue(":max_lat", env.getMaxY());
  _insertChangeSet->bindValue(":min_lon", env.getMinX());
  _insertChangeSet->bindValue(":max_lon", env.getMaxX());
  _insertChangeSet->bindValue(":tags", _escapeTags(tags));

  return _insertRecord(*_insertChangeSet);
}

long ServicesDb::insertMap(QString displayName, int userId, bool publicVisibility)
{
  if (_insertMap == 0)
  {
    _insertMap.reset(new QSqlQuery(_db));
    _insertMap->prepare("INSERT INTO maps (display_name, user_id, public, created_at) "
                       "VALUES (:display_name, :user_id, :public, NOW()) "
                       "RETURNING id");
  }
  _insertMap->bindValue(":display_name", displayName);
  _insertMap->bindValue(":user_id", userId);
  _insertMap->bindValue(":public", publicVisibility);

  long mapId = _insertRecord(*_insertMap);

  QString mapIdStr = _getMapIdString(mapId);
  _copyTableStructure("changesets", _getChangesetsTableName(mapId));
  _copyTableStructure("current_nodes", "current_nodes" + mapIdStr);
  _copyTableStructure("current_relation_members", "current_relation_members" + mapIdStr);
  _copyTableStructure("current_relations", "current_relations" + mapIdStr);
  _copyTableStructure("current_way_nodes", "current_way_nodes" + mapIdStr);
  _copyTableStructure("current_ways", "current_ways" + mapIdStr);

  _execNoPrepare("CREATE SEQUENCE " + _getNodeSequenceName(mapId));
  _execNoPrepare("CREATE SEQUENCE " + _getRelationSequenceName(mapId));
  _execNoPrepare("CREATE SEQUENCE " + _getWaySequenceName(mapId));

  _execNoPrepare(QString("ALTER TABLE %1 "
    "ALTER COLUMN id SET DEFAULT NEXTVAL('%4'::regclass)")
      .arg(_getNodesTableName(mapId))
      .arg(_getNodeSequenceName(mapId)));

  _execNoPrepare(QString("ALTER TABLE %1 "
    "ALTER COLUMN id SET DEFAULT NEXTVAL('%4'::regclass)")
      .arg(_getRelationsTableName(mapId))
      .arg(_getRelationSequenceName(mapId)));

  _execNoPrepare(QString("ALTER TABLE %1 "
    "ALTER COLUMN id SET DEFAULT NEXTVAL('%4'::regclass)")
      .arg(_getWaysTableName(mapId))
      .arg(_getWaySequenceName(mapId)));

  // remove the index to speed up inserts. It'll be added back by createPendingMapIndexes
  _execNoPrepare(QString("DROP INDEX %1_tile_idx")
      .arg(_getNodesTableName(mapId)));

  _pendingMapIndexes.append(mapId);

  return mapId;
}

long ServicesDb::insertNode(long mapId, long id, double lat, double lon, long changeSetId,
  const Tags& tags, bool createNewId)
{
  double start = Tgs::Time::getTime();

  _checkLastMapId(mapId);

  if (_nodeBulkInsert == 0)
  {
    QStringList columns;
    columns << "id" << "latitude" << "longitude" << "changeset_id" <<
               "tile" << "tags";

    _nodeBulkInsert.reset(new SqlBulkInsert(_db, _getNodesTableName(mapId), columns));
  }

  if (createNewId)
  {
    id = _getNextNodeId(mapId);
  }

  QList<QVariant> v;
  v.append((qlonglong)id);
  v.append((qlonglong)_round(lat * COORDINATE_SCALE, 7));
  v.append((qlonglong)_round(lon * COORDINATE_SCALE, 7));
  v.append((qlonglong)changeSetId);
  v.append(_tileForPoint(lat, lon));
  // escaping tags ensures that we won't introduce a SQL injection vulnerability, however, if a
  // bad tag is passed and it isn't escaped properly (shouldn't happen) it may result in a syntax
  // error.
  v.append(_escapeTags(tags));

  _nodeBulkInsert->insert(v);

  _nodesInsertElapsed += Tgs::Time::getTime() - start;

  if (_nodeBulkInsert->getPendingCount() >= _nodesPerBulkInsert)
  {
    _nodeBulkInsert->flush();
  }

  return id;
}

long ServicesDb::insertRelation(long mapId, long relationId, long changeSetId, const Tags &tags,
  bool createNewId)
{
  _checkLastMapId(mapId);

  if (_relationBulkInsert == 0)
  {
    QStringList columns;
    columns << "id" << "changeset_id" << "tags";

    _relationBulkInsert.reset(new SqlBulkInsert(_db, _getRelationsTableName(mapId), columns));
  }

  if (createNewId)
  {
    relationId = _getNextRelationId(mapId);
  }

  QList<QVariant> v;
  v.append((qlonglong)relationId);
  v.append((qlonglong)changeSetId);
  // escaping tags ensures that we won't introduce a SQL injection vulnerability, however, if a
  // bad tag is passed and it isn't escaped properly (shouldn't happen) it may result in a syntax
  // error.
  v.append(_escapeTags(tags));

  _relationBulkInsert->insert(v);

  _lazyFlushBulkInsert();

  return relationId;
}

void ServicesDb::insertRelationMembers(long mapId, long relationId, ElementType type,
  long elementId, QString role, int sequenceId)
{
  _checkLastMapId(mapId);

  if (_insertRelationMembers == 0)
  {
    _insertRelationMembers.reset(new QSqlQuery(_db));
    _insertRelationMembers->prepare(
      "INSERT INTO " + _getRelationMembersTableName(mapId) +
        " (relation_id, member_type, member_id, member_role, sequence_id) "
      "VALUES (:relation_id, :member_type, :member_id, :member_role, :sequence_id)");
  }

  _insertRelationMembers->bindValue(":relation_id", (qlonglong)relationId);
  _insertRelationMembers->bindValue(":member_type", type.toString().toLower());
  _insertRelationMembers->bindValue(":member_id", (qlonglong)elementId);
  _insertRelationMembers->bindValue(":member_role", role);
  _insertRelationMembers->bindValue(":sequence_id", sequenceId);

  if (!_insertRelationMembers->exec())
  {
    throw HootException("Error inserting relation memeber: " +
      _insertRelationMembers->lastError().text());
  }
}

long ServicesDb::insertUser(QString email, QString displayName)
{
  if (_insertUser == 0)
  {
    _insertUser.reset(new QSqlQuery(_db));
    _insertUser->prepare("INSERT INTO users (email, display_name) "
                        "VALUES (:email, :display_name) "
                        "RETURNING id");
  }
  _insertUser->bindValue(":email", email);
  _insertUser->bindValue(":display_name", displayName);

  long id = -1;
  // if we failed to execute the query the first time
  if (_insertUser->exec() == false)
  {
    // it may be that another process beat us to it and the user was already inserted. This can
    // happen if a bunch of converts are run in parallel. See #3588
    id = getUserId(email, false);

    // nope, there is something else wrong. Report an error.
    if (id == -1)
    {
      QString err = QString("Error executing query: %1 (%2)").arg(_insertUser->executedQuery()).
          arg(_insertUser->lastError().text());
      LOG_WARN(err)
      throw HootException(err);
    }
    else
    {
      LOG_DEBUG("Did not insert user, queryied a previously created user.")
    }
  }
  // if the insert succeeded
  else
  {
    bool ok = false;
    if (_insertUser->next())
    {
      id = _insertUser->value(0).toLongLong(&ok);
    }

    if (!ok || id == -1)
    {
      LOG_ERROR("query bound values: ");
      LOG_ERROR(_insertUser->boundValues());
      LOG_ERROR("\n");
      throw HootException("Error retrieving new ID " + _insertUser->lastError().text() + " Query: " +
        _insertUser->executedQuery());
    }

    _insertUser->finish();
  }

  return id;
}

long ServicesDb::insertWay(long mapId, long wayId, long changeSetId, const Tags& tags,
                           /*const vector<long>& nids,*/ bool createNewId)
{
  double start = Tgs::Time::getTime();

  _checkLastMapId(mapId);

  if (_wayBulkInsert == 0)
  {
    QStringList columns;
    columns << "id" << "changeset_id" << "tags";

    _wayBulkInsert.reset(new SqlBulkInsert(_db, _getWaysTableName(mapId), columns));
  }

  if (createNewId)
  {
    wayId = _getNextWayId(mapId);
  }

  QList<QVariant> v;
  v.append((qlonglong)wayId);
  v.append((qlonglong)changeSetId);
  // escaping tags ensures that we won't introduce a SQL injection vulnerability, however, if a
  // bad tag is passed and it isn't escaped properly (shouldn't happen) it may result in a syntax
  // error.
  v.append(_escapeTags(tags));
  //v.append(_escapeIds(nids));

  _wayBulkInsert->insert(v);

  _wayNodesInsertElapsed += Tgs::Time::getTime() - start;

  _lazyFlushBulkInsert();

  return wayId;
}

void ServicesDb::insertWayNodes(long mapId, long wayId, const vector<long>& nodeIds)
{
  double start = Tgs::Time::getTime();

  _checkLastMapId(mapId);

  if (_wayNodeBulkInsert == 0)
  {
    QStringList columns;
    columns << "way_id" << "node_id" << "sequence_id";

    _wayNodeBulkInsert.reset(new SqlBulkInsert(_db, _getWayNodesTableName(mapId), columns));
  }

  QList<QVariant> v;
  v.append((qlonglong)wayId);
  v.append((qlonglong)0);
  v.append((qlonglong)0);

  for (size_t i = 0; i < nodeIds.size(); ++i)
  {
    v[1] = (qlonglong)nodeIds[i];
    v[2] = (qlonglong)i;
    _wayNodeBulkInsert->insert(v);
  }

  _wayNodesInsertElapsed += Tgs::Time::getTime() - start;

  _lazyFlushBulkInsert();
}

long ServicesDb::getOrCreateUser(QString email, QString displayName)
{
  long result = getUserId(email, false);

  if (result == -1)
  {
    result = insertUser(email, displayName);
  }

  return result;
}

long ServicesDb::getUserId(QString email, bool throwWhenMissing)
{
  if (_selectUserByEmail == 0)
  {
    _selectUserByEmail.reset(new QSqlQuery(_db));
    _selectUserByEmail->prepare("SELECT email, id, display_name FROM users WHERE email LIKE :email");
  }
  _selectUserByEmail->bindValue(":email", email);
  if (_selectUserByEmail->exec() == false)
  {
    throw HootException("Error finding user with the email: " + email + " " +
                        _selectUserByEmail->lastError().text());
  }

  long result = -1;
  if (_selectUserByEmail->next())
  {
    bool ok;
    result = _selectUserByEmail->value(1).toLongLong(&ok);
    if (!ok && throwWhenMissing)
    {
      throw HootException("Specified user was not found.");
    }
  }
  else if (throwWhenMissing)
  {
    QString error = QString("No user found with the email: %1 (maybe specify `%2=true`?)")
        .arg(email).arg(ConfigOptions::getServicesDbWriterCreateUserKey());
    LOG_WARN(error);
    throw HootException(error);
  }

  _selectUserByEmail->finish();

  return result;
}

long ServicesDb::_insertRecord(QSqlQuery& query)
{
  if (query.exec() == false)
  {
    QString err = QString("Error executing query: %1 (%2)").arg(query.executedQuery()).
        arg(query.lastError().text());
    LOG_WARN(err)
    throw HootException(err);
  }
  bool ok = false;
  long id = -1;
  if (query.next())
  {
    id = query.value(0).toLongLong(&ok);
  }

  if (!ok || id == -1)
  {
    LOG_ERROR("query bound values: ");
    LOG_ERROR(query.boundValues());
    LOG_ERROR("\n");
    throw HootException("Error retrieving new ID " + query.lastError().text() + " Query: " +
      query.executedQuery());
  }

  query.finish();

  return id;
}

bool ServicesDb::isSupported(QUrl url)
{
  bool valid = url.isValid();
  valid = valid && url.scheme() == "postgresql";

  if (valid)
  {
    QString path = url.path();
    QStringList plist = path.split("/");

    if (plist.size() != 3 || (plist.size() == 4 && plist[3] == ""))
    {
      LOG_WARN("Looks like a DB path, but a DB name and layer was expected. E.g. "
               "postgresql://myhost:5432/mydb/mylayer");
      valid = false;
    }
    else if (plist[1] == "")
    {
      LOG_WARN("Looks like a DB path, but a DB name was expected. E.g. "
               "postgresql://myhost:5432/mydb/mylayer");
      valid = false;
    }
    else if (plist[2] == "")
    {
      LOG_WARN("Looks like a DB path, but a layer name was expected. E.g. "
               "postgresql://myhost:5432/mydb/mylayer");
      valid = false;
    }
  }

  return valid;
}

void ServicesDb::_lazyFlushBulkInsert()
{
  bool flush = false;

  if (_nodeBulkInsert && _nodeBulkInsert->getPendingCount() > _nodesPerBulkInsert)
  {
    flush = true;
  }
  if (_wayNodeBulkInsert && _wayNodeBulkInsert->getPendingCount() > _wayNodesPerBulkInsert)
  {
    flush = true;
  }
  if (_wayBulkInsert && _wayBulkInsert->getPendingCount() > _waysPerBulkInsert)
  {
    flush = true;
  }
  if (_relationBulkInsert && _relationBulkInsert->getPendingCount() > _relationsPerBulkInsert)
  {
    flush = true;
  }

  if (flush)
  {
    _flushBulkInserts();
  }
}

void ServicesDb::open(QUrl url)
{
  if (!isSupported(url))
  {
    throw HootException("An unsupported URL was passed in.");
  }

  QStringList pList = url.path().split("/");
  QString db = pList[1];

  QString connectionName = url.toString() + " 0x" + QString::number((qulonglong)this, 16);
  if (QSqlDatabase::contains(connectionName) == false)
  {
    _db = QSqlDatabase::addDatabase("QPSQL", connectionName);
  }
  else
  {
    _db = QSqlDatabase::database(connectionName);
  }

  if (_db.isOpen() == false)
  {
    _db.setDatabaseName(db);
    if (url.host() == "local")
    {
      _db.setHostName("/var/run/postgresql");
    }
    else
    {
      _db.setHostName(url.host());
    }
    _db.setPort(url.port());
    _db.setUserName(url.userName());
    _db.setPassword(url.password());

    if (_db.open() == false)
    {
      throw HootException("Error opening database: " + _db.lastError().text());
    }
  }

  if (_db.tables().size() == 0)
  {
    throw HootException("Attempting to open database " + url.toString() +
                        " but found zero tables. Does the DB exist? Has it been populated?");
  }

  _resetQueries();

  if (!isCorrectDbVersion())
  {
    LOG_WARN("Running against an unexpected DB version.");
    LOG_WARN("Expected: " << expectedDbVersion());
    LOG_WARN("Actual: " << getDbVersion());
  }

  QSqlQuery query("SET client_min_messages TO WARNING", _db);
  // if there was an error
  if (query.lastError().isValid())
  {
    LOG_WARN("Error disabling Postgresql INFO messages.");
  }
}

void ServicesDb::_resetQueries()
{
  _closeChangeSet.reset();
  _insertChangeSet.reset();
  _insertChangeSetTag.reset();
  _insertMap.reset();
  _insertUser.reset();
  _insertWayNodes.reset();
  _insertRelationMembers.reset();
  _selectDbVersion.reset();
  _selectUserByEmail.reset();
  _mapExists.reset();
  _changesetExists.reset();
  _numTypeElementsForMap.reset();
  _selectElementsForMap.reset();
  _selectReserveNodeIds.reset();
  _selectNodeIdsForWay.reset();
  _selectMapIds.reset();
  _selectMembersForRelation.reset();
  _updateNode.reset();
  _updateRelation.reset();
  _updateWay.reset();

  // bulk insert objects.
  _nodeBulkInsert.reset();
  _nodeIdReserver.reset();
  _relationBulkInsert.reset();
  _relationIdReserver.reset();
  _wayNodeBulkInsert.reset();
  _wayBulkInsert.reset();
  _wayIdReserver.reset();
}

void ServicesDb::rollback()
{
  _resetQueries();

  if (!_db.rollback())
  {
    LOG_WARN("Error rolling back transaction.");
    throw HootException("Error rolling back transaction: " + _db.lastError().text());
  }

  _inTransaction = false;
}

long ServicesDb::_round(double x)
{
  return (long)(x + 0.5);
}

long ServicesDb::_round(double x, int precision)
{
  return (long)(floor(x * (10 * (precision - 1)) + 0.5) / (10 * (precision - 1)));
  //return (long)(ceil(x * (10 * (precision - 1))) / (10 * (precision - 1)));
}

set<long> ServicesDb::selectMapIds(QString name, long userId)
{
  if (_selectMapIds == 0)
  {
    _selectMapIds.reset(new QSqlQuery(_db));
    _selectMapIds->prepare("SELECT id FROM maps WHERE display_name LIKE :name AND user_id=:userId");
  }

  _selectMapIds->bindValue(":name", name);
  _selectMapIds->bindValue(":user_id", (qlonglong)userId);

  if (_selectMapIds->exec() == false)
  {
    throw HootException(_selectMapIds->lastError().text());
  }

  set<long> result;
  while (_selectMapIds->next())
  {
    bool ok;
    long id = _selectMapIds->value(0).toLongLong(&ok);
    if (!ok)
    {
      throw HootException("Error parsing map ID.");
    }
    result.insert(id);
  }

  return result;
}

unsigned int ServicesDb::_tileForPoint(double lat, double lon)
{
  int lonInt = _round((lon + 180.0) * 65535.0 / 360.0);
  int latInt = _round((lat + 90.0) * 65535.0 / 180.0);

  unsigned int tile = 0;
  int          i;

  for (i = 15; i >= 0; i--)
  {
    tile = (tile << 1) | ((lonInt >> i) & 1);
    tile = (tile << 1) | ((latInt >> i) & 1);
  }

  return tile;
}

void ServicesDb::transaction()
{
  // Queries must be created from within the current transaction.
  _resetQueries();
  if (!_db.transaction())
  {
    throw HootException(_db.lastError().text());
  }
  _inTransaction = true;
}

//using some kind of SQL generator (if one exists for C++ would prevent us from having to do
//hardcoded table and column references and keep this code less brittle...

QString ServicesDb::_elementTypeToElementTableName(long mapId, const ElementType& elementType) const
{
  if (elementType == ElementType::Node)
  {
    return _getNodesTableName(mapId);
  }
  else if (elementType == ElementType::Way)
  {
    return _getWaysTableName(mapId);
  }
  else if (elementType == ElementType::Relation)
  {
    return _getRelationsTableName(mapId);
  }
  else
  {
    throw HootException("Unsupported element type.");
  }
}

//TODO: consolidate these exists queries into a single method

bool ServicesDb::mapExists(const long id)
{
  if (_mapExists == 0)
  {
    _mapExists.reset(new QSqlQuery(_db));
    _mapExists->prepare("SELECT display_name FROM maps WHERE id = :mapId");
  }
  _mapExists->bindValue(":mapId", (qlonglong)id);
  if (_mapExists->exec() == false)
  {
    throw HootException(_mapExists->lastError().text());
  }

  return _mapExists->next();
}

bool ServicesDb::changesetExists(long mapId, const long id)
{
  _checkLastMapId(mapId);
  if (_changesetExists == 0)
  {
    _changesetExists.reset(new QSqlQuery(_db));
    _changesetExists->prepare(QString("SELECT num_changes FROM %1 WHERE id = :changesetId")
      .arg(_getChangesetsTableName(mapId)));
  }
  _changesetExists->bindValue(":changesetId", (qlonglong)id);
  if (_changesetExists->exec() == false)
  {
    throw HootException(_changesetExists->lastError().text());
  }

  return _changesetExists->next();
}

long ServicesDb::numElements(const long mapId, const ElementType& elementType)
{
  _numTypeElementsForMap.reset(new QSqlQuery(_db));
  _numTypeElementsForMap->prepare(
    "SELECT COUNT(*) FROM " + _elementTypeToElementTableName(mapId, elementType));
  if (_numTypeElementsForMap->exec() == false)
  {
    LOG_ERROR(_numTypeElementsForMap->executedQuery());
    LOG_ERROR(_numTypeElementsForMap->lastError().text());
    throw HootException(_numTypeElementsForMap->lastError().text());
  }

  long result = -1;
  if (_numTypeElementsForMap->next())
  {
    bool ok;
    result = _numTypeElementsForMap->value(0).toLongLong(&ok);
    if (!ok)
    {
      throw HootException("Count not retrieve count for element type: " + elementType.toString());
    }
  }
  _numTypeElementsForMap->finish();
  return result;
}

shared_ptr<QSqlQuery> ServicesDb::selectAllElements(const long mapId, const long elementId, const ElementType& elementType)
{
  return selectElements(mapId, elementId, elementType, -1, 0);
}

shared_ptr<QSqlQuery> ServicesDb::selectAllElements(const long mapId, const ElementType& elementType)
{
  return selectElements(mapId, -1, elementType, -1, 0);
}



shared_ptr<QSqlQuery> ServicesDb::selectElements(const long mapId, const long elementId,
  const ElementType& elementType, const long limit, const long offset)
{
  _selectElementsForMap.reset(new QSqlQuery(_db));
  _selectElementsForMap->setForwardOnly(true);
  QString limitStr;
  if (limit == -1)
  {
    limitStr = "ALL";
  }
  else
  {
    limitStr = QString::number(limit);
  }

  QString sql =  "SELECT * FROM " + _elementTypeToElementTableName(mapId, elementType);

  if(elementId > -1)
  {
    sql += " WHERE id = :elementId ";
  }
  sql += " ORDER BY id LIMIT " + limitStr + " OFFSET " + QString::number(offset);
  _selectElementsForMap->prepare(sql);
  _selectElementsForMap->bindValue(":mapId", (qlonglong)mapId);
  if(elementId > -1)
  {
    _selectElementsForMap->bindValue(":elementId", (qlonglong)elementId);
  }

  if (_selectElementsForMap->exec() == false)
  {
    QString err = _selectElementsForMap->lastError().text();
    LOG_WARN(sql);
    throw HootException("Error selecting elements of type: " + elementType.toString() +
      " for map ID: " + QString::number(mapId) + " Error: " + err);
  }
  return _selectElementsForMap;
}

vector<long> ServicesDb::selectNodeIdsForWay(long mapId, long wayId)
{
  vector<long> result;

  _checkLastMapId(mapId);

  if (!_selectNodeIdsForWay)
  {
    _selectNodeIdsForWay.reset(new QSqlQuery(_db));
    _selectNodeIdsForWay->setForwardOnly(true);
    _selectNodeIdsForWay->prepare(
      "SELECT node_id FROM " + _getWayNodesTableName(mapId) +
          " WHERE way_id = :wayId ORDER BY sequence_id");
  }
  _selectNodeIdsForWay->bindValue(":wayId", (qlonglong)wayId);
  if (_selectNodeIdsForWay->exec() == false)
  {
    throw HootException("Error selecting node ID's for way with ID: " + QString::number(wayId) +
      " Error: " + _selectNodeIdsForWay->lastError().text());
  }

  while (_selectNodeIdsForWay->next())
  {
    bool ok;
    result.push_back(_selectNodeIdsForWay->value(0).toLongLong(&ok));

    if (!ok)
    {
      QString err = QString("Error converting node ID to long. (%1)").
          arg(_selectNodeIdsForWay->value(0).toString());
      throw HootException(err);
    }
  }

  return result;
}

vector<RelationData::Entry> ServicesDb::selectMembersForRelation(long mapId, long relationId)
{
  vector<RelationData::Entry> result;

  if (!_selectMembersForRelation)
  {
    _selectMembersForRelation.reset(new QSqlQuery(_db));
    _selectMembersForRelation->setForwardOnly(true);
#warning fix me.
    _selectMembersForRelation->prepare(
      "SELECT member_type, member_id, member_role FROM " + _getRelationMembersTableName(mapId) +
      " WHERE relation_id = :relationId ORDER BY sequence_id");
  }

  _selectMembersForRelation->bindValue(":mapId", (qlonglong)mapId);
  _selectMembersForRelation->bindValue(":relationId", (qlonglong)relationId);
  if (_selectMembersForRelation->exec() == false)
  {
    throw HootException("Error selecting members for relation with ID: " +
      QString::number(relationId) + " Error: " + _selectMembersForRelation->lastError().text());
  }

  while (_selectMembersForRelation->next())
  {
    const QString memberType = _selectMembersForRelation->value(0).toString();
    if (ElementType::isValidTypeString(memberType))
    {
        result.push_back(
          RelationData::Entry(
            _selectMembersForRelation->value(2).toString(),
            ElementId(ElementType::fromString(memberType),
              _selectMembersForRelation->value(1).toLongLong())));
    }
    else
    {
        LOG_WARN("Invalid relation member type: " + memberType + ".  Skipping relation member.");
    }
  }

  return result;
}

void ServicesDb::_unescapeString(QString& s)
{
  if (s.startsWith("\""))
  {
    s.remove(0, 1);
  }
  if (s.endsWith("\""))
  {
    s.remove(s.size() - 1, 1);
  }

  s.replace("\\042", "\"");
  s.replace("\\\"", "\"");
  s.replace("\\\\", "\\");
  s.replace("\\134", "\\");
}

Tags ServicesDb::unescapeTags(const QVariant &v)
{
  assert(v.type() == QVariant::String);
  QString s = v.toString();
  // convert backslash and double quotes to their octal form so we can safely split on double quotes
  s.replace("\\\\", "\\134");
  s.replace("\\\"", "\\042");

  Tags result;
  QStringList l = s.split("\"");
  for (int i = 1; i < l.size(); i+=4)
  {
    _unescapeString(l[i]);
    _unescapeString(l[i + 2]);

    result.insert(l[i], l[i + 2]);
  }

  return result;
}

void ServicesDb::updateNode(long mapId, long id, double lat, double lon, long changeSetId,
                            const Tags& tags)
{
  _flushBulkInserts();

  _checkLastMapId(mapId);

  if (_updateNode == 0)
  {
    _updateNode.reset(new QSqlQuery(_db));
    _updateNode->prepare(
      "UPDATE " + _getNodesTableName(mapId) +
      " SET latitude=:latitude, longitude=:longitude, changeset_id=:changeset_id, tile=:tile, "
      "    tags=:tags "
      "WHERE id=:id");
  }

  _updateNode->bindValue(":id", (qlonglong)id);
  _updateNode->bindValue(":latitude", (qlonglong)_round(lat * COORDINATE_SCALE, 7));
  _updateNode->bindValue(":longitude", (qlonglong)_round(lon * COORDINATE_SCALE, 7));
  _updateNode->bindValue(":changeset_id", (qlonglong)changeSetId);
  _updateNode->bindValue(":tile", (qlonglong)_tileForPoint(lat, lon));
  _updateNode->bindValue(":tags", _escapeTags(tags));

  if (_updateNode->exec() == false)
  {
    QString err = QString("Error executing query: %1 (%2)").arg(_updateNode->executedQuery()).
        arg(_updateNode->lastError().text());
    LOG_WARN(err)
    throw HootException(err);
  }

  _updateNode->finish();
}

void ServicesDb::updateRelation(long mapId, long id, long changeSetId, const Tags& tags)
{
  _flushBulkInserts();
  _checkLastMapId(mapId);

  if (_updateRelation == 0)
  {
    _updateRelation.reset(new QSqlQuery(_db));
    _updateRelation->prepare(
      "UPDATE " + _getRelationsTableName(mapId) +
      " SET changeset_id=:changeset_id, tags=:tags "
      "WHERE id=:id");
  }

  _updateRelation->bindValue(":id", (qlonglong)id);
  _updateRelation->bindValue(":changeset_id", (qlonglong)changeSetId);
  _updateRelation->bindValue(":tags", _escapeTags(tags));

  if (_updateRelation->exec() == false)
  {
    QString err = QString("Error executing query: %1 (%2)").arg(_updateWay->executedQuery()).
        arg(_updateRelation->lastError().text());
    LOG_WARN(err)
    throw HootException(err);
  }

  _updateRelation->finish();
}

void ServicesDb::updateWay(long mapId, long id, long changeSetId, const Tags& tags)
{
  _flushBulkInserts();
  _checkLastMapId(mapId);

  if (_updateWay == 0)
  {
    _updateWay.reset(new QSqlQuery(_db));
    _updateWay->prepare(
      "UPDATE " + _getWaysTableName(mapId) +
      " SET changeset_id=:changeset_id, tags=:tags "
      "WHERE id=:id");
  }

  _updateWay->bindValue(":id", (qlonglong)id);
  _updateWay->bindValue(":changeset_id", (qlonglong)changeSetId);
  _updateWay->bindValue(":tags", _escapeTags(tags));

  if (_updateWay->exec() == false)
  {
    QString err = QString("Error executing query: %1 (%2)").arg(_updateWay->executedQuery()).
        arg(_updateWay->lastError().text());
    LOG_WARN(err)
    throw HootException(err);
  }

  _updateWay->finish();
}

}

