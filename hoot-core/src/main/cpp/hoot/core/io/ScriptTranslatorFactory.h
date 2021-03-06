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
#ifndef SCRIPTTRANSLATORFACTORY_H
#define SCRIPTTRANSLATORFACTORY_H

// Qt
#include <QString>

// Standard
#include <string>
#include <vector>

namespace hoot
{
using namespace std;
class ScriptTranslator;

class ScriptTranslatorFactory
{
public:

  static ScriptTranslatorFactory& getInstance() { return _theInstance; }

  ScriptTranslator* createTranslator(QString scriptPath);

  /**
   * This can be used to register translators, but using the Factory methods are preferred. See
   * JavaScriptTranslator for an example.
   */
  void registerScript(const std::string& className) { _translators.push_back(className); }

private:

  static ScriptTranslatorFactory _theInstance;

  ScriptTranslatorFactory();

  vector<string> _translators;

  void _init();
};

}

#endif // SCRIPTTRANSLATORFACTORY_H
