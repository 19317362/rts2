"""Class definitions for rts2_autofocus"""
# (C) 2010, Markus Wildi, markus.wildi@one-arcsec.org
#
#   usage 
#   rts_autofocus.py --help
#   
#   see man 1 rts2_autofocus.py
#   see rts2_autofocus_unittest.py for unit tests
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2, or (at your option)
#   any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software Foundation,
#   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
#   Or visit http://www.gnu.org/licenses/gpl.html.
#
# required packages:
# wget 'http://argparse.googlecode.com/files/argparse-1.1.zip

__author__ = 'markus.wildi@one-arcsec.org'

import argparse
import logging
from operator import itemgetter
# globals
LOG_FILENAME = '/var/log/rts2-autofocus'
logger= logging.getLogger('rts2_af_logger') ;


class AFScript:
    """Class for any AF script"""
    def __init__(self, scriptName):
        self.scriptName= scriptName

    def arguments( self): 

        parser = argparse.ArgumentParser(description='RTS2 autofocus', epilog="See man 1 rts2-autofocus.py for mor information")
#        parser.add_argument(
#            '--write', action='store', metavar='FILE NAME', 
#            type=argparse.FileType('w'), 
#            default=sys.stdout,
#            help='the file name where the default configuration will be written default: write to stdout')

        parser.add_argument(
            '-w', '--write', action='store_true', 
            help='write defaults to configuration file ' + runTimeConfig.configurationFileName())

        parser.add_argument('--config', dest='fileName',
                            metavar='CONFIG FILE', nargs=1, type=str,
                            help='configuration file')

        parser.add_argument('-r', '--reference', dest='reference',
                            metavar='REFERENCE', nargs=1, type=str,
                            help='reference file name')

        parser.add_argument('-l', '--logging', dest='logLevel', action='store', 
                            metavar='LOGLEVEL', nargs=1, type=str,
                            default='warning',
                            help=' log level: usual levels')

#        parser.add_argument('-t', '--test', dest='test', action='store', 
#                            metavar='TEST', nargs=1,
#    no default means None                        default='myTEST',
#                            help=' test case, default: myTEST')

        parser.add_argument('-v', dest='verbose', 
                            action='store_true',
                            help=' print (some) messages to stdout'
                            )

        self.args= parser.parse_args()

        if(self.args.verbose):
            global verbose
            verbose= self.args.verbose
            runTimeConfig.dumpDefaults()

        if( self.args.write):
            runTimeConfig.writeDefaultConfiguration()
            print 'wrote default configuration to ' +  runTimeConfig.configurationFileName()
            sys.exit(0)

        return  self.args

    def configureLogger(self):

        if( self.args.logLevel[0]== 'debug'): 
            logging.basicConfig(filename=LOG_FILENAME,level=logging.DEBUG)
        else:
            logging.basicConfig(filename=LOG_FILENAME,level=logging.WARN)

        return logger

import ConfigParser
import string

class Configuration:
    """Configuration for any AFScript"""
    

    def __init__(self, fileName='rts2-autofocus-offline.cfg'):
        self.configFileName = fileName
        self.values={}
        self.filters=[]
        self.filtersInUse=[]

        self.cp={}

        self.config = ConfigParser.RawConfigParser()
        
        self.cp[('basic', 'CONFIGURATION_FILE')]= '/etc/rts2/autofocus/rts2-autofocus.cfg'
        
        self.cp[('basic', 'BASE_DIRECTORY')]= '/tmp'
        self.cp[('basic', 'TEMP_DIRECTORY')]= '/tmp'
        self.cp[('basic', 'FILE_GLOB')]= 'X/*fits'
        self.cp[('basic', 'FITS_IN_BASE_DIRECTORY')]= False
        self.cp[('basic', 'CCD_CAMERA')]= 'CD'
        self.cp[('basic', 'CHECK_RTS2_CONFIGURATION')]= False

        self.cp[('filter properties', 'U')]= '[0, U, 5074, -1500, 1500, 100, 40]'
        self.cp[('filter properties', 'B')]= '[1, B, 4712, -1500, 1500, 100, 30]'
        self.cp[('filter properties', 'V')]= '[2, V, 4678, -1500, 1500, 100, 20]'
        self.cp[('filter properties', 'R')]= '[4, R, 4700, -1500, 1500, 100, 20]'
        self.cp[('filter properties', 'I')]= '[4, I, 4700, -1500, 1500, 100, 20]'
        self.cp[('filter properties', 'X')]= '[5, X, 3270, -1500, 1500, 100, 10]'
        self.cp[('filter properties', 'Y')]= '[6, Y, 3446, -1500, 1500, 100, 10]'
        self.cp[('filter properties', 'NOFILTER')]= '[6, NOFILTER, 3446, -1500, 1500, 109, 19]'
        
        self.cp[('focuser properties', 'FOCUSER_RESOLUTION')]= 20
        self.cp[('focuser properties', 'FOCUSER_ABSOLUTE_LOWER_LIMIT')]= 1501
        self.cp[('focuser properties', 'FOCUSER_ABSOLUTE_UPPER_LIMIT')]= 6002

        self.cp[('acceptance circle', 'CENTER_OFFSET_X')]= 0
        self.cp[('acceptance circle', 'CENTER_OFFSET_Y')]= 0
        self.cp[('acceptance circle', 'RADIUS')]= 2000.
        
        self.cp[('filters', 'filters')]= 'U:B:V:R:I:X:Y'
        
        self.cp[('DS9', 'DS9_REFERENCE')]= True
        self.cp[('DS9', 'DS9_MATCHED')]= True
        self.cp[('DS9', 'DS9_ALL')]= True
        self.cp[('DS9', 'DS9_DISPLAY_ACCEPTANCE_AREA')]= True
        self.cp[('DS9', 'DS9_REGION_FILE')]= 'ds9-autofocus.reg'
        
        self.cp[('analysis', 'ANALYSIS_UPPER_LIMIT')]= 1.e12
        self.cp[('analysis', 'ANALYSIS_LOWER_LIMIT')]= 0.
        self.cp[('analysis', 'MINIMUM_OBJECTS')]= 5
        self.cp[('analysis', 'INCLUDE_AUTO_FOCUS_RUN')]= False
        self.cp[('analysis', 'SET_LIMITS_ON_SANITY_CHECKS')]= True
        self.cp[('analysis', 'SET_LIMITS_ON_FILTER_FOCUS')]= True
        self.cp[('analysis', 'FIT_RESULT_FILE')]= 'fit-autofocus.dat'
        
        self.cp[('fitting', 'FOCROOT')]= 'rts2-fit-focus'
        
        self.cp[('SExtractor', 'SEXPRG')]= 'sex 2>/dev/null'
        self.cp[('SExtractor', 'SEXCFG')]= '/etc/rts2/autofocus/sex-autofocus.cfg'
        self.cp[('SExtractor', 'SEXPARAM')]= '/etc/rts2/autofocus/sex-autofocus.param'
        self.cp[('SExtractor', 'SEXREFERENCE_PARAM')]= '/etc/rts2/autofocus/sex-autofocus-reference.param'
        self.cp[('SExtractor', 'OBJECT_SEPARATION')]= 10.
        self.cp[('SExtractor', 'ELLIPTICITY')]= .1
        self.cp[('SExtractor', 'ELLIPTICITY_REFERENCE')]= .1
        self.cp[('SExtractor', 'SEXSKY_LIST')]= 'sex-autofocus-assoc-sky.list'
        self.cp[('SExtractor', 'SEXCATALOGUE')]= 'sex-autofocus.cat'
        self.cp[('SExtractor', 'SEX_TMP_CATALOGUE')]= 'sex-autofocus-tmp.cat'
        self.cp[('SExtractor', 'CLEANUP_REFERENCE_CATALOGUE')]= True
        
        self.cp[('mode', 'TAKE_DATA')]= True
        self.cp[('mode', 'SET_FOCUS')]= True
        self.cp[('mode', 'CCD_BINNING')]= 0
        self.cp[('mode', 'AUTO_FOCUS')]= False
        self.cp[('mode', 'NUMBER_OF_AUTO_FOCUS_IMAGES')]= 10
        
        self.cp[('rts2', 'RTS2_DEVICES')]= '/etc/rts2/devices'
        
        self.defaults={}
        for (section, identifier), value in sorted(self.cp.iteritems()):
            self.defaults[(identifier)]= value
#            print 'value ', self.defaults[(identifier)]
 
    def configurationFileName(self):
        return self.configFileName

    def configIdentifiers(self):
        return sorted(self.cp.iteritems())

    def defaultsIdentifiers(self):
        return sorted(self.defaults.iteritems())
        
    def defaultsValue( self, identifier):
        return self.defaults[ identifier]

    def dumpDefaults(self):
        for (identifier), value in self.configIdentifiers():
            print "dump defaults :", ', ', identifier, '=>', value

    def valuesIdentifiers(self):
        return sorted(self.values.iteritems())
    # why not values?
    #  runTimeConfig.values('SEXCFG'), TypeError: 'dict' object is not callable
    def value( self, identifier):
        return self.values[ identifier]

    def dumpValues(self):
        for (identifier), value in self.valuesIdentifiers():
            print "dump values :", ', ', identifier, '=>', value

    def filters(self):
        return self.filters

    def filterByName(self, name):
        for filter in  self.filters:
            print "NAME>" + name + "<>" + filter.filterName
            if( name == filter.filterName):
                print "NAME" + name + filter.filterName
                return filter

        return False

    def filtersInUse(self):
        return self.filtersInUse

    def filterInUseByName(self, name):
        for filter in  self.filtersInUse:
            if( name == filter.filterName):
                return filter
        return False

    def writeDefaultConfiguration(self):
        for (section, identifier), value in sorted(self.cp.iteritems()):
#            print section, '=>', identifier, '=>', value
            if( self.config.has_section(section)== False):
                self.config.add_section(section)

            self.config.set(section, identifier, value)

        with open( self.configFileName, 'wb') as configfile:
            configfile.write('# 2010-07-10, Markus Wildi\n')
            configfile.write('# default configuration for rts2-autofocus.py\n')
            configfile.write('# generated with rts2-autofous.py -p\n')
            configfile.write('# see man rts2-autofocus.py for details\n')
            configfile.write('#\n')
            configfile.write('#\n')
            self.config.write(configfile)

    def readConfiguration( self, configFileName):

        config = ConfigParser.ConfigParser()
        try:
            config.readfp(open(configFileName))
        except:
            logger.error('Configuration.readConfiguration: config file ' + configFileName + ' not found, exiting')
            sys.exit(1)

# read the defaults
        for (section, identifier), value in self.configIdentifiers():
             self.values[identifier]= value

# over write the defaults
        for (section, identifier), value in self.configIdentifiers():

            try:
                value = config.get( section, identifier)
            except:
                logger.info('Configuration.readConfiguration: no section ' +  section + ' or identifier ' +  identifier + ' in file ' + configFileName)
# first bool, then int !
            if(isinstance(self.values[identifier], bool)):
#ToDO, looking for a direct way
                if( value == 'True'):
                    self.values[identifier]= True
                else:
                    self.values[identifier]= False
            elif( isinstance(self.values[identifier], int)):
                try:
                    self.values[identifier]= int(value)
                except:
                    logger.error('Configuration.readConfiguration: no int '+ value+ 'in section ' +  section + ', identifier ' +  identifier + ' in file ' + configFileName)
                    

            elif(isinstance(self.values[identifier], float)):
                try:
                    self.values[identifier]= float(value)
                except:
                    logger.error('Configuration.readConfiguration: no float '+ value+ 'in section ' +  section + ', identifier ' +  identifier + ' in file ' + configFileName)

            else:
                self.values[identifier]= value
                items=[] ;
                if( section == 'filter properties'): 
                    items= value[1:-1].split(',')
#, ToDo, hm

                    for item in items: 
                        item= string.replace( item, ' ', '')

                    items[1]= string.replace( items[1], ' ', '')
                    print '-----------filterInUse---------:>' + items[1] + '<'
                    self.filters.append( Filter( items[1], string.atoi(items[2]), string.atoi(items[3]), string.atoi(items[4]), string.atoi(items[5]), string.atoi(items[6])))
                elif( section == 'filters'):
                    items= value.split(':')
                    for item in items:
                        item.replace(' ', '')
                        if(verbose):
                            print 'filterInUse---------:>' + item + '<'
                        self.filtersInUse.append(item)

        if(verbose):
            for (identifier), value in self.defaultsIdentifiers():
               print "over ", identifier, self.values[(identifier)]

        if(verbose):
           for filter in self.filters:
               print 'Filter --------' + filter.filterName
               print 'Filter --------%d'  % filter.exposure 

        if(verbose):
            for filter in self.filtersInUse:
                print "used filters :", filter

        return self.values

class SExtractorParams():
    def __init__(self, paramsFileName=None):
# ToDo, calrify 
        if( paramsFileName== None):
            self.paramsFileName= runTimeConfig.value('SEXREFERENCE_PARAM')
        else:
            self.paramsFileName= paramsFileName
        self.paramsSExtractorReference= []
        self.paramsSExtractorAssoc= []

    def lengthAssoc(self):
        return len(self.paramsSExtractorAssoc)

    def elementIndex(self, element):
        for ele in  self.paramsSExtractorAssoc:
            if( element== ele) : 
                return self.paramsSExtractorAssoc.index(ele)
        else:
            return False

    def readSExtractorParams(self):
        params=open( self.paramsFileName, 'r')
        lines= params.readlines()
        pElement = re.compile( r'([\w]+)')
        for (i, line) in enumerate(lines):
            element = pElement.match(line)
            if( element):
                if(verbose):
                    print "element.group(1) : ", element.group(1)
                self.paramsSExtractorReference.append(element.group(1))
                self.paramsSExtractorAssoc.append(element.group(1))

# current structure of the reference catalogue
#   1 NUMBER                 Running object number
#   2 X_IMAGE                Object position along x                                    [pixel]
#   3 Y_IMAGE                Object position along y                                    [pixel]
#   4 FLUX_ISO               Isophotal flux                                             [count]
#   5 FLUX_APER              Flux vector within fixed circular aperture(s)              [count]
#   6 FLUXERR_APER           RMS error vector for aperture flux(es)                     [count]
#   7 MAG_APER               Fixed aperture magnitude vector                            [mag]
#   8 MAGERR_APER            RMS error vector for fixed aperture mag.                   [mag]
#   9 FLUX_MAX               Peak flux above background                                 [count]
#  10 ISOAREA_IMAGE          Isophotal area above Analysis threshold                    [pixel**2]
#  11 FLAGS                  Extraction flags
#  12 FWHM_IMAGE             FWHM assuming a gaussian core                              [pixel]
#  13 FLUX_RADIUS            Fraction-of-light radii                                    [pixel]
#  14 ELLIPTICITY            1 - B_IMAGE/A_IMAGE

# current structure of the associated catalogue
#   1 NUMBER                 Running object number
#   2 X_IMAGE                Object position along x                                    [pixel]
#   3 Y_IMAGE                Object position along y                                    [pixel]
#   4 FLUX_ISO               Isophotal flux                                             [count]
#   5 FLUX_APER              Flux vector within fixed circular aperture(s)              [count]
#   6 FLUXERR_APER           RMS error vector for aperture flux(es)                     [count]
#   7 MAG_APER               Fixed aperture magnitude vector                            [mag]
#   8 MAGERR_APER            RMS error vector for fixed aperture mag.                   [mag]
#   9 FLUX_MAX               Peak flux above background                                 [count]
#  10 ISOAREA_IMAGE          Isophotal area above Analysis threshold                    [pixel**2]
#  11 FLAGS                  Extraction flags
#  12 FWHM_IMAGE             FWHM assuming a gaussian core                              [pixel]
#  13 FLUX_RADIUS            Fraction-of-light radii                                    [pixel]
#  14 ELLIPTICITY            1 - B_IMAGE/A_IMAGE
#  15 VECTOR_ASSOC           ASSOCiated parameter vector
#  29 NUMBER_ASSOC           Number of ASSOCiated IDs
            
        for element in self.paramsSExtractorReference:
            self.paramsSExtractorAssoc.append('ASSOC_'+ element)

        self.paramsSExtractorAssoc.append('NUMBER_ASSOC')

        for element in  self.paramsSExtractorReference:
            print "Reference element : >"+ element+"<"
        for element in  self.paramsSExtractorAssoc:
            print "Association element : >" + element+"<"


    def index(self, element):
        return self.paramsSExtractorAssoc(element)

class Filter:
    """Class for filter properties"""

    def __init__(self, filterName, focDef, lowerLimit, upperLimit, resolution, exposure, ):
        self.filterName= filterName
        self.focDef    = focDef
        self.lowerLimit= lowerLimit
        self.upperLimit= upperLimit
        self.stepSize  = resolution # [tick]
        self.exposure  = exposure

    def filerName(self):
        return self.filterName
    def focDef(self):
        return self.focDef 
    def lowerLimit(self):
        return self.lowerLimit
    def upperLimit(self):
        return  self.upperLimit
    def stepSize(self):
        return self.stepSize
    def exposure(self):
        return self.exposure

import shlex
import subprocess
import re
class Catalogue():
    """Class for a catalogue (SExtractor result)"""
    def __init__(self, fitsHDU=None):
        self.fitsHDU  = fitsHDU
        self.catalogueFileName= serviceFileOp.expandToCat(self.fitsHDU)
        self.catalogue = []
        self.positions = []
        self.elements  = {}
        self.isReference = False
        self.isValid= False
        Catalogue.__lt__ = lambda self, other: self.fitsHDU.headerElements['FOC_POS'] < other.fitsHDU.headerElements['FOC_POS']

    def fitsHDU(self):
        return self.fitsHDU

    def isReference(self):
        return self.isReference

    def extractToCatalogue(self):
        if( verbose):
            print 'sextractor  ' + runTimeConfig.value('SEXPRG')
            print 'sextractor  ' + runTimeConfig.value('SEXCFG')
            print 'sextractor  ' + runTimeConfig.value('SEXREFERENCE_PARAM')

        # 2>/dev/null swallowed by PIPE
        (prg, arg)= runTimeConfig.value('SEXPRG').split(' ')

        if( self.fitsHDU.isReference):
            self.isReference = True 
            cmd= [  prg,
                    self.fitsHDU.fitsFileName, 
                    '-c ',
                    runTimeConfig.value('SEXCFG'),
                    '-CATALOG_NAME',
                    serviceFileOp.expandToSkyList(self.fitsHDU),
                    '-PARAMETERS_NAME',
                    runTimeConfig.value('SEXREFERENCE_PARAM'),]
        else:
            cmd= [  prg,
                    self.fitsHDU.fitsFileName, 
                    '-c ',
                    runTimeConfig.value('SEXCFG'),
                    '-CATALOG_NAME',
                    self.catalogueFileName,
                    '-PARAMETERS_NAME',
                    runTimeConfig.value('SEXPARAM'),
                    '-ASSOC_NAME',
                    serviceFileOp.expandToSkyList(self.fitsHDU)
                    ]
        try:
            output = subprocess.Popen( cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()[0]

        except OSError as (errno, strerror):
            logging.error( 'Catalogue.extractToCatalogue: I/O error({0}): {1}'.format(errno, strerror))
            sys.exit(1)

        except:
            logging.error('Catalogue.extractToCatalogue: '+ repr(cmd) + ' died')
            sys.exit(1)

# checking against reference items see http://www.wellho.net/resources/ex.php4?item=y115/re1.py
    def createCatalogue(self, SExtractorParams=None):
        if( SExtractorParams==None):
            logger.error( 'Catalogue.createCatalogue: no SExtractor parameter configuration given')
            return False

        if( not self.fitsHDU.isReference):
            cat= open( self.catalogueFileName, 'r')
        else:
            cat= open( serviceFileOp.expandToSkyList(self.fitsHDU), 'r')
        lines= cat.readlines()
        # rely on the fact that first line is stored as first element in list
        # match, element, data
        ##   2 X_IMAGE                Object position along x                                    [pixel]
        #         1   2976.000      7.473     2773.781     219.8691     42.19405  -5.8554   0.2084     120.5605         6  26     4.05      5.330    0.823
 
        pElement = re.compile( r'#[ ]+([0-9]+)[ ]+([\w]+)')
        pData    = re.compile( r'')

        for (lineNumber, line) in enumerate(lines):
            element = pElement.match(line)
            data    = pData.match(line)
            if( element):
#                if(verbose):
#                    print "element.group(1) : ", element.group(1)
#                    print "element.group(2) : ", element.group(2)
            
                if( not ( element.group(2)== 'VECTOR_ASSOC')):
                    try:
                        SExtractorParams.paramsSExtractorAssoc.index(element.group(2))
                    except:
                        logger.error( 'Catalogue.createCatalogue: no matching element for ' + element.group(2) +' found')
                        break
            elif( data):
#                if(verbose):
#                    print line
                items= line.split()
                objectNumber= items[0] # NUMBER
#                if(verbose):
#                    for item in items:
#                        print item

                for (j, item) in enumerate(items):
#                    print "file " + self.fitsHDU.fitsFileName + "======= %d" % j + ' value ' + item + ' name ' + SExtractorParams.paramsSExtractorAssoc[j] 

                    if( item == 'X_IMAGE'):
                        x= value
                    if( item == 'Y_IMAGE'):
                        y= value

                    try:
                        self.catalogue.append((int(objectNumber), int(j), SExtractorParams.paramsSExtractorAssoc[j], float(item)))
                    except:
                        print 'readCatalogue: exception on line %d' % j + ' ' + line 
                        break
            else:
                logger.error( 'Catalogue.readCatalogue: no match on line %d' % lineNumber)
                logger.error( 'Catalogue.readCatalogue: ' + line)
                break

            try:
                self.positions.append((int(objectNumber), x, y))
            except:
                logger.error( 'Catalogue.readCatalogue: no (x,y) positions on line %d' % lineNumber)
                logger.error( 'Catalogue.readCatalogue: ' + line)

        else: # exhausted 
            logger.error( 'Catalogue.readCatalogue: catalogue created ' + self.fitsHDU.fitsFileName)
            self.isValid= True

        return self.isValid

    def printObject(self, objectNumber):
        for (oNr, pos, item, value) in self.catalogue:
            if( objectNumber== oNr):
                print "Object number %d" %  oNr + repr((oNr, pos, item, value))
        else:
             print "Object number %d" % objectNumber + " is not there anymore"

    def removeObject(self, objectNumber):
        # ToDo, hm, if not sorted not all item.objectNumber are removed
        for (oNr, pos, item, value) in sorted(self.catalogue):
            if( objectNumber== oNr):
                self.catalogue.remove((oNr, pos, item, value))

    def opjectPosition(self, objectNumber):
        for object in positions:
            if( objectNumber== objNr[0]):
                return object

    def cleanUpReference(self, paramsSexctractor):
        if( not self.catalogue):
            logger.error( 'Catalogue.cleanUpReference: clean up only for a reference catalogue') 
            return False
 
        for (objectNumber, pos, item, value) in sorted(tup[0:-len(paramsSexctractor.lengthAssoc.length())], key=itemgetter(0,1), reverse=True):
            print "===============%d"% objectNumber +  " x %f" % self.positions + " %f" % value
#            for (objectNumberI, posI, itemI, valueI) in sorted(tup[0:-(2*(paramsSexctractor.lengthAssoc.length()))], key=itemgetter(0,1), reverse=True):
#                pass

    def prinPositions(self):
        for (objectNumber, x, y) in sorted( self.positions, key=itemgetter(0,1)):
            print "===" + repr((objectNumber, x, y))
            if( int(objectNumber) > 3):
                print objectNumber
                return

    def printCatalogue(self, name=None):
        if( name== None):
            for (objectNumber, pos, item, value) in sorted( self.catalogue, key=itemgetter(0,1)):
                print "===" + repr((objectNumber, pos, item, value))
                if( int(objectNumber) > 3):
                    print objectNumber
                    return


# example how to access the catalogue
    def average(self, variable):
        sum= 0
        i= 0
        for (objectNumber,pos,item, value) in self.catalogue:
            if( item == variable):
                sum += float(value)
                i += 1

        if(verbose):
            if( i != 0):
                print 'average ' + variable + ' %f ' % (sum/ float(i)) 
                return (sum/ float(i))
            else:
                print 'Error in average i=0'
                return False

class Catalogues():
    """Class holding Catalogues"""
    def __init__(self):
        self.CataloguesList= []
        self.isReference = False
        self.isValid= False

    def append( self, Catalogue):
        self.CataloguesList.append(Catalogue)

    def catalogues(self):
        return self.CataloguesList

    def isValid():
        return self.isValid

    def findReference(self):
        for cat in self.CataloguesList:
            if( cat.isReference):
                if( verbose):
                    print 'Catalogues.findReference: reference catalogue found for file: ' + cat.fitsHDU.fitsFileName
                return cat
        if( verbose):
            print 'Catalogues: no reference fits file found'
        return False

    def validate(self):
        catr= self.findReference()
        if( catr== False):
            return self.isValid

        if( len(self.CataloguesList) > 1):
            # default is False
            for cat in self.CataloguesList:
                if( cat.isValid== True):
                    if( verbose):
                        print 'Catalogues.validate: valid cat for: ' + cat.fitsHDU.fitsFileName
                    continue
                else:
                    if( verbose):
                        print 'Catalogues.validate: removed cat for: ' + cat.fitsHDU.fitsFileName

                    try:
                        self.CataloguesList.remove(cat)
                    except:
                        logger.error('Catalogues.validate: could not remove cat for ' + cat.fitsHDU.fitsFileName)
                        if(verbose):
                            print "Catalogues.validate: failed to removed cat: " + cat.fitsHDU.fitsFileName
                    break
            else:
                if( len(self.CataloguesList) > 0):
                    self.isValid= True
                    return self.isValid


        return False
 

import sys
import pyfits

class FitsHDU():
    """Class holding fits file name and ist properties"""
    def __init__(self, fitsFileName=None, isReference=False):
        self.fitsFileName= fitsFileName
        self.isReference= isReference
        self.isValid= False
        self.headerElements={}
        FitsHDU.__lt__ = lambda self, other: self.headerElements['FOC_POS'] < other.headerElements['FOC_POS']

    def keys(self):
        return self.headerElements.keys()

    def isFilter(self, filter):
        if( verbose):
            print "fits file path: " + self.fitsFileName
        try:
            fitsHDU = pyfits.fitsopen(self.fitsFileName)
        except:
            if( self.fitsFileName):
                logger.error('FitsHDU: file not found : ' + self.fitsFileName)
            else:
                logger.error('FitsHDU: file name not given (None), exiting')
            sys.exit(1)

        fitsHDU.close()
#        if( verbose):
#            fitsHDU.info()

        if( filter == fitsHDU[0].header['FILTER']):
            self.headerElements['FILTER'] = fitsHDU[0].header['FILTER']
            self.headerElements['FOC_POS']= fitsHDU[0].header['FOC_POS']
            self.headerElements['BINNING']= fitsHDU[0].header['BINNING']
            self.headerElements['NAXIS1'] = fitsHDU[0].header['NAXIS1']
            self.headerElements['NAXIS2'] = fitsHDU[0].header['NAXIS2']
            self.headerElements['ORIRA']  = fitsHDU[0].header['ORIRA']
            self.headerElements['ORIDEC'] = fitsHDU[0].header['ORIDEC']
            return True
        else:
            return False

class FitsHDUs():
    """Class holding FitsHDU"""
    def __init__(self):
        self.fitsHDUsList= []
        self.isValid= False

    def append(self, FitsHDU):
        self.fitsHDUsList.append(FitsHDU)

    def fitsHDUs(self):
        return self.fitsHDUsList

    def findFintsFileByName( self, fileName):
        for hdu in sorted(self.fitsHDUsList):
            if( fileName== hdu.fitsFileName):
                return hdu
        return False

    def findReference(self):
        for hdu in sorted(self.fitsHDUsList):
            if( hdu.isReference):
                return hdu
        return False

    def findReferenceByFocPos(self, filterName):
        filter=  runTimeConfig.filterByName(filterName)
#        for hdu in sorted(self.fitsHDUsList):
        for hdu in sorted(self.fitsHDUsList):
            if( filter.focDef < hdu.headerElements['FOC_POS']):
                print "FOUND reference by foc pos ==============" + hdu.fitsFileName + "======== %d" % hdu.headerElements['FOC_POS']
                return hdu
        return False

    def isValid(self):
        return self.isValid

    def validate(self):
        hdur= self.findReference()
        if( hdur== False):
            hdur= self.findReferenceByFocPos()
            if( hdur== False):
                if( verbose):
                    print "Nothing found in  findReference and findReferenceByFocPos"
                    return self.isValid

        keys= hdur.keys()
        i= 0
        for hdu in self.fitsHDUsList:
            for key in keys:
                if(key == 'FOC_POS'):
                    continue
                if( hdur.headerElements[key]!= hdu.headerElements[key]):
                    try:
                        self.fitsHDUsList.remove(hdu)
                    except:
                        logger.error('FitsHDUs.validate: could not remove hdu for ' + hdu.fitsFileName)
                        if(verbose):
                            print "FitsHDUs.validate: removed hdu: " + hdu.fitsFileName + "========== %d" %  self.fitsHDUsList.index(hdu)
                    break # really break out the keys loop 
            else: # exhausted
                hdu.isValid= True
                if( verbose):
                    print 'FitsHDUs.validate: valid hdu: ' + hdu.fitsFileName


        if( len(self.fitsHDUsList) > 0):
            self.isValid= True

        return self.isValid

import time
import datetime
import glob
class ServiceFileOperations():
    """Class performing various task on files, e.g. expansio to (full) path"""
    def __init__(self):
        self.now= datetime.datetime.now().isoformat()

    def prefix( self):
        return 'rts2af-'

    def notFits(self, fileName):
        items= fileName.split('/')[-1]
        return items.split('.fits')[0]

    def expandToTmp( self, fileName=None):
        if( fileName==None):
            logger.error('ServiceFileOperations.expandToTmp: no file name given')

        fileName= runTimeConfig.value('TEMP_DIRECTORY') + '/'+ fileName
        return fileName

    def expandToSkyList( self, fitsHDU=None):
        if( fitsHDU==None):
            logger.error('ServiceFileOperations.expandToCat: no hdu given')
        
        items= runTimeConfig.value('SEXSKY_LIST').split('.')
        fileName= self.prefix() + items[0] +  '-' + fitsHDU.headerElements['FILTER'] + '-' +   self.now + '.' + items[1]
        if(verbose):
            print 'ServiceFileOperations:expandToSkyList expanded to ' + fileName
        
        return  self.expandToTmp(fileName)

    def expandToCat( self, fitsHDU=None):
        if( fitsHDU==None):
            logger.error('ServiceFileOperations.expandToCat: no hdu given')
        fileName= self.prefix() + self.notFits(fitsHDU.fitsFileName) + '-' + fitsHDU.headerElements['FILTER'] + '-' + self.now + '.cat'
        return self.expandToTmp(fileName)

#    def expandToPath( self, fileName=None):
#        if( fileName==None):
#            logger.error('ServiceFileOperations.expandToCat: no file name given')
        

    def findFitsHDUs( self):
        if( verbose):
            print "searching in " + runTimeConfig.value('BASE_DIRECTORY') + '/' + runTimeConfig.value('FILE_GLOB')
        return glob.glob( runTimeConfig.value('BASE_DIRECTORY') + '/' + runTimeConfig.value('FILE_GLOB'))

#
# stub, will be called in main script 
runTimeConfig= Configuration()
serviceFileOp= ServiceFileOperations()
verbose= False
class Service():
    """Temporary class for uncategorized methods"""
    def __init__(self):
        print
