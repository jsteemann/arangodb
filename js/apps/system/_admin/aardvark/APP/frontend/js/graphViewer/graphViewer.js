/*global _, $*/
/*global ArangoAdapter, JSONAdapter, FoxxAdapter, PreviewAdapter, GharialAdapter*/
/*global ForceLayouter, EdgeShaper, NodeShaper, ZoomManager */
////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

function GraphViewer(svg, width, height, adapterConfig, config) {
  "use strict";

  // Make the html aware of xmlns:xlink
  $("html").attr("xmlns:xlink", "http://www.w3.org/1999/xlink");

  // Check if all required inputs are given
  if (svg === undefined || svg.append === undefined) {
    throw "SVG has to be given and has to be selected using d3.select";
  }

  if (width === undefined || width <= 0) {
    throw "A width greater 0 has to be given";
  }

  if (height === undefined || height <= 0) {
    throw "A height greater 0 has to be given";
  }

  if (adapterConfig === undefined || adapterConfig.type === undefined) {
    throw "An adapter configuration has to be given";
  }

  // Globally disable the right-click menu
  /*
  svg[0][0].oncontextmenu = function() {
    return false;
  };
  */
  var self = this,
    adapter,
    nodeShaper,
    edgeShaper,
    layouter,
    zoomManager,
    graphContainer,
    nodeContainer,
    edgeContainer,
    fixedSize,
    edges = [],
    nodes = [],

  parseLayouterConfig = function (config) {
    if (!config) {
      // Default
      config = {};
      config.nodes = nodes;
      config.links = edges;
      config.width = width;
      config.height = height;
      layouter = new ForceLayouter(config);
      return;
    }
    switch (config.type.toLowerCase()) {
      case "force":
        config.nodes = nodes;
        config.links = edges;
        config.width = width;
        config.height = height;
        layouter = new ForceLayouter(config);
        break;
      default:
        throw "Sorry unknown layout type.";
    }
  },

  nodeLimitCallBack = function(limit) {
    adapter.setNodeLimit(limit, self.start);
  },

  parseZoomConfig = function(config) {
    if (config) {
      zoomManager = new ZoomManager(width, height, svg,
        graphContainer, nodeShaper, edgeShaper,
        {}, nodeLimitCallBack);
    }
  },

  parseConfig = function(config) {
    var esConf = config.edgeShaper || {},
      nsConf = config.nodeShaper || {},
      idFunc = nsConf.idfunc || undefined,
      zConf = config.zoom || false;
    esConf.shape = esConf.shape || {
      type: EdgeShaper.shapes.ARROW
    };
    parseLayouterConfig(config.layouter);
    edgeContainer = graphContainer.append("g");
    edgeShaper = new EdgeShaper(edgeContainer, esConf);
    nodeContainer = graphContainer.append("g");
    nodeShaper = new NodeShaper(nodeContainer, nsConf, idFunc);
    layouter.setCombinedUpdateFunction(nodeShaper, edgeShaper);
    parseZoomConfig(zConf);
  };

  switch (adapterConfig.type.toLowerCase()) {
    case "arango":
      adapterConfig.width = width;
      adapterConfig.height = height;
      adapter = new ArangoAdapter(
        nodes,
        edges,
        this,
        adapterConfig
      );
      adapter.setChildLimit(10);
      break;
    case "gharial":
      adapterConfig.width = width;
      adapterConfig.height = height;
      adapter = new GharialAdapter(
        nodes,
        edges,
        this,
        adapterConfig
      );
      adapter.setChildLimit(10);
      break;
    case "foxx":
      adapterConfig.width = width;
      adapterConfig.height = height;
      adapter = new FoxxAdapter(
        nodes,
        edges,
        adapterConfig.route,
        this,
        adapterConfig
      );
      break;
    case "json":
      adapter = new JSONAdapter(
        adapterConfig.path,
        nodes,
        edges,
        this,
        width,
        height
      );
      break;
    case "preview":
      adapterConfig.width = width;
      adapterConfig.height = height;
      adapter = new PreviewAdapter(
        nodes,
        edges,
        this,
        adapterConfig
      );
      break;
    default:
      throw "Sorry unknown adapter type.";
  }

  graphContainer = svg.append("g");

  parseConfig(config || {});

  this.start = function(expand) {
    layouter.stop();

    if (expand) {
      if ($('.infoField').text() !== '') {
        _.each(nodes, function(node) {
          _.each(adapter.randomNodes, function(compare) {
            if (node._id === compare._id) {
              node._expanded = true;
            }
          });
        });
      }
      else {
        _.each(nodes, function(node) {
          node._expanded = true;
        });
      }
    }

    //expand all wanted nodes
    nodeShaper.drawNodes(nodes);
    edgeShaper.drawEdges(edges);
    layouter.start();
  };

  this.loadGraph = function(nodeId, callback) {
//    loadNode
//  loadInitialNode
    adapter.loadInitialNode(nodeId, function (node) {
      if (node.errorCode) {
        callback(node);
        return;
      }
      node._expanded = true;
      self.start();
      if (_.isFunction(callback)) {
        callback();
      }
    });
  };

  this.loadGraphWithRandomStart = function(callback) {
    adapter.loadRandomNode(function (node) {
      if (node.errorCode && node.errorCode === 404) {
        callback(node);
        return;
      }
      node._expanded = true;
      self.start(true);
      if (_.isFunction(callback)) {
        callback();
      }
    });
  };

  this.loadGraphWithAdditionalNode = function(attribute, value, callback) {
    adapter.loadAdditionalNodeByAttributeValue(attribute, value, function (node) {
      if (node.errorCode) {
        callback(node);
        return;
      }
      node._expanded = true;
      self.start();
      if (_.isFunction(callback)) {
        callback();
      }
    });
  };

  this.loadGraphWithAttributeValue = function(attribute, value, callback) {

    //clear random and defined nodes
    adapter.randomNodes = [];
    adapter.definedNodes = [];

    adapter.loadInitialNodeByAttributeValue(attribute, value, function (node) {
      if (node.errorCode) {
        callback(node);
        return;
      }
      node._expanded = true;

      self.start();
      if (_.isFunction(callback)) {
        callback();
      }
    });
  };

  this.cleanUp = function() {
    nodeShaper.resetColourMap();
    edgeShaper.resetColourMap();
  };

  this.changeWidth = function(w) {
    layouter.changeWidth(w);
    zoomManager.changeWidth(w);
    adapter.setWidth(w);
  };

  this.dispatcherConfig = {
    expand: {
      edges: edges,
      nodes: nodes,
      startCallback: self.start,
      adapter: adapter,
      reshapeNodes: nodeShaper.reshapeNodes
    },
    drag: {
      layouter: layouter
    },
    nodeEditor: {
      nodes: nodes,
      adapter: adapter
    },
    edgeEditor: {
      edges: edges,
      adapter: adapter
    }
  };
  this.adapter = adapter;
  this.nodeShaper = nodeShaper;
  this.edgeShaper = edgeShaper;
  this.layouter = layouter;
  this.zoomManager = zoomManager;
}
