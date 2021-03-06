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

/*
    TDSv61 conversion script
        TDSv61 -> OSM+, and
        OSM+ -> TDSv61

    Based on mgcp/__init__.js script
*/

tds61 = {
    // getDbSchema - Load the standard schema or modify it into the TDS structure.
    getDbSchema: function() {
        layerNameLookup = {}; // <GLOBAL> Lookup table for converting an FCODE to a layername
        nfddAttrLookup = {}; // <GLOBAL> Lookup table for checking what attrs are in an FCODE
        
        // Warning: This is <GLOBAL> so we can get access to it from other functions
        rawSchema = tds61.schema.getDbSchema();

        // Add the Very ESRI specific FCSubtype attribute
        if (config.getOgrTdsAddFcsubtype() == 'true') rawSchema = translate.addFCSubtype(rawSchema);

     /* 
        // This has been removed since we no longer have text enumerations in the schema

        // Go go through the Schema and fix/add attributes
        for (var i=0, slen = rawSchema.length; i < slen; i++)
        {
            // Cycle throught he columns and "edit" the attribute fields with Text Enumerations
            // We convert these to plain String types and avoid having to handle String enumerations
            for (var j=0, clen = rawSchema[i].columns.length; j < clen; j++)
            {
                // exploit the Object and avoid a Switch :-)
                if (rawSchema[i].columns[j].name in {'ZI004_RCG':1,'ZSAX_RS0':1,'ZI020_IC2':1})
                {
                    rawSchema[i].columns[j].type = "String";
                    delete rawSchema[i].columns[j].enumerations;
                }
            } // End For rawSchema.columns.length
        } // End For rawSchema.length
     */

        // Build the NFDD fcode/attrs lookup table. Note: This is <GLOBAL>
        nfddAttrLookup = translate.makeAttrLookup(rawSchema);

        // Debug:
        // print("nfddAttrLookup");
        // translate.dumpLookup(nfddAttrLookup);

        // Decide if we are going to use TDS structure or 1 FCODE / File
        // if we DON't want the new structure, just return the rawSchema
        if (config.getOgrTdsStructure() == 'false')
        {
            // Now build the FCODE/layername lookup table. Note: This is <GLOBAL>
            layerNameLookup = translate.makeLayerNameLookup(rawSchema);

            // Now add an o2s[A,L,P] feature to the rawSchema
            // We can drop features but this is a nice way to see what we would drop
            rawSchema = translate.addEmptyFeature(rawSchema);

            // Add the empty Review layers
            rawSchema = translate.addReviewFeature(rawSchema);

            // Debugging:
            // translate.dumpSchema(rawSchema);

            return rawSchema;
        }

        // OK, now we build a new schema
        var newSchema = [];
        var layerName = '';
        var fCode = '';

        // Go through the fcode/layer list, find all of the layers and build a skeleton schema
        // layerList is used to keep track of what we have already seen
        var layerList = [];
        var geomType = '';
        for (var fc in tds61.rules.thematicGroupList)
        {
            layerName = tds61.rules.thematicGroupList[fc];
            if (~layerList.indexOf(layerName)) continue;  // Funky use of ~ instead of '!== -1'
            layerList.push(layerName);

            // Now build a skeleton schema
            if (~layerName.indexOf('Pnt'))
            {
                geomType = 'Point';
            }
            else if (~layerName.indexOf('Srf'))
            {
                geomType = 'Area';
            }
            else
            {
                geomType = 'Line';
            }

            newSchema.push({ name: layerName,
                          desc: layerName,
                          geom: geomType,
                          columns:[] 
                        });
        } // End fc loop

        // Loop through the old schema and populate the new one
        var newSchemaLen = newSchema.length; // cached as we use this a lot
        for (var os = 0, osLen = rawSchema.length; os < osLen; os++)
        {
            // The table looks like:
            // 'PGB230':'AeronauticPnt', // AircraftHangar
            // 'AGB230':'AeronauticSrf', // AircraftHangar
            // 'AGB015':'AeronauticSrf', // Apron
            // .... 
            // So we add the geometry to the FCODE

            fCode = rawSchema[os].geom.charAt(0) + rawSchema[os].fcode;
            layerName = tds61.rules.thematicGroupList[fCode];

            // Loop through the new schema and find the right layer
            for (var ns = 0; ns < newSchemaLen; ns++)
            { 
                // If we find the layer, populate it
                if (newSchema[ns].name == layerName)
                {
                    // now start adding attrs from the raw schema. This Is Not Pretty

                    // Loop through the columns in the OLD schema
                    for (var cos = 0, cosLen = rawSchema[os].columns.length; cos < cosLen; cos++)
                    {
                        var same = false;
                        // Loop through the columns in the NEW schema
                        for (var cns = 0, cnsLen = newSchema[ns].columns.length; cns < cnsLen; cns++)
                        {
                            // If the attribute names match then we can ignore it, unless it is enumerated
                            if (rawSchema[os].columns[cos].name == newSchema[ns].columns[cns].name)
                            {
                                same = true;
                                if (rawSchema[os].columns[cos].type !== 'enumeration' ) break;

                                // Now for some more uglyness....
                                // loop through the enumerated values  in the OLD schema
                                for (var oen = 0, oenlen = rawSchema[os].columns[cos].enumerations.length; oen < oenlen; oen++)
                                {
                                    var esame = false;
                                    // Loop through the enumerated values in the NEW schema
                                    for (var nen = 0, nenlen = newSchema[ns].columns[cns].enumerations.length; nen < nenlen; nen++)
                                    {
                                        // If the names match, ignore it
                                        if (rawSchema[os].columns[cos].enumerations[oen].name == newSchema[ns].columns[cns].enumerations[nen].name)
                                        {
                                            esame = true;
                                            break;
                                        }
                                    } // End nen loop
                                    // if the enumerated value isn't in the new list, add it
                                    if (!esame) 
                                    {
                                        newSchema[ns].columns[cns].enumerations.push(rawSchema[os].columns[cos].enumerations[oen]);
                                    }
                                } // End oen loop
                            } // End if enumeration
                        } // End nsc loop

                        // if the attr isn't in the new schema, add it
                        if (!same) 
                        {
                            // Remove the Default Value so we get all Null values on export
                            // delete rawSchema[os].columns[cos].defValue;
                            //rawSchema[os].columns[cos].defValue = undefined;

                            newSchema[ns].columns.push(rawSchema[os].columns[cos]);
                        }
                    } // End osc loop
                } // End if layerName
            } // End newSchema loop
        } // end rawSchema loop
        
        // Create a lookup table of TDS structures attributes. Note this is <GLOBAL>
        tdsAttrLookup = translate.makeTdsAttrLookup(newSchema);

        // Debug:
        // print("tdsAttrLookup");
        // translate.dumpLookup(tdsAttrLookup);

        // Add the ESRI Feature Dataset name to the schema
        //  newSchema = translate.addFdName(newSchema,'TDS');
        if (config.getOgrTdsFdname() !== "") newSchema = translate.addFdName(newSchema,config.getOgrTdsFdname());

        // Now add the o2s feature to the rawSchema
        // We can drop features but this is a nice way to see what we would drop
        // NOTE: We add these feature AFTER adding the ESRI Feature Dataset so that they
        // DON'T get put under the Feature Dataset in the output.
        newSchema = translate.addEmptyFeature(newSchema);

        // Add the empty Review layers
        newSchema = translate.addReviewFeature(newSchema);

        // Debug:
        // translate.dumpSchema(newSchema);

        return newSchema;

    }, // End getDbSchema

    // validateAttrs: Clean up the supplied attr list by dropping anything that should not be part of the
    //                feature, checking enumerated values and populateing the OTH field.
    validateAttrs: function(geometryType,attrs) {

        // First, use the lookup table to quickly drop all attributes that are not part of the feature.
        // This is quicker than going through the Schema due to the way the Schema is arranged
        var attrList = nfddAttrLookup[geometryType.toString().charAt(0) + attrs.F_CODE];

        var othList = {};

        if (attrs.OTH)
        {
            othList = translate.parseOTH(attrs.OTH); // Unpack the OTH field
            delete attrs.OTH;
        }

		if (attrList != undefined)
		{
            // The code is duplicated but it is quicker than doing the "if" on each iteration
            if (config.getOgrDebugDumpvalidate() == 'true')
            {
        	    for (var val in attrs) 
        	    {
            	    if (attrList.indexOf(val) == -1) 
                    {
                        if (val in othList)
                        {
                            //Debug:
                            // print('Validate: Dropping OTH: ' + val + '  (' + othList[val] + ')');
                            delete othList[val];
                        }

                        logWarn('Validate: Dropping ' + val + '  from ' + attrs.F_CODE);
                        delete attrs[val];
                    }
        	    }
            }
            else
            {
        	    for (var val in attrs) 
        	    {
            	    if (attrList.indexOf(val) == -1)
                    {
                        if (val in othList)
                        {
                            //Debug:
                            // print('Validate: Dropping OTH: ' + val + '  (' + othList[val] + ')');
                            delete othList[val];
                        }

                        delete attrs[val];
                    }
        	    }
            }
        }
        else
        {
            logWarn('Validate: No attrList for ' + attrs.F_CODE + ' ' + geometryType);
		} // End Drop attrs

        // Repack the OTH field
        if (Object.keys(othList).length > 0)
        {
            attrs.OTH = translate.packOTH(othList);
            // Debug:
            // print('New OTH: ' + attrs.OTH);
        }

        // No quick and easy way to do this unless we build yet another lookup table
        var feature = {};

        for (var i=0, sLen = rawSchema.length; i < sLen; i++)
        {
            if (rawSchema[i].fcode == attrs.F_CODE && rawSchema[i].geom == geometryType)
            {
                feature = rawSchema[i];
                break;
            }
        }

        // Now validate the Enumerated values
        for (var i=0, cLen = feature['columns'].length; i < cLen; i++)
        {
            // Skip non enumeratied attributes
            if (feature.columns[i].type !== 'enumeration') continue;

            var enumName = feature.columns[i].name;

            // Skip stuff that is missing and will end up as a default value
            if (!(attrs[enumName])) continue;

            var attrValue = attrs[enumName];
            var enumList = feature.columns[i].enumerations;
            var enumValueList = [];

            // Pull all of the values out of the enumerated list to make life easier
            for (var j=0, elen = enumList.length; j < elen; j++) enumValueList.push(enumList[j].value);

            // If we DONT have the value in the list, add it to the OTH or MEMO field
            if (enumValueList.indexOf(attrValue) == -1) 
            {
                var othVal = '(' + enumName + ':' + attrValue + ')';

                // No "Other" value. Push to the Memo field
                if (enumValueList.indexOf('999') == -1) 
                {
                    // Set the offending enumerated value to the default value
                    attrs[enumName] = feature.columns[i].defValue;

                    logWarn('Validate: Enumerated Value: ' + attrValue + ' not found in ' + enumName + ' Setting ' + enumName + ' to its default value (' + feature.columns[i].defValue + ')');

                    attrs.ZI006_MEM = translate.appendValue(attrs.ZI006_MEM,othVal,';');
                }
                else
                {
                    // Set the offending enumerated value to the "other" value
                    attrs[enumName] = '999';

                    logWarn('Validate: Enumerated Value: ' + attrValue + ' not found in ' + enumName + ' Setting OTH and ' + enumName + ' to Other (999)');

                    attrs.OTH = translate.appendValue(attrs.OTH,othVal,' ');
                }

            } // End attrValue in enumList

        } // End Validate Enumerations

    }, // End validateAttrs


    // validateTDSAttrs - Clean up the TDS format attrs.  This sets all of the extra attrs to be "undefined"
    validateTDSAttrs: function(gFcode, attrs) {

        var tdsAttrList = tdsAttrLookup[tds61.rules.thematicGroupList[gFcode]];
        var nfddAttrList = nfddAttrLookup[gFcode];

        for (var i = 0, len = tdsAttrList.length; i < len; i++)
        {
            if (nfddAttrList.indexOf(tdsAttrList[i]) == -1) attrs[tdsAttrList[i]] = undefined;
        }
    }, // End validateTDSAttrs


    // Sort out if we need to return two features or one.
    // This is generally for Roads/Railways & bridges but can also be for other features.
    twoFeatures: function(geometryType, tags, attrs)
    {
        var newAttrs = {};

        // Sort out Roads, Railways and Bridges.
        if (geometryType == 'Line' && tags.bridge && (tags.highway || tags.railway))
        {
            if (attrs.F_CODE !== 'AQ040') // Not a Bridge
            {
                newAttrs.F_CODE = 'AQ040';

                if (tags.highway)
                {
                    newAttrs.TRS = '13'; // Road
                }
                else if (tags.railway)
                {
                    newAttrs.TRS = '12'; // Railway
                }
            }
            else // A Bridge
            {
                if (tags.railway)
                {
                    newAttrs.F_CODE = 'AN010'; // Railway
                    attrs.TRS = '12'; // Railway
                }
                else
                {
                    if (tags.highway == 'track')
                    {
                        newAttrs.F_CODE = 'AP010';
                    }
                    else
                    {   // The default is to make it a road
                        newAttrs.F_CODE = 'AP030';
                    }

                    attrs.TRS = '13'; // Road
                }
            }

            // Remove the uuid from the tag list so we get a new one for the second feature
            delete tags.uuid;
        } // End sort out Road, Railway & Bridge

        // If we are making a second feature, process it.
        if (newAttrs.F_CODE)
        {
            // Now go make a second feature
            // pre processing
            tds61.applyToNfddPreProcessing(tags, newAttrs, geometryType);

            // one 2 one - we call the version that knows about OTH fields
            translate.applyNfddOne2One(tags, newAttrs, tds61.lookup, tds61.fcodeLookup, tds61.ignoreList);

            // apply the simple number and text biased rules
            // Note: These are BACKWARD, not forward!
            translate.applySimpleNumBiased(newAttrs, tags, tds61.rules.numBiased, 'backward');
            translate.applySimpleTxtBiased(newAttrs, tags, tds61.rules.txtBiased, 'backward');

            // post processing
            tds61.applyToNfddPostProcessing(tags, newAttrs, geometryType);
        }

        // Debug:
        // for (var i in newAttrs) print('twoFeatures: New Attrs:' + i + ': :' + newAttrs[i] + ':');

        // Return the new attributes
        return newAttrs;
    }, // End twoFeatures


    // ##### Start of the xxToOsmxx Block #####
    applyToOsmPreProcessing: function(attrs, layerName) 
    {
        // The What Were They Thinking? swap list.  Each of these is the _same_ attribute
        // but renamed in different features. We swap these so that there is only one
        // set of rules needed in the One2One section.
        // These get converted back on output - if we need to.
        var swapList = { 
                'ASU':'ZI019_ASU', 'ASU2':'ZI019_ASU3', 'ASU3':'ZI019_ASU3',
                'AT005_CAB':'CAB', 'AT005_CAB2':'CAB2', 'AT005_CAB3':'CAB3',
                'HYP':'ZI024_HYP', 
                // 'LEN_':'LZN',
                'MEM':'ZI006_MEM',
                'PBY':'ZI014_PBY', 'PBY2':'ZI014_PBY2', 'PBY3':'ZI014_PBY3',
                'PPO':'ZI014_PPO', 'PPO2':'ZI014_PPO2', 'PPO3':'ZI014_PPO3',
                'PRW':'ZI014_PRW', 'PRW2':'ZI014_PRW2', 'PRW3':'ZI014_PRW3',
                'RCG':'ZI004_RCG',
                'RTN':'RIN_RTN', 'RTN2':'RIN_RTN2', 'RTN3':'RIN_RTN3',
                'SUR':'ZI026_SUR',
                'WD1':'ZI016_WD1',
                'YWQ':'ZI024_YWQ',   
                'ZI025_MAN':'MAN',
                'ZI025_WLE':'WLE',
                'ZI032_GUG':'GUG',
                'ZI032_TOS':'TOS',
                'ZI032_PYC':'PYC',   
                'ZI032_PYM':'PYM',
                'ZI071_FFN':'FFN', 'ZI071_FFN2':'FFN2', 'ZI071_FFN3':'FFN3',
                'ZVH_VDT':'VDT'
                };


        // List of data values to drop/ignore
        var ignoreList = { '-999999.0':1, '-999999':1, 'noinformation':1 };

        // This is a handy loop. We use it to:
        // 1) Remove all of the "No Information" and -999999 fields
        // 2) Convert all of the Attrs to uppercase - if needed
       for (var col in attrs)
        {
            // slightly ugly but we would like to account for: 'No Information', 'noInformation' etc
            // First, push to lowercase
            var attrValue = attrs[col].toString().toLowerCase();

            // Get rid of the spaces in the text
            attrValue = attrValue.replace(/\s/g, '');

            // Wipe out the useless values
            if (attrs[col] == '' || attrValue in ignoreList || attrs[col] in ignoreList)
            {
                delete attrs[col]; // debug: Comment this out to leave all of the No Info stuff in for testing
                continue;
            }

            // Push the attribute to upper case - if needed
            var c = col.toUpperCase();
            if (c !== col)
            {
                attrs[c] = attrs[col];
                delete attrs[col];
                col = c; // For the rest of this loop iteration
            }

            // Now see if we need to swap attr names
            if (col in swapList)
            {
                // print('Swapped: ' + swapList[i]); // debug
                attrs[swapList[col]] = attrs[col];
                delete attrs[col];
                continue;
            }

        } // End in attrs loop

        // Drop all of the XXX Closure default values IFF the associated attributes are 
        // not set.
        // Doing this after the main cleaning loop so all of the -999999 values are
        // already gone and we can just check for existance.
        var closureList = {
                            'AQTC':['AQTL','AQTU'],
                            'AYRC':['AYRL','AYRU'],
                            'BPWHAC':['BPWHAL','BPWHAU'],
                            'BPWHBC':['BPWHBL','BPWHBU'],
                            'BPWSAC':['BPWSAL','BPWSAU'],
                            'BPWSBC':['BPWSBL','BPWSBU'],
                            'BWVCAC':['BWVCAL','BWVCAU'],
                            'BWVCBC':['BWVCBL','BWVCBU'],
                            'DMBC':['DMBL','DMBU'],
                            'DPAC':['DPAL','DPAU'],
                            'GSGCHC':['GSGCHL','GSGCHU'],
                            'GSGCLC':['GSGCLL','GSGCLU'],
                            'PWAC':['PWAL','PWAU'],
                            'RMWC':['RMWL','RMWU'],
                            'SDCC':['SDCL','SDCU'],
                            'SDSC':['SDSL','SDSU'],
                            'SGCC':['SGCL','SGCU'],
                            'TSCC':['TSCL','TSCU'],
                            'WDAC':['WDAL','WDAU'],
                            'ZI026_CTUC':['ZI026_CTUL','ZI026_CTUU']
                          }

        for (var i in closureList)
        {
            if (attrs[i])
            {
                if (attrs[closureList[i][0]] || attrs[closureList[i][1]])
                {
                    continue;
                }
                else
                {
                    delete attrs[i];
                }
            }
        } // End closureList

        // Now find an F_CODE
        if (attrs.F_CODE)
        {
            // Nothing to do :-)
        }
        else if (attrs.FCODE)
        {
            attrs.F_CODE = attrs.FCODE;
            delete attrs.FCODE;
        }
        else
        {
            // Time to find an FCODE based on the filename
            var fCodeMap = [
                ['AF010', ['af010','smokestack_p']], // Smokestack
                ['AH025', ['ah025','engineered_earthwork_s','engineered_earthwork_p']], // Engineered Earthwork
                ['AH060', ['ah060','underground_bunker_s','underground_bunker_p']], // Underground Bunker
                ['AL010', ['al010','facility_s','facility_p']], // Facility
                ['AL013', ['al013','building_s','building_p']], // Building
                ['AL018', ['al018','building_superstructure_s','building_superstructure_c','building_superstructure_p']], // Building Superstructure
                ['AL020', ['al020','built-up_area_s','built-up_area_p']], // Built up area
                ['AL030', ['al030','cemetery_s','cemetery_p']], // Cemetary
                ['AL070', ['al070','fence_c']], // Fence
                ['AL099', ['al099','hut_p']], // Hut
                ['AL105', ['al105','settlement_s','settlement_p']], // Settlement
                ['AL130', ['al130','memorial_monument_s','memorial_monument_p']], // Memorial Monument
                ['AL200', ['al200','ruins_s','ruins_p']], // Ruins
                ['AL208', ['al208','shanty_town_s','shanty_town_p']], // Shanty Town
                ['AL241', ['al241','tower_s','tower_p']], // Tower
                ['AL260', ['al260','wall_c']], // Wall
                ['AM080', ['am080','water_tower_p','water_tower_s']], // Water Tower
                ['AN010', ['an010','railway_c']], // Railway
                ['AN050', ['an050','railway_sidetrack_c']], // Railway Sidetrack
                ['AN060', ['an060','railway_yard_s']], // Railway Yard
                ['AN075', ['an075','railway_turntable_p','railway_turntable_p']], // Railway Yard
                ['AN076', ['an076','roundhouse_s','roundhouse_p']], // Roundhouse
                ['AP010', ['ap010','cart_track_c']], // Cart Track
                ['AP020', ['ap020','road_interchange_p']], // Interchange
                ['AP030', ['ap030','road_c']], // Road
                ['AP040', ['ap040','gate_c','gate_p']], // Gate
                ['AP041', ['ap041','vehicle_barrier_c','vehicle_barrier_p']], // Vehicle Barrier
                ['AP050', ['ap050','trail_c']], // Trail
                ['AQ040', ['aq040','bridge_c','bridge_p']], // Bridge 
                ['AQ045', ['aq045','bridge_span_c','bridge_span_p']], // Bridge Span
                ['AQ065', ['aq065','culvert_c','culvert_p']], // Culvert
                ['AQ070', ['aq070','ferry_crossing_c']], // Ferry Crossing
                ['AQ095', ['aq095','tunnel_mouth_p']], // Tunnel Mouth
                ['AQ113', ['aq113','pipeline_c']], // Pipeline
                ['AQ125', ['aq125','transportation_station_s','transportation_station_p']], // Transportation Station
                ['AQ130', ['aq130','tunnel_c']], // Tunnel
                ['AQ140', ['aq140','vehicle_lot_s']], // Vehicle Lot
                ['AQ141', ['aq141','parking_garage_s','parking_garage_p']], // Parking Garage
                ['AQ170', ['aq170','motor_vehicle_station_s','motor_vehicle_station_p']], // Motor Vehicle Station
                ['AT010', ['at010','dish_aerial_p']], // Dish Aerial
                ['AT042', ['at042','pylon_p']], // Pylon
                ['BH010', ['bh010','aqueduct_s','aqueduct_c']], // Aqueduct
                ['BH020', ['bh020','canal_s','canal_c']], // Canal
                ['BH030', ['bh030','ditch_s','ditch_c']], // Ditch 
                ['BH070', ['bh070','ford_c','ford_p']], // Ford 
                ['BH082', ['bh082','inland_waterbody_s','inland_waterbody_p']], // Inland Waterbody 
                ['BH140', ['bh140', 'river_s','river_c']], // River
                ['BH170', ['bh170','natural_pool_p']], // Natural Pool
                ['BH230', ['bh230', 'water_well_p','water_well_s']], // Water Well
                ['BI010', ['bi010', 'cistern_p']], // Cistern
                ['DB070', ['db070','cut_c']], // Cut
                ['DB150', ['db150','mountain_pass_p']], // Mountain Pass
                ['GB050', ['gb050','aircraft_revetment_c']], // Aircraft Revetment
                ['ZD040', ['zd040','named_location_s','named_location_c','named_location_p']], // Named Location
                ['ZD045', ['zd045','annotated_location_s','annotated_location_c','annotated_location_p']], // Named Location
                ];

            // Funky but it makes life easier
            var llayerName = layerName.toString().toLowerCase();

            for (var row in fCodeMap)
            {
                for (var val in fCodeMap[row][1])
                {
                    if (llayerName.match(fCodeMap[row][1][val])) attrs.F_CODE = fCodeMap[row][0];
                }
            }
        } // End of Find an FCode

    }, // End of applyToOsmPreProcessing

    applyToOsmPostProcessing : function (attrs, tags, layerName)
    {
        // Roads. TDSv61 are a bit simpler than TDSv30 & TDSv40
        if (attrs.F_CODE == 'AP030') // Make sure we have a road
        {
             // Set a Default: "It is a road but we don't know what it is"
            tags.highway = 'road';

            // Top level
            if (tags['ref:road:type'] == 'motorway' || tags['ref:road:class'] == 'national_motorway')
            {
                tags.highway = 'motorway';
            }
            else if (tags['ref:road:type'] == 'limited_access_motorway' || tags['ref:road:class'] == 'primary')
            {
                tags.highway = 'trunk';
            }
            else if (tags['ref:road:class'] == 'secondary')
            {
                tags.highway = 'primary';
            }
            else if (tags['ref:road:class'] == 'local')
            {
                tags.highway = 'secondary';
            }
            else if (tags['ref:road:type'] == 'road')
            {
                tags.highway = 'tertiary';
            }
            else if (tags['ref:road:type'] == 'street')
            {
                tags.highway = 'unclassified';
            }
            // Other should get picked up by the OTH field
            else if (tags['ref:road:type'] == 'other')
            {
                tags.highway = 'road';
            }
            // Catch all for the rest of the ref:road:type: close, circle drive etc
            else if (tags['ref:road:type'])
            {
                tags.highway = 'residential';
            }
        } // End if AP030


        // New TDSv61 Attribute - ROR (Road Interchange Ramp)
        if (tags.highway && tags.link_road == 'yes')
        {
            var roadList = ['motorway','trunk','primary','secondary','tertiary'];
            if (roadList.indexOf(tags.highway) !== -1) tags.highway = tags.highway + '_link';
        }

        // If we have a UFI, store it. Some of the MAAX data has a LINK_ID instead of a UFI
        tags.source = 'tdsv61'; 
        if (attrs.UFI)
        {
            tags.uuid = '{' + attrs['UFI'].toString().toLowerCase() + '}';
        }
        else
        {
            tags.uuid = createUuid(); 
        }


        if (tds61.osmPostRules == undefined)
        {
            // ##############
            // A "new" way of specifying rules. Jason came up with this while playing around with NodeJs
            //
            // Rules format:  ["test expression","output result"];
            // Note: t = tags, a = attrs and attrs can only be on the RHS
            var rulesList = [
            ["t.amenity == 'stop' && t['transport:type'] == 'bus'","t.highway = 'bus_stop';"],
            ["t.boundary == 'protected_area' && !(t.protect_class)","t.protect_class = '4';"],
            ["t.control_tower == 'yes' && t.use == 'air_traffic_control'","t['tower:type'] = 'observation'"],
            ["t['generator:source'] == 'wind'","t.power = 'generator'"],
            ["t.historic == 'castle' && !(t.ruins) && !(t.building)","t.building = 'yes'"],
            ["(t.landuse == 'built_up_area' || t.place == 'settlement') && t.building","t['settlement:type'] = t.building; delete t.building"],
            ["t.leisure == 'stadium'","t.building = 'yes'"],
            ["t['material:vertical']","t.material = t['material:vertical']; delete t['material:vertical']"],
            ["t['monitoring:weather'] == 'yes'","t.man_made = 'monitoring_station'"],
            ["t.public_transport == 'station' && t['transport:type'] == 'railway'","t.railway = 'station'"],
            ["t.public_transport == 'station' && t['transport:type'] == 'bus'","t.bus = 'yes'"],
            ["t.pylon =='yes' && t['cable:type'] == 'cableway'"," t.aerialway = 'pylon'"],
            ["t.pylon =='yes' && t['cable:type'] == 'power'"," t.power = 'tower'"],
            ["t.service == 'yard'","t.railway = 'yes'"],
            ["t.social_facility","t.amenity = 'social_facility'; t['social_facility:for'] = t.social_facility; t.social_facility = 'shelter'"],
            ["t['tower:material']","t.material = t['tower:material']; delete t['tower:material']"],
            ["t['tower:type'] && !(t.man_made)","t.man_made = 'tower'"],
            ["t.use == 'islamic_prayer_hall' && !(t.amenity)","t.amenity = 'place_of_worship'"],
            ["t.water || t.landuse == 'basin'","t.natural = 'water'"],
            ["t.waterway == 'flow_control'","t.flow_control = 'sluice_gate'"],
            ["t.wetland && !(t.natural)","t.natural = 'wetland'"]
            ];

            tds61.osmPostRules = translate.buildComplexRules(rulesList);
        }

        // translate.applyComplexRules(tags,attrs,rulesList);
        translate.applyComplexRules(tags,attrs,tds61.osmPostRules);

        // ##############

        // Road & Railway Crossings
        // Road/Rail = crossing
        // Road + Rail = level_crossing
        if (tags.crossing_point)
        {
            if (tags['transport:type'] == 'railway')
            {
                tags.railway = 'crossing';

                if (tags['transport:type2'] == 'road') tags.railway = 'level_crossing';
            }
            else if (tags['transport:type'] == 'road')
            {
                if (tags['transport:type2'] == 'railway')
                {
                    tags.railway = 'level_crossing';
                }
                else
                {
                    tags.highway = 'crossing';
                }
            }
        } // End crossing_point


        // Add a building tag to buildings if we don't have one
        // We can't do this in the funky rules function as it uses "attrs" _and_ "tags"
        if (attrs.F_CODE == 'AL013' && !(tags.building)) tags.building = 'yes';

        // facility list is used for translating between buildings and facilities based on use
        // format:  "use":"building or amenity"
        // var facilityList = {'education':'school', 'healthcare':'hospital', 'university':'university'};

        // Fix the building 'use' tag. If the building has a 'use' and no specific building tag. Give it one
        if (tags.building == 'yes' && tags.use)
        {
            if ((tags.use.indexOf('manufacturing') > -1) || (tags.use.indexOf('processing') > -1))
            {
                tags.building = 'industrial';
            }
      /*
            else if (tags.use in facilityList)
            {
                tags.building = facilityList[tags.use];
                // delete tags.use;
            }
       */
        }

        // A facility is an area. Therefore "use" becomes "amenity". "Building" becomes "landuse"
        if (tags.facility && tags.use)
        {
            if ((tags.use.indexOf('manufacturing') > -1) || (tags.use.indexOf('processing') > -1))
            {
                tags.man_made = 'works';
            }
       /*
            else if (tags.use in facilityList) 
            {
                tags.amenity = facilityList[tags.use];
            }
        */
        }

      // Should be disabled until polygons are fixed....
        translate.fixConstruction(tags, 'highway');
        translate.fixConstruction(tags, 'railway');

        // Add defaults for common features
        if (attrs.F_CODE == 'AP020' && !(tags.junction)) tags.junction = 'yes';
        if (attrs.F_CODE == 'AQ040' && !(tags.bridge)) tags.bridge = 'yes';
        if (attrs.F_CODE == 'AQ040' && !(tags.highway)) tags.highway = 'yes';
        if (attrs.F_CODE == 'BH140' && !(tags.waterway)) tags.waterway = 'river';

    }, // End of applyToOsmPostProcessing
  
    // ##### End of the xxToOsmxx Block #####

    // ##### Start of the xxToNfddxx Block #####

    applyToNfddPreProcessing: function(tags, attrs, geometryType)
    {
        // initial cleanup
        for (var val in tags)
        {
            // Remove empty tags
            if (tags[val] == '')
            {
                delete tags[val];
                continue;
            }

            // Convert "abandoned:XXX" features
            if (val.indexOf('abandoned:') !== -1)
            {
                // Hopeing there is only one ':' in the tag name...
                var tList = val.split(':');
                tags[tList[1]] = tags[val];
                tags.condition = 'abandoned';
                delete tags[val];
            }
        }

        if (tds61.nfddPreRules == undefined)
        {
        // See ToOsmPostProcessing for more details about rulesList.
            var rulesList = [
            ["t.amenity == 'bus_station'","t.public_transport = 'station'; t['transport:type'] == 'bus'"],
            ["t.amenity == 'marketplace'","t.facility = 'yes'"],
            ["t.construction && t.highway","t.highway = t.construction; t.condition = 'construction'; delete t.construction"],
            ["t.construction && t.railway","t.railway = t.construction; t.condition = 'construction'; delete t.construction"],
            ["t.control_tower && t.man_made == 'tower'","delete t.man_made"],
            ["t.highway == 'bus_stop'","t['transport:type'] = 'bus'"],
            ["t.highway == 'crossing'","t['transport:type'] = 'road';a.F_CODE = 'AQ062'; delete t.highway"],
            ["t.highway == 'mini_roundabout'","t.junction = 'roundabout'"],
            ["t.historic == 'castle' && t.building","delete t.building"],
            ["t.historic == 'castle' && t.ruins == 'yes'","t.condition = 'destroyed'; delete t.ruins"],
            ["t.landcover == 'snowfield' || t.landcover == 'ice-field'","a.F_CODE = 'BJ100'"],
            ["t.landuse == 'allotments'","t.landuse = 'farmland'"],
            ["t.landuse == 'brownfield'","t.landuse = 'built_up_area'; t.condition = 'destroyed'"],
            ["t.landuse == 'construction'","t.landuse = 'built_up_area'; t.condition = 'construction'"],
            ["t.landuse == 'farm'","t.landuse = 'farmland'"],
            ["t.landuse == 'farmland' && t.crop == 'fruit_tree'","t.landuse = 'orchard'"],
            ["t.landuse == 'farmyard'","t.facility = 'yes'; t.use = 'agriculture'; delete t.landuse"],
            ["t.landuse == 'grass'","t.natural = 'grassland'; t['grassland:type'] = 'grassland';"],
            ["t.landuse == 'meadow'","t.natural = 'grassland'; t['grassland:type'] = 'meadow';"],
            ["t.landuse == 'military'","t.military = 'installation'; delete t.landuse"],
            ["t.leisure == 'recreation_ground'","t.landuse = 'recreation_ground'; delete t.leisure"],
            ["t.landuse == 'reservoir'","t.water = 'reservoir'; delete t.landuse"],
            ["t.landuse == 'retail'","t.landuse = 'built_up_area'; t.use = 'commercial'"],
            ["t.landuse == 'scrub'","t.natural = 'scrub'; delete t.landuse"],
            // ["t.landuse == 'grass'","a.F_CODE = 'EB010'; t['grassland:type'] = 'grassland';"],
            // ["t.landuse == 'meadow'","a.F_CODE = 'EB010'; t['grassland:type'] = 'meadow';"],
            ["t.leisure == 'sports_centre'","t.facility = 'yes'; t.use = 'recreation'; delete t.leisure"],
            ["t.leisure == 'stadium' && t.building","delete t.building"],
            ["t.median == 'yes'","t.divider = 'yes'"],
            ["t.natural == 'wood'","t.landuse = 'forest'; delete t.natural"],
            ["t.power == 'pole'","t['cable:type'] = 'power'; t['tower:shape'] = 'pole'"],
            ["t.power == 'tower'","t['cable:type'] = 'power'"],
            ["t.power == 'line'","t['cable:type'] = 'power'; t.cable = 'yes'"],
            ["t.power == 'generator'","t.use = 'power_generation'; a.F_CODE = 'AL013'"],
            ["t.rapids == 'yes'","t.waterway = 'rapids'; delete t.rapids"],
            ["t.railway == 'station'","t.public_transport = 'station';  t['transport:type'] = 'railway'"],
            ["t.railway == 'level_crossing'","t['transport:type'] = 'railway';t['transport:type2'] = 'road'; a.F_CODE = 'AQ062'; delete t.railway"],
            ["t.railway == 'crossing'","t['transport:type'] = 'railway'; a.F_CODE = 'AQ062'; delete t.railway"],
            ["t.resource","t.raw_material = t.resource; delete t.resource"],
            ["t.route == 'road' && !(t.highway)","t.highway = 'road'; delete t.route"],
            ["(t.shop || t.office) &&  !(t.building)","a.F_CODE = 'AL013'"],
            ["t.social_facility == 'shelter'","t.social_facility = t['social_facility:for']; delete t.amenity; delete t['social_facility:for']"],
            ["!(t.water) && t.natural == 'water'","t.water = 'lake'"],
            ["t.use == 'islamic_prayer_hall' && t.amenity == 'place_of_worship'","delete t.amenity"],
            ["t.wetland && t.natural == 'wetland'","delete t.natural"],
            ["t.water == 'river'","t.waterway = 'river'"],
            ["t.waterway == 'riverbank'","t.waterway = 'river'"]
            ];

            tds61.nfddPreRules = translate.buildComplexRules(rulesList);
        }

        // Apply the rulesList.
        // translate.applyComplexRules(tags,attrs,rulesList);
        translate.applyComplexRules(tags,attrs,tds61.nfddPreRules);

        // Fix up OSM 'walls' around facilities
        if (tags.barrier == 'wall' && geometryType == 'Area')
        {
            attrs.F_CODE = 'AL010'; // Facility
            delete tags.barrier; // Take away the walls...
        }

        // Some tags imply that they are buildings but don't actually say so.
        // Most of these are taken from raw OSM and the OSM Wiki
        // Not sure if the list of amenities that ARE buildings is shorter than the list of ones that 
        // are not buildings.
        // Taking "place_of_worship" out of this and making it a building
        var notBuildingList = [
            'bbq','biergarten','drinking_water','bicycle_parking','bicycle_rental','boat_sharing',
            'car_sharing','charging_station','grit_bin','parking','parking_entrance','parking_space',
            'taxi','atm','fountain','bench','clock','hunting_stand','marketplace','post_box',
            'recycling', 'vending_machine','waste_disposal','watering_place','water_point',
            'waste_basket','drinking_water','swimming_pool','fire_hydrant','emergency_phone','yes',
            'compressed_air','water','nameplate','picnic_table','life_ring','grass_strip','dog_bin',
            'artwork','dog_waste_bin','street_light','park','hydrant','tricycle_station','loading_dock',
            'trailer_park','game_feeding'
            ]; // End notBuildingList

        if (tags.amenity && !(tags.building) && (notBuildingList.indexOf(tags.amenity) == -1)) attrs.F_CODE = 'AL013';

        // going out on a limb and processing OSM specific tags:
        // - Building == a thing,
        // - Amenity == The area around a thing
        // Note: amenity=place_of_worship is a special case. It _should_ have an associated building tag
        var facilityList = {
            'school':'850', 'university':'855', 'college':'857', 'hospital':'860'
            };

        if (tags.amenity in facilityList)
        {
            if (geometryType == 'Area')
            {
                attrs.F_CODE = 'AL010'; // Facility
            }
            else
            {
                attrs.F_CODE = 'AL013'; // Building
            }

            // If we don't have a Feature Function then assign one.
            if (!(attrs.FFN)) attrs.FFN = facilityList[tags.amenity];
        }

        // Fix up water features from OSM
        if (tags.natural == 'water' && !(tags.water)) 
        {
            if (geometryType =='Line')
            {
                tags.waterway = 'river';
                attrs.F_CODE = 'BH140';
            }
            else
            {
                tags.water = 'lake';
                attrs.F_CODE = 'BH082';
            }
        }

        // Cutlines/Cleared Ways & Highways
        // When we can output two features, this will be split
        if (tags.man_made == 'cutline' && tags.highway)
        {
            if (geometryType == 'Area')
            {
                // Keep the cutline/cleared way
                attrs.F_CODE = 'EC040';

                if (tags.memo)
                {
                    tags.memo = tags.memo + ';highway:' + tags.highway;
                }
                else
                {
                    tags.memo = 'highway:' + tags.highway;
                }

                delete tags.highway;
            }
            else
            { 
                // Drop the cutline/cleared way
                delete tags.man_made; 

                if (tags.memo)
                {
                    tags.memo = tags.memo + ';cleared_way:yes';
                }
                else
                {
                    tags.memo = 'cleared_way:yes';
                }
            }
        }

        // Places, localities and regions
        if (tags.place)
        {
            switch (tags.place)
            {
                case 'city':
                case 'town':
                case 'suburb':
                case 'neighbourhood':
                case 'quarter':
                case 'village':
                    attrs.F_CODE = 'AL020'; // Built Up Area
                    delete tags.place;
                    break;

                case 'hamlet':
                case 'isolated_dwelling':
                    attrs.F_CODE = 'AL105'; // Settlement
                    delete tags.place;
                    break;

                case 'populated':
                case 'state':
                case 'county':
                case 'region':
                case 'locality':
                case 'municipality':
                case 'borough':
                case 'unknown':
                    attrs.F_CODE = 'ZD045'; // Annotated Location
                    if (tags.memo)
                    {
                        tags.memo = tags.memo + ';annotation:' + tags.place;
                    }
                    else
                    {
                        tags.memo = 'annotation:' + tags.place;
                    }
                    delete tags.place;
                    break;

            } // End switch
        }

        // Bridges & Roads.  If we have an area or a line everything is fine.
        // If we have a point, we need to make sure that it becomes a bridge, not a road.
        if (tags.bridge && geometryType =='Point') attrs.F_CODE = 'AQ040';

        // Now use the lookup table to find an FCODE. This is here to stop clashes with the 
        // standard one2one rules
        if (!(attrs.F_CODE) && tds61.fcodeLookup)
        {
            for (var col in tags)
            {
                var value = tags[col];
                if (col in tds61.fcodeLookup)
                {
                    if (value in tds61.fcodeLookup[col])
                    {
                        var row = tds61.fcodeLookup[col][value];
                        attrs.F_CODE = row[1];
                        // print('FCODE: Got ' + attrs.F_CODE);
                    }
                }
            }
        } // End find F_CODE

        // If we still don't have an FCODE, try looking for individual elements
        if (!attrs.F_CODE)
        {
            var fcodeMap = { 
                'highway':'AP030', 'railway':'AN010', 'building':'AL013', 'ford':'BH070', 
                'waterway':'BH140', 'bridge':'AQ040', 'railway:in_road':'AN010', 
                'barrier':'AP040', 'tourism':'AL013','junction':'AP020',
                'mine:access':'AA010'
                           };

            for (var i in fcodeMap)
            {
                if (i in tags) attrs.F_CODE = fcodeMap[i];
            }
        }

        // Sort out PYM vs ZI032_PYM vs MCC vs VCM - Ugly
        var pymList = [ 'AL110','AL241','AQ055','AQ110','AT042'];

        var vcmList = [ 'AA040', 'AC020', 'AD010', 'AD025', 'AD030', 'AD041', 'AD050', 'AF010',
                        'AF020', 'AF021', 'AF030', 'AF040', 'AF070', 'AH055', 'AJ050', 'AJ051',
                        'AJ080', 'AJ085', 'AL010', 'AL013', 'AL019', 'AL080', 'AM011', 'AM020',
                        'AM030', 'AM070', 'AN076', 'AQ040', 'AQ045', 'AQ060', 'AQ116', 'BC050',
                        'BD115', 'BI010', 'BI050', 'GB230' ];

        if (tags.material)
        {
            if (pymList.indexOf(attrs.F_CODE) !== -1) 
            {
                tags['tower:material'] = tags.material;
                delete tags.material;
            }
            else if (vcmList.indexOf(attrs.F_CODE) !== -1) 
            {
                tags['material:vertical'] = tags.material;
                delete tags.material;
            }
        }

       // Protected areas have two attributes that need sorting out
       if (tags.protection_object == 'habitat' || tags.protection_object == 'breeding_ground')
       {
           if (tags.protect_class) delete tags.protect_class;
       }

       // Split link roads. TDSv61 now has an attribute for this
       if (tags.highway && (tags['highway'].indexOf('_link') !== -1))
       {
           tags.highway = tags['highway'].replace('_link','');
           tags.road_ramp = 'yes';
       }


    }, // End applyToNfddPreProcessing

    applyToNfddPostProcessing : function (tags, attrs, geometryType)
    {
        // Shoreline Construction (BB081) covers a lot of features
        if (attrs.PWC) attrs.F_CODE = 'BB081';

        // Inland Water Body (BH082) also covers a lot of features
        if (attrs.IWT && !(attrs.F_CODE)) attrs.F_CODE = 'BH082';

        // The follwing bit of ugly code is to account for the specs haveing two different attributes
        // with similar names and roughly the same attributes. Bleah!
        // Format is: <FCODE>:[<from>:<to>]
        var swapList = {
            'AA010':['ZI014_PPO','PPO'], 'AA010':['ZI014_PPO2','PPO2'], 'AA010':['ZI014_PPO3','PPO3'],
            'AA020':['ZI014_PPO','PPO'], 'AA020':['ZI014_PPO2','PPO2'], 'AA020':['ZI014_PPO3','PPO3'],
            'AA040':['ZI014_PPO','PPO'], 'AA040':['ZI014_PPO2','PPO2'], 'AA040':['ZI014_PPO3','PPO3'],
            'AA052':['ZI014_PPO','PPO'], 'AA052':['ZI014_PPO2','PPO2'], 'AA052':['ZI014_PPO3','PPO3'],
            'AA054':['ZI014_PPO','PPO'], 'AA054':['ZI014_PPO2','PPO2'], 'AA054':['ZI014_PPO3','PPO3'],
            'AB000':['ZI014_PBY','PBY'], 'AB000':['ZI014_PBY2','PBY2'], 'AB000':['ZI014_PBY3','PBY3'],
            'AC060':['ZI014_PPO','PPO'], 'AC060':['ZI014_PPO2','PPO2'], 'AC060':['ZI014_PPO3','PPO3'],
            'AD020':['ZI014_PPO','PPO'], 'AD020':['ZI014_PPO2','PPO2'], 'AD020':['ZI014_PPO3','PPO3'],
            'AD025':['ZI014_PPO','PPO'], 'AD025':['ZI014_PPO2','PPO2'], 'AD025':['ZI014_PPO3','PPO3'],
            'AJ050':['ZI014_PPO','PPO'], 'AJ050':['ZI014_PPO2','PPO2'], 'AJ050':['ZI014_PPO3','PPO3'],
            'AL020':['ZI005_NFN','ZI005_NFN1'],
            'AM010':['ZI014_PPO','PPO'], 'AM010':['ZI014_PPO2','PPO2'], 'AM010':['ZI014_PPO3','PPO3'],
            'AM040':['ZI014_PRW','PRW'], 'AM040':['ZI014_PRW2','PRW2'], 'AM040':['ZI014_PRW3','PRW3'],
            'AM060':['ZI014_PPO','PPO'], 'AM060':['ZI014_PPO2','PPO2'], 'AM060':['ZI014_PPO3','PPO3'],
            'AM070':['ZI014_PPO','PPO'], 'AM070':['ZI014_PPO2','PPO2'], 'AM070':['ZI014_PPO3','PPO3'],
            'AM071':['ZI014_PPO','PPO'], 'AM071':['ZI014_PPO2','PPO2'], 'AM071':['ZI014_PPO3','PPO3'],
            'AM080':['ZI014_YWQ','YWQ'],
            'AQ059':['ZI016_WD1','WD1'],
            'AQ113':['ZI014_PPO','PPO'], 'AQ113':['ZI014_PPO2','PPO2'], 'AQ113':['ZI014_PPO3','PPO3'],
            'AQ116':['ZI014_PPO','PPO'], 'AQ116':['ZI014_PPO2','PPO2'], 'AQ116':['ZI014_PPO3','PPO3'],
            'AT005':['WLE','ZI025_WLE'],
            'AT042':['GUG','ZI032_GUG'], 'AT042':['PYC','ZI032_PYC'], 'AT042':['PYM','ZI032_PYM'],
            'AT042':['TOS','ZI032_TOS'], 'AT042':['CAB','AT005_CAB'],
            'BD100':['WLE','ZI025_WLE'],
            'BH051':['ZI014_PPO','PPO'], 'BH051':['ZI014_PPO2','PPO2'], 'BH051':['ZI014_PPO3','PPO3'],
            'DB029':['FFN','ZI071_FFN'], 'DB029':['FFN2','ZI071_FFN2'], 'DB029':['FFN3','ZI071_FFN3'],
            'ED010':['ZI024_HYP','HYP'],
            'GB045':['ZI019_ASU','ASU'], 'GB045':['ZI019_ASU2','ASU2'], 'GB045':['ZI019_ASU3','ASU3'],
            'BD115':['MAN','ZI025_MAN'],
            'AP055':['RIN_RTN','RTN'], 'AP055':['RIN_RTN2','RTN2'], 'AP055':['RIN_RTN3','RTN3'],
            'ZI031':['ZI006_MEM','MEM'], 'ZI031':['ZI004_RCG','RCG'],
            'ZI026':['ZI026_SUR','SUR']
                };

        // Shorter but more ugly version of a set of if..else if statements
        if (swapList[attrs.F_CODE] && attrs[swapList[attrs.F_CODE][0]])
        {
            attrs[swapList[attrs.F_CODE][1]] = attrs[swapList[attrs.F_CODE][0]];
            delete attrs[swapList[attrs.F_CODE][0]];
        }

        // Sort out the UUID
        if (tags.uuid)
        {
            var str = tags['uuid'].split(';'); 
            attrs.UFI = str[0].replace('{','').replace('}','');
        }
        else
        {
            attrs.UFI = createUuid().replace('{','').replace('}','');
        }

        // Custom Road rules
        // - Fix the "highway=" stuff that cant be done in the one2one rules
        if (attrs.F_CODE == 'AP030')
        {
            // If we havent fixed up the road type/class, have a go with the
            // highway tag.
            if (!attrs.RTY && !attrs.RIN_ROI)
            {
                switch (tags.highway)
                {
                    case 'motorway':
                        attrs.RIN_ROI = '2'; // National Motorway
                        attrs.RTY = '1'; // Motorway
                        break;

                    case 'trunk':
                        attrs.RIN_ROI = '3'; // National/Primary
                        attrs.RTY = '2'; // Limited Access Motorway
                        break;

                    case 'primary':
                        attrs.RIN_ROI = '3'; // National
                        attrs.RTY = '3'; // road: Road outside a BUA
                        break;

                    case 'secondary':
                        attrs.RIN_ROI = '4'; // Secondary
                        attrs.RTY = '3'; // road: Road outside a BUA
                        break;

                    case 'tertiary':
                        attrs.RIN_ROI = '5'; // Local
                        attrs.RTY = '3'; // road: Road outside a BUA
                        break;

                    case 'residential':
                    case 'unclassified':
                    case 'service':
                        attrs.RIN_ROI = '5'; // Local
                        attrs.RTY = '4'; // street: Road inside a BUA
                        break;

                    case 'road':
                        attrs.RIN_ROI = '-999999'; // No Information
                        attrs.RTY = '-999999'; // No Information
                } // End tags.highway switch
            }

        }

        // RLE vs LOC: Need to deconflict this for various features.
        // This is the list of features that can be "Above Surface". Other features use RLE (Relitive Level) instead.
        if (attrs.LOC == '45' && (['AT005','AQ113','BH065','BH110'].indexOf(attrs.TRS) == -1))
        {
            attrs.RLE = '2'; // Raised above surface
            attrs.LOC = '44'; // On Surface
        }

        // Clean up Cart Track attributes
        if (attrs.F_CODE == 'AP010')
        {
            if (attrs.TRS && (['3','4','6','11','21','22','999'].indexOf(attrs.TRS) == -1))
            {
                var othVal = '(TRS:' + attrs.TRS + ')';
                attrs.OTH = translate.appendValue(attrs.OTH,othVal,' ');
                attrs.TRS = '999';

            }
        }

        // Fix HGT and LMC to keep GAIT happy
        // If things have a height greater than 46m, tags them as being a "Navigation Landmark"
        if (attrs.HGT > 46 && !(attrs.LMC)) attrs.LMC = '1001';

        // Alt_Name:  AL020 Built Up Area & ZD070 Water Measurement Location are the _ONLY_ features in TDS
        // that have a secondary name.
        // We are going to push the Alt Name onto the end of the standard name field - ZI005_FNA
        if (attrs.ZI005_FNA2 && (attrs.F_CODE !== 'AL020' && attrs.F_CODE !== 'ZD070'))
        {
            attrs.ZI005_FNA = translate.appendValue(attrs.ZI005_FNA,attrs.ZI005_FNA2,';');
            delete attrs.ZI005_FNA2;
        }

        // Alt_Name2:  ZD070 Water Measurement Location is the _ONLY_ feature in TDS that has a
        // third name.
        // We are going to push the Alt Name onto the end of the standard name field - ZI005_FNA
        if (attrs.ZI005_FNA3 && attrs.F_CODE !== 'ZD070')
        {
            attrs.ZI005_FNA = translate.appendValue(attrs.ZI005_FNA,attrs.ZI005_FNA2,';');
            delete attrs.ZI005_FNA3;
        }

    }, // End applyToNfddPostProcessing

    // ##### End of the xxToNfddxx Block #####

    // toOsm - Translate Attrs to Tags
    // This is the main routine to convert _TO_ OSM
    toOsm : function(attrs, layerName)
    {
        tags = {};  // The final output Tag list

        // Debug:
        if (config.getOgrDebugDumpattrs() == 'true') for (var i in attrs) print('In Attrs:' + i + ': :' + attrs[i] + ':');

        if (tds61.lookup == undefined)
        {
            // Setup lookup tables to make translation easier. I'm assumeing that since this is not set, the 
            // other tables are not set either.
            
            // Support Import Only attributes
            tds61.rules.one2one.push.apply(tds61.rules.one2one,tds61.rules.one2oneIn);

            // Add the FCODE input rules
            // We add these since they don't conflict with the TDS one2one rules
            tds61.rules.one2one.push.apply(tds61.rules.one2one,fcodeCommon.one2one);
            tds61.rules.one2one.push.apply(tds61.rules.one2one,tds61.rules.fcodeOne2oneIn);
            
            tds61.lookup = translate.createLookup(tds61.rules.one2one);

            // Build an Object with both the SimpleText & SimpleNum lists
            tds61.ignoreList = translate.joinList(tds61.rules.numBiased, tds61.rules.txtBiased);
            
            // Add features to ignore
            tds61.ignoreList.F_CODE = '';
            tds61.ignoreList.FCSUBTYPE = '';
            tds61.ignoreList.UFI = '';
        }

        // pre processing
        tds61.applyToOsmPreProcessing(attrs, layerName);

        // one 2 one
        translate.applyOne2One(attrs, tags, tds61.lookup, {'k':'v'}, tds61.ignoreList);

        
        // apply the simple number and text biased rules
        translate.applySimpleNumBiased(attrs, tags, tds61.rules.numBiased, 'forward');
        translate.applySimpleTxtBiased(attrs, tags, tds61.rules.txtBiased, 'forward');

        // Crack open the OTH field and populate the appropriate attributes
        if (attrs.OTH) translate.processOTH(attrs, tags, tds61.lookup);

        // post processing
        tds61.applyToOsmPostProcessing(attrs, tags, layerName);
        
        // Debug:
        if (config.getOgrDebugDumptags() == 'true') 
        {
            for (var i in tags) print('Out Tags: ' + i + ': :' + tags[i] + ':');
            print('');
        }

        // Debug: Add the FCODE to the tags
        if (config.getOgrDebugAddfcode() == 'true') tags['raw:debugFcode'] = attrs.F_CODE;

        return tags;
    }, // End of toOsm


    // This gets called by translateToOGR and is where the main work gets done
    // We get Tags and return Attrs and a tableName
    // This is the main routine to convert _TO_ NFDD
    toNfdd : function(tags, elementType, geometryType)
    {
        var tableName = ''; // The final table name
        attrs = {}; // The output
        
        var tableName2 = ''; // The second table name - will populate if appropriate
        var attrs2 = {}; // The second feature - will populate if appropriate

        attrs.F_CODE = ''; // Initial setup
                
        // Start processing here
        // Debug:
        if (config.getOgrDebugDumptags() == 'true') for (var i in tags) print('In Tags: ' + i + ': :' + tags[i] + ':');

        // The Nuke Option: If we have a relation, drop the feature and carry on
        if (tags['building:part']) return null;

        // The Nuke Option: "Collections" are groups of different feature types: Point, Area and Line.  
        // There is no way we can translate these to a single TDS feature.
        if (geometryType == 'Collection') return null;

        // Set up the fcode translation rules. We need this due to clashes between the one2one and
        // the fcode one2one rules
        if (tds61.fcodeLookup == undefined)
        {
            // Add the FCODE rules for Export
            fcodeCommon.one2one.push.apply(fcodeCommon.one2one,tds61.rules.fcodeOne2oneOut);

            tds61.fcodeLookup = translate.createBackwardsLookup(fcodeCommon.one2one);
            // translate.dumpOne2OneLookup(tds61.fcodeLookup);
        }

        if (tds61.lookup == undefined)
        {
            // Add "other" rules to the one2one
            tds61.rules.one2one.push.apply(tds61.rules.one2one,tds61.rules.one2oneOut);

            tds61.lookup = translate.createBackwardsLookup(tds61.rules.one2one);
            // translate.dumpOne2OneLookup(tds61.lookup);
            
            // Build a list of things to ignore and flip is backwards
            tds61.ignoreList = translate.flipList(translate.joinList(tds61.rules.numBiased, tds61.rules.txtBiased));
            
            // Add some more features to ignore
            tds61.ignoreList.uuid = '';
            tds61.ignoreList.source = '';
            tds61.ignoreList.area = '';
            tds61.ignoreList['note:extra'] = '';
            tds61.ignoreList['hoot:status'] = '';
            tds61.ignoreList['error:circular'] = '';
        }

        // pre processing
        tds61.applyToNfddPreProcessing(tags, attrs, geometryType);

        // one 2 one - we call the version that knows about OTH fields
        translate.applyNfddOne2One(tags, attrs, tds61.lookup, tds61.fcodeLookup, tds61.ignoreList);

        // apply the simple number and text biased rules
        // Note: These are BACKWARD, not forward!
        translate.applySimpleNumBiased(attrs, tags, tds61.rules.numBiased, 'backward');
        translate.applySimpleTxtBiased(attrs, tags, tds61.rules.txtBiased, 'backward');

        // post processing
        // tds61.applyToNfddPostProcessing(attrs, tableName, geometryType);
        tds61.applyToNfddPostProcessing(tags, attrs, geometryType);

        // Now check for invalid feature geometry
        // E.g. If the spec says a runway is a polygon and we have a line, throw error and 
        // push the feature to o2s layer
        var gFcode = geometryType.toString().charAt(0) + attrs.F_CODE;

        if (!(nfddAttrLookup[gFcode])) 
        {
            logError('FCODE and Geometry: ' + gFcode + ' is not in the schema');

            tableName = 'o2s_' + geometryType.toString().charAt(0);

            // Debug:
            // Dump out what attributes we have converted before they get wiped out
            if (config.getOgrDebugDumpattrs() == 'true') for (var i in attrs) print('Converted Attrs:' + i + ': :' + attrs[i] + ':');

            for (var i in tags)
            {
                // Clean out all of the source: hoot: and error: tags to save space
                // if (i.indexOf('source:') !== -1) delete tags[i];
                if (i.indexOf('hoot:') !== -1) delete tags[i];
                if (i.indexOf('error:') !== -1) delete tags[i];
            }

            // Convert all of the Tags to a string so we can jam it into an attribute
            var str = JSON.stringify(tags);

            // If the tags are > 254 char, split into pieces. Not pretty but stops errors.
            // A nicer thing would be to arrange the tags until they fit neatly
            // Apparently Shape & FGDB can't handle fields > 254 chars. 
            if (str.length < 255 || config.getOgrSplitO2s() == 'false') 
            {
                // return {attrs:{tag1:str}, tableName: tableName};
                attrs = {tag1:str};
            }
            else
            {
                // Not good. Will fix with the rewrite of the tag splitting code
                if (str.length > 1012)
                {
                    logError('o2s tags truncated to fit in available space.');
                    str = str.substring(0,1012);
                }

                // return {attrs:{tag1:str.substring(0,253), tag2:str.substring(253)}, tableName: tableName};
                attrs = {tag1:str.substring(0,253), 
                         tag2:str.substring(253,506),
                         tag3:str.substring(506,759),
                         tag4:str.substring(759,1012)};
            }
        }
        else // We have a feature
        {
            // Check if we need to make a second feature.
            attrs2 = tds61.twoFeatures(geometryType,tags,attrs);

            // If we have a second feature, we need another layer name
            if (attrs2.F_CODE)
            {
                // Repeat the feature validation and adding attributes
                var gFcode2 = geometryType.toString().charAt(0) + attrs2.F_CODE;

                // Validate attrs: remove all that are not supposed to be part of a feature
                tds61.validateAttrs(geometryType,attrs2);

                // Now set the FCSubtype.help

                // NOTE: If we export to shapefile, GAIT _will_ complain about this
                if (config.getOgrTdsAddFcsubtype() == 'true') attrs2.FCSUBTYPE = tds61.rules.subtypeList[attrs2.F_CODE];

                // If we are using the TDS structure, fill the rest of the unused attrs in the schema
                if (config.getOgrTdsStructure() == 'true')
                {
                    tableName2 = tds61.rules.thematicGroupList[gFcode2];
                    tds61.validateTDSAttrs(gFcode2, attrs2);
                }
                else
                {
                    tableName2 = layerNameLookup[gFcode2.toUpperCase()];
                }
            } // End if attrs2.F_CODE

            // Validate attrs: remove all that are not supposed to be part of a feature
            tds61.validateAttrs(geometryType,attrs);

            // Now set the FCSubtype. 
            // NOTE: If we export to shapefile, GAIT _will_ complain about this
            if (config.getOgrTdsAddFcsubtype() == 'true') attrs.FCSUBTYPE = tds61.rules.subtypeList[attrs.F_CODE];

            // If we are using the TDS structre, fill the rest of the unused attrs in the schema
            if (config.getOgrTdsStructure() == 'true')
            {
                tableName = tds61.rules.thematicGroupList[gFcode];
                tds61.validateTDSAttrs(gFcode, attrs);
            }
            else
            {
                tableName = layerNameLookup[gFcode.toUpperCase()]; 
            }

        } // End else We have a feature

        // Debug:
        if (config.getOgrDebugDumpattrs() == 'true' || config.getOgrDebugDumptags() == 'true')
        {
            print('TableName: ' + tableName + '  FCode: ' + attrs.F_CODE + '  Geom: ' + geometryType);
            if (tableName2 !== '') print('TableName2: ' + tableName2 + '  FCode: ' + attrs2.F_CODE + '  Geom: ' + geometryType);
        }

        // Debug:
        if (config.getOgrDebugDumpattrs() == 'true')
        {
            for (var i in attrs) print('Out Attrs:' + i + ': :' + attrs[i] + ':');
            if (attrs2.F_CODE) for (var i in attrs2) print('2Out Attrs:' + i + ': :' + attrs2[i] + ':');
            print('');
        }

        var returnData = [{attrs:attrs, tableName: tableName}];

        if (attrs2.F_CODE)
        {
            returnData.push({attrs: attrs2, tableName: tableName2});
        }

        // Look for Review tags and push them to a review layer if found
        if (tags['hoot:review:needs'] == 'yes')
        {
            var reviewAttrs = {};

            // Note: Some of these may be "undefined"
            reviewAttrs.note = tags['hoot:review:note'];
            reviewAttrs.score = tags['hoot:review:score'];
            reviewAttrs.uuid = tags.uuid;
            reviewAttrs.source = tags['hoot:review:source'];

            var reviewTable = 'review_' + geometryType.toString().charAt(0);
            returnData.push({attrs: reviewAttrs, tableName: reviewTable});
        }

        return returnData;

    } // End of toNfdd

} // End of tds61
