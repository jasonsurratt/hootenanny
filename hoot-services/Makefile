
####
# This makefile is here to give a consistent interface to all the projects.
# In other words, I don't like remembering cryptic maven commands. -JRS
####

SHELL=/bin/bash

# If the silent flag is passed to make then make Maven quiet too.
ifneq (,$(findstring s,$(MAKEFLAGS)))
    MVN_QUIET="-q"
endif

default: build

#add -X param temporarily to mvn command to debug mvn execution

./src/main/resources/db/liquibase.properties: ./src/main/resources/db/liquibase.properties.in ../conf/DatabaseConfig.sh
	../scripts/ReplaceEnvironmentVariables.sh ./src/main/resources/db/liquibase.properties.in ./src/main/resources/db/liquibase.properties

src/main/resources/db/spring-database.xml: src/main/resources/db/spring-database.xml.in ../conf/DatabaseConfig.sh
	../scripts/ReplaceEnvironmentVariables.sh src/main/resources/db/spring-database.xml.in src/main/resources/db/spring-database.xml

src/main/webapp/WEB-INF/workspace/jdbc/WFS_Connection.xml: src/main/webapp/WEB-INF/workspace/jdbc/WFS_Connection.xml.in ../conf/DatabaseConfig.sh
	../scripts/ReplaceEnvironmentVariables.sh src/main/webapp/WEB-INF/workspace/jdbc/WFS_Connection.xml.in src/main/webapp/WEB-INF/workspace/jdbc/WFS_Connection.xml


pom.xml: pom.xml.in ../conf/DatabaseConfig.sh
	../scripts/ReplaceEnvironmentVariables.sh pom.xml.in pom.xml

src/main/resources/conf/hoot-services.conf: src/main/resources/conf/hoot-services.conf.in ../conf/DatabaseConfig.sh
	../scripts/ReplaceEnvironmentVariables.sh src/main/resources/conf/hoot-services.conf.in src/main/resources/conf/hoot-services.conf

build-info:
	../scripts/GenerateBuildInfoFiles.sh

config: ./src/main/resources/db/liquibase.properties src/main/resources/db/spring-database.xml src/main/webapp/WEB-INF/workspace/jdbc/WFS_Connection.xml pom.xml src/main/resources/conf/hoot-services.conf


build: build-info config
	# If there isn't a local copy of the properties, copy the global version.
	# [ -f src/main/resources/conf/hoot-services.conf ] || cp src/main/resources/conf/hoot-services.conf.orig src/main/resources/conf/hoot-services.conf
	rm -rf ./src/main/java/hoot/services/db2
ifneq ($(MVN_QUIET),)
	../scripts/mvn-build $(MVN_QUIET) build 2> >( $(FILTER) >&2 )
else
	../scripts/mvn-build $(MVN_QUIET) build
endif
	#mvn -q compile com.sun.jersey.contribs:maven-wadl-plugin:generate 2> >( $(FILTER) >&2 )
	
	cp target/hoot-services-*.war ../bin


clean: pom.xml
	../scripts/mvn-build $(MVN_QUIET) clean
	rm -f pom.xml

# Both commands now do the same thing.
clean-db: clean-db-force
clean-db-force:
	../scripts/DeleteAllTables.sh
	rm -rf ./src/main/java/hoot/services/db2
	
test: config
	rm -rf ./src/main/java/hoot/services/db2
ifneq ($(MVN_QUIET),)
	../scripts/mvn-build $(MVN_QUIET) test 2> >( $(FILTER) >&2 )
else
	../scripts/mvn-build $(MVN_QUIET) test
endif

test-all: config
	rm -rf ./src/main/java/hoot/services/db2
ifneq ($(MVN_QUIET),)
	../scripts/mvn-build $(MVN_QUIET) verify 2> >( $(FILTER) >&2 )
else
	../scripts/mvn-build $(MVN_QUIET) verify
endif

