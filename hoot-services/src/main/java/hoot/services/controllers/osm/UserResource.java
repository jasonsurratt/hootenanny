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
package hoot.services.controllers.osm;

import java.sql.Connection;

import hoot.services.db.DbUtils;
import hoot.services.db2.QUsers;
import hoot.services.db2.Users;
import hoot.services.models.osm.ModelDaoUtils;
import hoot.services.models.osm.User;
import hoot.services.utils.ResourceErrorHandler;
import hoot.services.utils.XmlDocumentBuilder;
import hoot.services.writers.osm.UserResponseWriter;

import javax.ws.rs.Consumes;
import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.Produces;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import javax.xml.transform.dom.DOMSource;

import org.apache.commons.lang3.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.support.ClassPathXmlApplicationContext;
import org.w3c.dom.Document;

import com.mysema.query.sql.SQLQuery;

/**
 * Service endpoint for OSM user information
 */
@Path("/user/{userId}")
public class UserResource
{
  private static final Logger log = LoggerFactory.getLogger(UserResource.class);

  @SuppressWarnings("unused")
  private ClassPathXmlApplicationContext appContext;

  public UserResource()
  {
    log.debug("Reading application settings...");
    appContext = new ClassPathXmlApplicationContext(new String[] { "db/spring-database.xml" });
  }

	/**
	 * <NAME>User Service </NAME>
	 * <DESCRIPTION>
	 * 	This is currently implemented as a dummy method to appease iD.
	 *  It always retrieves information for the first user record in the services database.
	 *  It cannot properly be implemented until user authentication is first implemented.
	 * </DESCRIPTION>
	 * <PARAMETERS>
	 * </PARAMETERS>
	 * <OUTPUT>
	 * 	information about the currently logged in user in XML format
	 * </OUTPUT>
	 * <EXAMPLE>
	 * 	<URL>http://localhost:8080/hoot-services/osm/api/0.6/user/details</URL>
	 * 	<REQUEST_TYPE>GET</REQUEST_TYPE>
	 * 	<INPUT>
	 *	</INPUT>
	 * <OUTPUT>
	 * information about the currently logged in user in XML format
	 * see https://insightcloud.digitalglobe.com/redmine/projects/hootenany/wiki/User_-_OsmUserService#User
	 * </OUTPUT>
	 * </EXAMPLE>
	 *
   * Service method endpoint for retrieving OSM user information
   *
   * @param userId ID of the user to retrieve information for
   * @return Response with the requested user's information
   * @throws Exception
   * @see https://insightcloud.digitalglobe.com/redmine/projects/hootenany/wiki/User_-_OsmUserService#User
   */
  @GET
  @Consumes(MediaType.TEXT_PLAIN)
  @Produces(MediaType.TEXT_XML)
  public Response get(
    @PathParam("userId")
    final String userId)
    throws Exception
  {
    log.debug("Retrieving user with ID: " + userId.trim() + " ...");

    Connection conn = DbUtils.createConnection();
    Document responseDoc = null;
    try
    {
      log.debug("Initializing database connection...");


      long userIdNum = -1;
      try
      {
      	QUsers users = QUsers.users;
        //input mapId may be a map ID or a map name
        userIdNum =
          ModelDaoUtils.getRecordIdForInputString(userId, conn, users, users.id, users.displayName);
      }
      catch (Exception e)
      {
        if (e.getMessage().startsWith("Multiple records exist"))
        {
          ResourceErrorHandler.handleError(
            e.getMessage().replaceAll("records", "users").replaceAll("record", "user"),
            Status.NOT_FOUND,
            log);
        }
        else if (e.getMessage().startsWith("No record exists"))
        {
          ResourceErrorHandler.handleError(
            e.getMessage().replaceAll("records", "users").replaceAll("record", "user"),
            Status.NOT_FOUND,
            log);
        }
        ResourceErrorHandler.handleError(
          "Error requesting user with ID: " + userId + " (" + e.getMessage() + ")",
          Status.BAD_REQUEST,
          log);
      }

      assert(userIdNum != -1);
      QUsers usersTbl = QUsers.users;

      SQLQuery query = new SQLQuery(conn, DbUtils.getConfiguration());

      // there is only ever one test user
      Users user =  query.from(usersTbl).where(usersTbl.id.eq(userIdNum)).singleResult(usersTbl);

      if (user == null)
      {
        ResourceErrorHandler.handleError(
          "No user exists with ID: " + userId + ".  Please request a valid user.",
          Status.NOT_FOUND,
          log);
      }

      responseDoc =
        (new UserResponseWriter()).writeResponse(
          new User(user, conn), conn);
    }
    finally
    {
      DbUtils.closeConnection(conn);
    }

    log.debug("Returning response: " +
      StringUtils.abbreviate(XmlDocumentBuilder.toString(responseDoc), 100) + " ...");
    return
      Response
        .ok(new DOMSource(responseDoc), MediaType.APPLICATION_XML)
        .header("Content-type", MediaType.APPLICATION_XML)
        .build();
  }
}
