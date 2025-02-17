#!/usr/bin/python
# -*- coding: utf-8 -*-
################################################################################
### @brief creates swagger json files from doc headers of rest files
###
### find files in
###   arangod/RestHandler/*.cpp
###   js/actions/api-*.js
###
### @usage generateSwagger.py < RestXXXX.cpp > restSwagger.json
###
### @file
###
### DISCLAIMER
###
### Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
###
### Licensed under the Apache License, Version 2.0 (the "License");
### you may not use this file except in compliance with the License.
### You may obtain a copy of the License at
###
###     http://www.apache.org/licenses/LICENSE-2.0
###
### Unless required by applicable law or agreed to in writing, software
### distributed under the License is distributed on an "AS IS" BASIS,
### WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
### See the License for the specific language governing permissions and
### limitations under the License.
###
### Copyright holder is triAGENS GmbH, Cologne, Germany
###
### @author Dr. Frank Celler
### @author Thomas Richter
### @author Copyright 2014, triAGENS GmbH, Cologne, Germany
################################################################################

import sys, re, json, string, os, os.path, operator
#from pygments import highlight
#from pygments.lexers import YamlLexer
#from pygments.formatters import TerminalFormatter

#, yaml
#import ruamel.yaml as yaml
rc = re.compile
MS = re.M | re.S

################################################################################
### @brief swagger
################################################################################

swagger = {
  "swagger": "2.0",
  "info": {
    "description": "ArangoDB REST API Interface",
    "version": "1.0",
    "title": "ArangoDB",
    "license": {
      "name": "Apache License, Version 2.0"
    }
  },
  "basePath": "/",
  "schemes": [
    "http"
  ],
  "definitions": {},
  "paths" : {}
}

################################################################################
### @brief native swagger types
################################################################################

swaggerBaseTypes = [
    'object',
    'array',
    'integer',
    'long',
    'float',
    'double',
    'string',
    'byte',
    'binary',
    'boolean',
    'date',
    'dateTime',
    'password'
]

################################################################################
### @brief length of the swagger definition namespace
################################################################################

defLen = len('#/definitions/')

################################################################################
### @brief operation
################################################################################

httpPath = ''

################################################################################
### @brief operation
################################################################################

method = ''

################################################################################
### @brief operation
################################################################################

operation = {}

################################################################################
### @brief current filename
################################################################################

fn = ''

################################################################################
### @brief current section
################################################################################

currentTag = ''

################################################################################
### @brief current docublock
################################################################################

currentDocuBlock = None
lastDocuBlock = None

################################################################################
### @brief index of example block we're reading
################################################################################

currentExample = 0

################################################################################
### @brief the current returncode we're working on
################################################################################

currentReturnCode = 0

################################################################################
### @brief collect json body parameter definitions:
################################################################################

restBodyParam = None
restReplyBodyParam = None
restSubBodyParam = None

################################################################################
### @brief C_FILE
################################################################################

C_FILE = False

################################################################################
### @brief DEBUG
################################################################################

DEBUG = True
DEBUG = False

################################################################################
### @brief facility to remove leading and trailing html-linebreaks
################################################################################
removeTrailingBR = re.compile("<br>$")
removeLeadingBR = re.compile("^<br>")

def brTrim(text):
    return removeLeadingBR.sub("", removeTrailingBR.sub("", text.strip(' ')))

################################################################################
### @brief check for token to be right
################################################################################

reqOpt = ["required", "optional"]
def CheckReqOpt(token):
    if token not in reqOpt:
        print >> sys.stderr, "This is supposed to be required or optional!"
        raise Exception("invalid value")

################################################################################
### @brief trim_text
################################################################################

def trim_text(txt):
    r = rc(r"""[ \t]+$""")
    txt = r.sub("", txt)

    return txt

################################################################################
### @brief parameters
###
### suche die erste {
### suche die letzten }
### gib alles dazwischen zurck
################################################################################

def parameters(line):
    (l, c, line) = line.partition('{')
    (line, c , r) = line.rpartition('}')
    line = BackTicks(line, wordboundary = ['{','}'])

    return line

################################################################################
### @brief BackTicks
###
### `word` -> <b>word</b>
################################################################################

def BackTicks(txt, wordboundary = ['<em>','</em>']):
    r = rc(r"""([\(\s'/">]|^|.)\`(.*?)\`([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief AsteriskItalic
###
### *word* -> <b>word</b>
################################################################################

def AsteriskItalic(txt, wordboundary = ['<strong>','</strong>']):
    r = rc(r"""([\(\s'/">]|^|.)\*(.*?)\*([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief AsteriskBold
###
### **word** -> <b>word</b>
################################################################################

def AsteriskBold(txt, wordboundary = ['<strong>','</strong>']):
    r = rc(r"""([\(\s'/">]|^|.)\*\*(.*?)\*\*([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief FA
###
### @FA{word} -> <b>word</b>
################################################################################

def FA(txt, wordboundary = ['<b>','</b>']):
    r = rc(r"""([\(\s'/">]|^|.)@FA\{(.*?)\}([<\s\.\),:;'"?!/-]|$)""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief FN
###
### @FN{word} -> <b>word</b>
################################################################################

def FN(txt, wordboundary = ['<b>','</b>']):
    r = rc(r"""([\(\s'/">]|^|.)@FN\{(.*?)\}([<\s\.\),:;'"?!/-])""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief LIT
###
### @LIT{word} -> <b>word</b>
################################################################################

def LIT(txt, wordboundary = ['<b>','</b>']):
    r = rc(r"""([\(\s'/">]|^)@LIT\{(.*?)\}([<\s\.\),:;'"?!/-])""", MS)
    subpattern = '\\1' + wordboundary[0] + '\\2' + wordboundary[1] + '\\3'

    return r.sub(subpattern, txt)

################################################################################
### @brief Typegraphy
################################################################################

def Typography(txt):
    if C_FILE:
        txt = txt[4:-1]
    else:
        txt = txt[0:-1]

#    txt = BackTicks(txt)
    txt = AsteriskBold(txt)
    txt = AsteriskItalic(txt)
#    txt = FN(txt)
    txt = LIT(txt)
#    txt = FA(txt)
#
    # no way to find out the correct link for Swagger, 
    # so replace all @ref elements with just "the manual"

    r = rc(r"""@ref [a-zA-Z0-9]+""", MS)
    txt = r.sub("the manual", txt)
    txt = re.sub(r"@endDocuBlock", "", txt)

    return txt

################################################################################
### @brief InitializationError
################################################################################

class InitializationError(Exception): pass

################################################################################
### @brief StateMachine
################################################################################

class StateMachine:
    def __init__(self):
        self.handlers = []
        self.startState = None
        self.endStates = []
        self.file = ''
        self.fn = ''

    def add_state(self, handler, end_state=0):
        self.handlers.append(handler)

        if end_state:
            self.endStates.append(handler)

    def set_fn(self, filename):
        self.fn = filename
        global fn
        fn = filename

    def set_start(self, handler):
        self.startState = handler

    def run(self, cargo=None):
        if not self.startState:
            raise InitializationError,\
                "must call .set_start() before .run()"

        if not self.endStates:
            raise InitializationError, \
                "at least one state must be an end_state"

        handler = self.startState
        try:
            while 1:
                (newState, cargo) = handler(cargo)

                if newState in self.endStates:
                    newState(cargo)
                    break
                elif newState not in self.handlers:
                    raise RuntimeError, "Invalid target %s" % newState
                else:
                    handler = newState
        except:
            print >> sys.stderr, "while parsing '" + self.fn + "'"
            print >> sys.stderr, "trying to use handler '"  + handler.__name__ + "'"
            raise

################################################################################
### @brief Regexen
################################################################################

class Regexen:
    def __init__(self):
        self.DESCRIPTION_LI = re.compile('^-\s.*$')
        self.DESCRIPTION_SP = re.compile('^\s\s.*$')
        self.DESCRIPTION_BL = re.compile('^\s*$')
        self.EMPTY_LINE = re.compile('^\s*$')
        self.START_DOCUBLOCK = re.compile('.*@startDocuBlock ')
        self.END_EXAMPLE_ARANGOSH_RUN = re.compile('.*@END_EXAMPLE_ARANGOSH_RUN')
        self.EXAMPLES = re.compile('.*@EXAMPLES')
        self.EXAMPLE_ARANGOSH_RUN = re.compile('.*@EXAMPLE_ARANGOSH_RUN{')
        self.FILE = re.compile('.*@file')
        self.RESTBODYPARAM = re.compile('.*@RESTBODYPARAM')
        self.RESTSTRUCT = re.compile('.*@RESTSTRUCT')
        self.RESTALLBODYPARAM = re.compile('.*@RESTALLBODYPARAM')
        self.RESTDESCRIPTION = re.compile('.*@RESTDESCRIPTION')
        self.RESTDONE = re.compile('.*@RESTDONE')
        self.RESTHEADER = re.compile('.*@RESTHEADER{')
        self.RESTHEADERPARAM = re.compile('.*@RESTHEADERPARAM{')
        self.RESTHEADERPARAMETERS = re.compile('.*@RESTHEADERPARAMETERS')
        self.RESTQUERYPARAM = re.compile('.*@RESTQUERYPARAM{')
        self.RESTQUERYPARAMETERS = re.compile('.*@RESTQUERYPARAMETERS')
        self.RESTREPLYBODY = re.compile('.*@RESTREPLYBODY')
        self.RESTRETURNCODE = re.compile('.*@RESTRETURNCODE{')
        self.RESTRETURNCODES = re.compile('.*@RESTRETURNCODES')
        self.RESTURLPARAM = re.compile('.*@RESTURLPARAM{')
        self.RESTURLPARAMETERS = re.compile('.*@RESTURLPARAMETERS')
        self.NON_COMMENT = re.compile('^[^/].*')

################################################################################
### @brief checks for end of comment
################################################################################

def check_end_of_comment(line, r):
    if C_FILE:
        return r.NON_COMMENT.match(line)
    else:
        return r.RESTDONE.match(line)

################################################################################
### @brief next_step
################################################################################

def next_step(fp, line, r):
    global operation

    if not line:                              return eof, (fp, line)
    elif check_end_of_comment(line, r):       return skip_code, (fp, line)
    elif r.START_DOCUBLOCK.match(line):       return start_docublock, (fp, line)
    elif r.EXAMPLE_ARANGOSH_RUN.match(line):  return example_arangosh_run, (fp, line)
    elif r.RESTBODYPARAM.match(line):         return restbodyparam, (fp, line)
    elif r.RESTSTRUCT.match(line):            return reststruct, (fp, line)
    elif r.RESTALLBODYPARAM.match(line):      return restallbodyparam, (fp, line)
    elif r.RESTDESCRIPTION.match(line):       return restdescription, (fp, line)
    elif r.RESTHEADER.match(line):            return restheader, (fp, line)
    elif r.RESTHEADERPARAM.match(line):       return restheaderparam, (fp, line)
    elif r.RESTHEADERPARAMETERS.match(line):  return restheaderparameters, (fp, line)
    elif r.RESTQUERYPARAM.match(line):        return restqueryparam, (fp, line)
    elif r.RESTQUERYPARAMETERS.match(line):   return restqueryparameters, (fp, line)
    elif r.RESTREPLYBODY.match(line):         return restreplybody, (fp, line)
    elif r.RESTRETURNCODE.match(line):        return restreturncode, (fp, line)
    elif r.RESTRETURNCODES.match(line):       return restreturncodes, (fp, line)
    elif r.RESTURLPARAM.match(line):          return resturlparam, (fp, line)
    elif r.RESTURLPARAMETERS.match(line):     return resturlparameters, (fp, line)
    elif r.EXAMPLES.match(line):              return examples, (fp, line)

    return None, None

################################################################################
### @brief generic handler
################################################################################

def generic_handler(cargo, r, message):
    global DEBUG

    if DEBUG: print >> sys.stderr, message
    (fp, last) = cargo

    while 1:
        (next, c) = next_step(fp, fp.readline(), r)

        if next:
            return next, c

################################################################################
### @brief generic handler with description
### @param cargo the file we're working on
### @param r the regex that matched
### @param message
### @param op
### @param para an object were we should write to
### @param name the key in para we should write to
################################################################################

def generic_handler_desc(cargo, r, message, op, para, name):
    global DEBUG, C_FILE, operation

    if DEBUG: print >> sys.stderr, message
    (fp, last) = cargo

    inLI = False
    inUL = False

    while 1:
        line = fp.readline()
        (next, c) = next_step(fp, line, r)

        if next:
            para[name] = trim_text(para[name])

            if op:
                try:
                    operation[op].append(para)
                except AttributeError as x:
                    print >> sys.stderr, "trying to set '%s' on operations - failed. '%s'" % (op, para)
                    raise
            return next, c

        if C_FILE and line[0:4] == "////":
            continue

        line = Typography(line)

        if r.DESCRIPTION_LI.match(line):
            line = "<li>" + line[2:]
            inLI = True
        elif inLI and r.DESCRIPTION_SP.match(line):
            line = line[2:]
        elif inLI and r.DESCRIPTION_BL.match(line):
            line = ""
        else:
            inLI = False

        if not inUL and inLI:
            line = " <ul class=\"swagger-list\">" + line
            inUL = True
        elif inUL and r.EMPTY_LINE.match(line):
            line = "</ul> " + line
            inUL = False

        elif inLI and r.EMPTY_LINE.match(line):
            line = "</li> " + line
            inUL = False

        if not inLI and r.EMPTY_LINE.match(line):
            line = "<br>"

        para[name] += line + ' '
    para[name] = removeTrailingBR.sub("", para[name])

def start_docublock(cargo, r=Regexen()):
    global currentDocuBlock
    (fp, last) = cargo
    try:
        currentDocuBlock = last.split(' ')[2].rstrip()
    except Exception as x:
        print >> sys.stderr, "failed to fetch docublock in '" + last + "'"
        raise

    return generic_handler(cargo, r, 'start_docublock')


def setRequired(where, which):
    if not 'required' in where:
        where['required'] = []
    where['required'].append(which)

################################################################################
### @brief restheader
################################################################################

def restheader(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restBodyParam, fn, currentExample, currentReturnCode, currentDocuBlock, lastDocuBlock, restReplyBodyParam

    currentReturnCode = 0
    currentExample = 0
    restReplyBodyParam = None
    restBodyParam = None

    (fp, last) = cargo

    temp = parameters(last).split(',')
    (ucmethod, path) = temp[0].split()

    #TODO: hier checken, ob der letzte alles hatte (responses)
    summary = temp[1]
    summaryList = summary.split()
    method = ucmethod.lower()
    nickname = summaryList[0] + ''.join([word.capitalize() for word in summaryList[1:]])

    httpPath = FA(path, wordboundary = ['{', '}'])
    if not httpPath in swagger['paths']:
        swagger['paths'][httpPath] = {}
    if method in swagger['paths'][httpPath]:
        print >> sys.stderr, "duplicate route detected:"
        print >> sys.stderr, "There already is a route [" + ucmethod + " " + httpPath + "]: "
        print >> sys.stderr, json.dumps(swagger['paths'][httpPath], indent=4, separators=(', ',': '), sort_keys=True)
        raise Exception("Duplicate route")

    if currentDocuBlock == None:
        raise Exception("No docublock started for this restheader: " + ucmethod + " " + path )

    if lastDocuBlock != None and currentDocuBlock == lastDocuBlock:
        raise Exception("No new docublock started for this restheader: " + ucmethod + " " + path  + ' : ' + currentDocuBlock)

    lastDocuBlock = currentDocuBlock


    swagger['paths'][httpPath][method] = {
        'x-filename': fn,
        'x-examples': [],
        'tags': [currentTag],
        'summary': summary,
        'description': '',
        'parameters' : [],
        }
    operation = swagger['paths'][httpPath][method]
    return generic_handler(cargo, r, "resturlparameters")

################################################################################
### @brief resturlparameters
################################################################################

def resturlparameters(cargo, r=Regexen()):
    return generic_handler(cargo, r, "resturlparameters")

################################################################################
### @brief resturlparam
################################################################################

def resturlparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method
    (fp, last) = cargo

    parametersList = parameters(last).split(',')

    if parametersList[2] != 'required':
        print >> sys.stderr, "only required is supported in RESTURLPARAM"
        raise

    para = {
        'name': parametersList[0],
        'in': 'path',
        'format': parametersList[1],
        'description': '',
        'type': parametersList[1].lower(),
        'required': True
        }
    swagger['paths'][httpPath][method]['parameters'].append(para) 
    
    return generic_handler_desc(cargo, r, "resturlparam", None, para, 'description')

################################################################################
### @brief restqueryparameters
################################################################################

def restqueryparameters(cargo, r=Regexen()):
    return generic_handler(cargo, r, "restqueryparameters")

################################################################################
### @brief restheaderparameters
################################################################################

def restheaderparameters(cargo, r=Regexen()):
    return generic_handler(cargo, r, "restheaderparameters")

################################################################################
### @brief restheaderparameters
################################################################################

def restheaderparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method
    (fp, last) = cargo

    parametersList = parameters(last).split(',')

    para = {
        'in': 'header',
        'type': parametersList[1].lower(),
        'name': parametersList[0],
        'description': ''
        }
    swagger['paths'][httpPath][method]['parameters'].append(para) 

    return generic_handler_desc(cargo, r, "restheaderparam", None, para, 'description')

################################################################################
### @brief restbodyparam
################################################################################

def restbodyparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restBodyParam, fn, currentDocuBlock
    (fp, last) = cargo

    try:
        (name, ptype, required, ptype2) = parameters(last).split(',')
    except Exception as x:
        print >> sys.stderr, "RESTBODYPARAM: 4 arguments required. You gave me: " + parameters(last)
        print >> sys.stderr, "In this docublock: " + currentDocuBlock
        raise

    CheckReqOpt(required)
    if required == 'required':
        required = True
    else:
        required = False

    if restBodyParam == None:
        # https://github.com/swagger-api/swagger-ui/issues/1430
        # once this is solved we can skip this:
        operation['description'] += "**A json post document with these Properties is required:**"
        restBodyParam = {
            'name': 'Json Post Body',
            'x-description-offset': len(swagger['paths'][httpPath][method]['description']),
            'in': 'body',
            'required': True,
            'schema': {
                '$ref': '#/definitions/' + currentDocuBlock
                }
            }
        swagger['paths'][httpPath][method]['parameters'].append(restBodyParam) 

    if not currentDocuBlock in swagger['definitions']:
        swagger['definitions'][currentDocuBlock] = {
            'x-filename': fn,
            'type' : 'object',
            'properties': {},
            }

    swagger['definitions'][currentDocuBlock]['properties'][name] = {
        'type': ptype,
        'description': ''
        }

    if ptype == 'object' and len(ptype2) > 0:
        swagger['definitions'][currentDocuBlock]['properties'][name] = {
            '$ref': '#/definitions/' + ptype2
            }

        if not ptype2 in swagger['definitions']:
            swagger['definitions'][ptype2] = {
                'x-filename': fn,
                'type': 'object',
                'properties' : {},
                'description': ''
                }

        if required:
            setRequired(swagger['definitions'][ptype2], name)
        
        return generic_handler_desc(cargo, r, "restbodyparam", None,
                                    swagger['definitions'][ptype2],
                                    'description')

    if ptype == 'array':
        if ptype2 not in swaggerBaseTypes:
            swagger['definitions'][currentDocuBlock]['properties'][name]['items'] = {
                '$ref': '#/definitions/' + ptype2
            }
        else:
            swagger['definitions'][currentDocuBlock]['properties'][name]['items'] = {
                'type': ptype2
            }
            if ptype2 == 'object':
                swagger['definitions'][currentDocuBlock]['properties'][name]['items']['additionalProperties'] = {}
    elif ptype == 'object':
            swagger['definitions'][currentDocuBlock]['properties'][name]['additionalProperties'] = {}
    elif ptype != 'string':
        swagger['definitions'][currentDocuBlock]['properties'][name]['format'] = ptype2


    if required:
        setRequired(swagger['definitions'][currentDocuBlock], name)

    return generic_handler_desc(cargo, r, "restbodyparam", None,
                                swagger['definitions'][currentDocuBlock]['properties'][name],
                                'description')

################################################################################
### @brief restallbodyparam
################################################################################

def restallbodyparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restBodyParam
    (fp, last) = cargo

    try:
        (name, ptype, required) = parameters(last).split(',')
    except Exception as x:
        print >> sys.stderr, "RESTALLBODYPARAM: 3 arguments required. You gave me: " + parameters(last)

    CheckReqOpt(required)
    if required == 'required':
        required = True
    else:
        required = False
    if restBodyParam != None:
        raise Exception("May only have one 'ALLBODY'")

    restBodyParam = {
            'name': 'Json Post Body',
            'description': '',
            'in': 'body',
            'x-description-offset': len(swagger['paths'][httpPath][method]['description']),
            'required': required,
            'schema': {
                'type': 'object',
                'additionalProperties': {}
                }
            }
    swagger['paths'][httpPath][method]['parameters'].append(restBodyParam) 

    return generic_handler_desc(cargo, r, "restbodyparam", None,
                                restBodyParam,
                                'description')

################################################################################
### @brief reststruct
################################################################################

def reststruct(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restBodyParam, restSubBodyParam
    (fp, last) = cargo

    try:
        (name, className, ptype, required, ptype2) = parameters(last).split(',')
    except Exception as x:
        print >> sys.stderr, "RESTSTRUCT: 4 arguments required. You gave me: " + parameters(last)
        raise

    CheckReqOpt(required)
    if required == 'required':
        required = True
    else:
        required = False

    if className not in swagger['definitions']:
        swagger['definitions'][className] = {
            'type': 'object',
            'properties' : {},
            'description': ''
            }

    swagger['definitions'][className]['properties'][name] = {
        'type': ptype,
        'description': ''
        }

    if ptype == 'array':
        if ptype2 not in swaggerBaseTypes:
            swagger['definitions'][className]['properties'][name]['items'] = {
                '$ref': '#/definitions/' + ptype2
            }
        else:
            swagger['definitions'][className]['properties'][name]['items'] = {
                'type': ptype2
            }
    if ptype == 'object' and len(ptype2) > 0:
        if not ptype2 in swagger['definitions']:
            swagger['definitions'][ptype2] = {
                'x-filename': fn,
                'type': 'object',
                'properties' : {},
                'description': ''
                }
        swagger['definitions'][className]['properties'][name] = {
            '$ref': '#/definitions/' + ptype2
            }

        if required:
            setRequired(swagger['definitions'][className], name)
       
        return generic_handler_desc(cargo, r, "reststruct", None,
                                    swagger['definitions'][ptype2],
                                    'description')

    elif ptype != 'string' and ptype != 'boolean':
        swagger['definitions'][className]['properties'][name]['format'] = ptype2

    return generic_handler_desc(cargo, r, "restbodyparam", None,
                                swagger['definitions'][className]['properties'][name],
                                'description')

################################################################################
### @brief restqueryparam
################################################################################

def restqueryparam(cargo, r=Regexen()):
    global swagger, operation, httpPath, method
    (fp, last) = cargo

    parametersList = parameters(last).split(',')

    CheckReqOpt(parametersList[2])
    if parametersList[2] == 'required':
        required = True
    else:
        required = False

    para = {
        'name': parametersList[0],
        'in': 'query',
        'description': '',
        'type': parametersList[1].lower(),
        'required': required
        }

    swagger['paths'][httpPath][method]['parameters'].append(para) 
    return generic_handler_desc(cargo, r, "restqueryparam", None, para, 'description')

################################################################################
### @brief restdescription
################################################################################

def restdescription(cargo, r=Regexen()):
    global swagger, operation, httpPath, method
    swagger['paths'][httpPath][method]['description'] += '\n\n'
    return generic_handler_desc(cargo, r, "restdescription", None,
                                swagger['paths'][httpPath][method],
                                'description')

################################################################################
### @brief restreplybody
################################################################################

def restreplybody(cargo, r=Regexen()):
    global swagger, operation, httpPath, method, restReplyBodyParam, fn
    (fp, last) = cargo

    try:
        (name, ptype, required, ptype2) = parameters(last).split(',')
    except Exception as x:
        print >> sys.stderr, "RESTREPLYBODY: 4 arguments required. You gave me: " + parameters(last)

    CheckReqOpt(required)
    if required == 'required':
        required = True
    else:
        required = False

    rcBlock = currentDocuBlock + '_rc_' +  currentReturnCode
    #if currentReturnCode:
    if restReplyBodyParam == None:
        # https://github.com/swagger-api/swagger-ui/issues/1430
        # once this is solved we can skip this:
        operation['description'] += "**A json document with these Properties is returned:**"
        swagger['paths'][httpPath][method]['responses'][currentReturnCode][
            'x-description-offset'] = len(swagger['paths'][httpPath][method]['description'])
        swagger['paths'][httpPath][method]['responses'][currentReturnCode]['schema'] = {
                '$ref': '#/definitions/' + rcBlock
                }
        swagger['paths'][httpPath][method] ['produces'] = [
            "application/json"
            ]
        restReplyBodyParam = ''

    if not rcBlock in swagger['definitions']:
        swagger['definitions'][rcBlock] = {
            'x-filename': fn,
            'type' : 'object',
            'properties': {},
            }

    swagger['definitions'][rcBlock]['properties'][name] = {
        'type': ptype,
        'description': ''
        }

    if ptype == 'object' and len(ptype2) > 0:
        swagger['definitions'][rcBlock]['properties'][name] = {
            '$ref': '#/definitions/' + ptype2
            }

        if not ptype2 in swagger['definitions']:
            swagger['definitions'][ptype2] = {
                'x-filename': fn,
                'type': 'object',
                'properties' : {},
                'description': ''
                }

        if required:
            setRequired(swagger['definitions'][ptype2], name)
       
        return generic_handler_desc(cargo, r, "restbodyparam", None,
                                    swagger['definitions'][ptype2],
                                    'description')

    if ptype == 'array':
        if len(ptype2) == 0:
            swagger['definitions'][rcBlock]['properties'][name]['items'] = {
            }
        elif ptype2 not in swaggerBaseTypes:
            swagger['definitions'][rcBlock]['properties'][name]['items'] = {
                '$ref': '#/definitions/' + ptype2
            }
        else:
            swagger['definitions'][rcBlock]['properties'][name]['items'] = {
                'type': ptype2
            }
            if ptype2 == 'object':
                swagger['definitions'][rcBlock]['properties']\
                       [name]['items']['additionalProperties']      = {}
    elif ptype == 'object':
            swagger['definitions'][rcBlock]['properties'][name]['additionalProperties'] = {}
    elif ptype != 'string':
        swagger['definitions'][rcBlock]['properties'][name]['format'] = ptype2


    if required:
        setRequired(swagger['definitions'][rcBlock], name)

    return generic_handler_desc(cargo, r, "restreplybody", None,
                                swagger['definitions'][rcBlock]['properties'][name],
                                'description')

################################################################################
### @brief restreturncodes
################################################################################

def restreturncodes(cargo, r=Regexen()):
    return generic_handler(cargo, r, "restreturncodes")

################################################################################
### @brief restreturncode
################################################################################

def restreturncode(cargo, r=Regexen()):
    global currentReturnCode, restReplyBodyParam
    (fp, last) = cargo
    restReplyBodyParam = None
    currentReturnCode = parameters(last)

    if not 'responses' in swagger['paths'][httpPath][method]:
        swagger['paths'][httpPath][method]['responses'] = {}
    swagger['paths'][httpPath][method]['responses'][currentReturnCode] =  {
        #'code': parameters(last),
        'description': ''
        }
    return generic_handler_desc(cargo, r, "restreturncode", None,
                                swagger['paths'][httpPath][method]['responses'][parameters(last)],
                                'description')

################################################################################
### @brief examples
################################################################################

def examples(cargo, r=Regexen()):
    global currentExample
    operation['x-examples'].append('')
    return generic_handler_desc(cargo, r, "x-examples", None, operation['x-examples'], currentExample)

################################################################################
### @brief example_arangosh_run
################################################################################


def example_arangosh_run(cargo, r=Regexen()):
    global currentExample, DEBUG, C_FILE

    if DEBUG: print >> sys.stderr, "example_arangosh_run"
    fp, last = cargo

    exampleHeader = brTrim(operation['x-examples'][currentExample]).strip()

    # new examples code TODO should include for each example own object in json file
    examplefile = open(os.path.join(os.path.dirname(__file__), '../Examples/' + parameters(last) + '.generated'))
    operation['x-examples'][currentExample]= '<details><summary>Example: ' + exampleHeader.strip('\n ') + '</summary><br><br><pre><code class="json">'

    for line in examplefile.readlines():
        operation['x-examples'][currentExample] += line

    operation['x-examples'][currentExample] += '</code></pre><br></details>\n'

    line = ""

    while not r.END_EXAMPLE_ARANGOSH_RUN.match(line):
        line = fp.readline()

        if not line:
            return eof, (fp, line)

    currentExample += 1

    return examples, (fp, line)

################################################################################
### @brief eof
################################################################################

def eof(cargo):
    global DEBUG, C_FILE
    if DEBUG: print >> sys.stderr, "eof"

################################################################################
### @brief error
################################################################################

def error(cargo):
    global DEBUG, C_FILE
    if DEBUG: print >> sys.stderr, "error"

    sys.stderr.write('Unidentifiable line:\n' + cargo)

################################################################################
### @brief comment
################################################################################

def comment(cargo, r=Regexen()):
    global DEBUG, C_FILE

    if DEBUG: print >> sys.stderr, "comment"
    (fp, last) = cargo

    while 1:
        line = fp.readline()
        if not line: return eof, (fp, line)

        if r.FILE.match(line): C_FILE = True

        next, c = next_step(fp, line, r)

        if next:
            return next, c

################################################################################
### @brief skip_code
###
### skip all non comment lines
################################################################################

def skip_code(cargo, r=Regexen()):
    global DEBUG, C_FILE

    if DEBUG: print >> sys.stderr, "skip_code"
    (fp, last) = cargo
    
    if not C_FILE:
        return comment((fp, last), r)

    while 1:
        line = fp.readline()

        if not line:
            return eof, (fp, line)

        if not r.NON_COMMENT.match(line):
            return comment((fp, line), r)

################################################################################
### @brief main
################################################################################

automat = StateMachine()

automat.add_state(comment)
automat.add_state(eof, end_state=1)
automat.add_state(error, end_state=1)
automat.add_state(start_docublock)
automat.add_state(example_arangosh_run)
automat.add_state(examples)
automat.add_state(skip_code)
automat.add_state(restbodyparam)
automat.add_state(reststruct)
automat.add_state(restallbodyparam)
automat.add_state(restdescription)
automat.add_state(restheader)
automat.add_state(restheaderparam)
automat.add_state(restheaderparameters)
automat.add_state(restqueryparam)
automat.add_state(restqueryparameters)
automat.add_state(restreturncode)
automat.add_state(restreturncodes)
automat.add_state(restreplybody)
automat.add_state(resturlparam)
automat.add_state(resturlparameters)



def getOneApi(infile, filename):
    automat.set_start(skip_code)
    automat.set_fn(filename)
    automat.run((infile, ''))

def getReference(name, source, verb):
    try:
        ref = name['$ref'][defLen:]
    except Exception as x:
        print >>sys.stderr, "No reference in: "
        print >>sys.stderr, name
        raise
    if not ref in swagger['definitions']:
        fn = ''
        if verb:
            fn = swagger['paths'][route][verb]['x-filename']
        else:
            fn = swagger['definitions'][source]['x-filename']
        print >> sys.stderr, json.dumps(swagger['definitions'], indent=4, separators=(', ',': '), sort_keys=True)
        raise Exception("invalid reference: " + ref + " in " + fn)
    return ref

def unwrapPostJson(reference, layer):
    global swagger
    rc = ''
    for param in swagger['definitions'][reference]['properties'].keys():
        thisParam = swagger['definitions'][reference]['properties'][param]
        required = ('required' in swagger['definitions'][reference] and
                    param in swagger['definitions'][reference]['required'])

        if '$ref' in thisParam:
            subStructRef = getReference(thisParam, reference, None)

            rc += "<li><strong>" + param + "</strong>: "
            rc += swagger['definitions'][subStructRef]['description'] + "<ul class=\"swagger-list\">"
            rc += unwrapPostJson(subStructRef, layer + 1)
            rc += "</li></ul>"
        
        elif thisParam['type'] == 'object':
            rc += ' ' * layer + "<li><strong>" + param + "</strong>: " + brTrim(thisParam['description']) + "</li>"
        elif swagger['definitions'][reference]['properties'][param]['type'] == 'array':
            rc += ' ' * layer + "<li><strong>" + param + "</strong>: " + brTrim(thisParam['description'])
            if 'type' in thisParam['items']:
                rc += " of type " + thisParam['items']['type']#
            else:
                if len(thisParam['items']) == 0:
                    rc += "anonymous json object"
                else:
                    try:
                        subStructRef = getReference(thisParam['items'], reference, None)
                    except:
                        print >>sys.stderr, "while analyzing: " + param
                        print >>sys.stderr, thisParam
                    rc += "\n<ul class=\"swagger-list\">"
                    rc += unwrapPostJson(subStructRef, layer + 1)
                    rc += "</ul>"
                    rc += '</li>'
        else:
            rc += ' ' * layer + "<li><strong>" + param + "</strong>: " + thisParam['description'] + '</li>'
    return rc


files = { 
  "Administration" : ["js/actions/_admin/app.js",
                      "js/actions/_admin/routing/app.js",
                      "js/actions/_admin/server/app.js",
                      "js/actions/_admin/database/app.js",
                      "arangod/RestHandler/RestShutdownHandler.cpp",
                      "arangod/RestHandler/RestAdminLogHandler.cpp",
                      "js/actions/api-tasks.js",
                      "js/actions/api-endpoint.js",
                      "arangod/RestHandler/RestVersionHandler.cpp",
                      "js/actions/api-system.js" ],# TODO: no docu here.
  "AQL" : [ "arangod/RestHandler/RestQueryHandler.cpp",
            "js/actions/api-aqlfunction.js",
            "js/actions/api-explain.js",
            "arangod/RestHandler/RestQueryCacheHandler.cpp"],
  "Bulk" : [ "arangod/RestHandler/RestExportHandler.cpp",
             "arangod/RestHandler/RestImportHandler.cpp",
             "arangod/RestHandler/RestBatchHandler.cpp" ],
  "Collections" : [ "js/actions/_api/collection/app.js" ],
  "Cursors" : [ "arangod/RestHandler/RestCursorHandler.cpp" ],
  "Database" : [ "js/actions/api-database.js" ],
  "Cluster" : ["js/actions/api-cluster.js"],
  "Documents" : [ "arangod/RestHandler/RestDocumentHandler.cpp" ],
  "Graph" : ["js/apps/system/_api/gharial/APP/gharial.js", "js/actions/api-graph.js" ],
  "Graph edges" : [ "arangod/RestHandler/RestEdgeHandler.cpp", "js/actions/api-edges.js" ],
  "Graph Traversal" : [ "js/actions/api-traversal.js" ],
  "Indexes" : [ "js/actions/api-index.js" ],
  "job" : [ "arangod/HttpServer/AsyncJobManager.cpp",
            "arangod/RestHandler/RestJobHandler.cpp"],
  "Replication" : [ "arangod/RestHandler/RestReplicationHandler.cpp" ],
  "Simple Queries" : [ "js/actions/api-simple.js",
                       "arangod/RestHandler/RestSimpleHandler.cpp",
                       "arangod/RestHandler/RestSimpleQueryHandler.cpp" ],
  "Transactions" : [ "js/actions/api-transaction.js" ],
  "User handling" : [ "js/actions/_api/user/app.js" ],
  "wal" : [ "js/actions/_admin/wal/app.js" ]
}

# Intentionaly not there: 
#  "structure" : [ "js/actions/api-structure.js" ],


if len(sys.argv) < 3:
  print >> sys.stderr, "usage: " + sys.argv[0] + " <scriptDir> <outDir> <relDir>"
  sys.exit(1)

scriptDir = sys.argv[1]
if not scriptDir.endswith("/"):
  scriptDir += "/"

outDir = sys.argv[2]
if not outDir.endswith("/"):
  outDir += "/"

relDir = sys.argv[3]
if not relDir.endswith("/"):
  relDir += "/"

# read ArangoDB version
f = open(scriptDir + "VERSION", "r")
for version in f:
  version = version.strip('\n')
f.close()


paths = {};


for name, filenames in sorted(files.items(), key=operator.itemgetter(0)):
    currentTag = name
    for fn in filenames:
        infile = open(fn)
        getOneApi(infile, name + " - " + ', '.join(filenames))
        infile.close()
        currentDocuBlock = None
        lastDocuBlock = None

for route in swagger['paths'].keys():
    for verb in swagger['paths'][route].keys():
        offsetPlus = 0;
        thisVerb = swagger['paths'][route][verb]
        if len(thisVerb['description']) == 0:
            print >> sys.stderr, "Description of Route empty; @RESTDESCRIPTION missing?"
            print >> sys.stderr, "in :" + verb + " " + route
            #raise TODO
        # insert the post json description into the place we extracted it:
        for nParam in range(0, len(thisVerb['parameters'])):
            if thisVerb['parameters'][nParam]['in'] == 'body':
                descOffset = thisVerb['parameters'][nParam]['x-description-offset']
                addText = ''
                postText = thisVerb['description'][:descOffset]
                if 'additionalProperties' in thisVerb['parameters'][nParam]['schema']:
                    addText = "free style json body"
                else:
                    addText = "<ul class=\"swagger-list\">" + unwrapPostJson(
                        getReference(thisVerb['parameters'][nParam]['schema'], route, verb),0) + "</ul>"
                
                postText += addText
                postText += thisVerb['description'][descOffset:]
                offsetPlus += len(addText)
                thisVerb['description'] = postText

        # insert the reply json description into the place we extracted it:
        if 'responses' in thisVerb:
            for nRC in thisVerb['responses']:
                if 'x-description-offset' in thisVerb['responses'][nRC]:

                    descOffset = thisVerb['responses'][nRC]['x-description-offset']
                    #print descOffset 
                    #print offsetPlus
                    descOffset += offsetPlus
                    addText = ''
                    #print thisVerb['responses'][nRC]['description']
                    postText = thisVerb['description'][:descOffset]
                    #print postText
                    if 'additionalProperties' in thisVerb['responses'][nRC]['schema']:
                        addText = "free style json body"
                    else:
                        addText = "<ul class=\"swagger-list\">" + unwrapPostJson(
                            getReference(thisVerb['responses'][nRC]['schema'], route, verb),0) + "</ul>"
                    #print addText
                    postText += addText
                    postText += thisVerb['responses'][nRC]['description'][descOffset:]
                    offsetPlus += len(addText)
                    thisVerb['description'] = postText


        # Append the examples to the description:
        if 'x-examples' in thisVerb and len(thisVerb['x-examples']) > 0:
            thisVerb['description'] += '<br>'
            for nExample in range(0, len(thisVerb['x-examples'])):
                thisVerb['description'] +=  thisVerb['x-examples'][nExample]
            thisVerb['x-examples'] = []# todo unset!

#print highlight(yaml.dump(swagger, Dumper=yaml.RoundTripDumper), YamlLexer(), TerminalFormatter())
#print yaml.dump(swagger, Dumper=yaml.RoundTripDumper)
print json.dumps(swagger, indent=4, separators=(', ',': '), sort_keys=True)
#print json.dumps(swagger['paths'], indent=4, separators=(', ',': '), sort_keys=True)
#print highlight(yaml.dump(swagger, Dumper=yaml.RoundTripDumper), YamlLexer(), TerminalFormatter())

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4

