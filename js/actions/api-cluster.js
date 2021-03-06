/*jshint strict: false, unused: false */
/*global AQL_EXECUTE, SYS_CLUSTER_TEST, UPGRADE_ARGS: true,
  ArangoServerState, ArangoClusterComm, ArangoClusterInfo, ArangoAgency */

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("@arangodb/actions");
var cluster = require("@arangodb/cluster");
var internal = require("internal");


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_cluster_test_GET
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_cluster_test_POST
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_cluster_test_PUT
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_cluster_test_DELETE
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_cluster_test_PATCH
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_cluster_test_HEAD
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster-test",
  prefix: true,

  callback : function (req, res) {
    var path;
    if (req.hasOwnProperty('suffix') && req.suffix.length !== 0) {
      path = "/"+req.suffix.join("/");
    }
    else {
      path = "/_admin/version";
    }
    var params = "";
    var shard = "";
    var p;

    for (p in req.parameters) {
      if (req.parameters.hasOwnProperty(p)) {
        if (params === "") {
          params = "?";
        }
        else {
          params += "&";
        }
        params += p+"="+ encodeURIComponent(String(req.parameters[p]));
      }
    }
    if (params !== "") {
      path += params;
    }
    var headers = {};
    var transID = "";
    var timeout = 24*3600.0;
    var asyncMode = true;

    for (p in req.headers) {
      if (req.headers.hasOwnProperty(p)) {
        if (p === "x-client-transaction-id") {
          transID = req.headers[p];
        }
        else if (p === "x-timeout") {
          timeout = parseFloat(req.headers[p]);
          if (isNaN(timeout)) {
            timeout = 24*3600.0;
          }
        }
        else if (p === "x-synchronous-mode") {
          asyncMode = false;
        }
        else if (p === "x-shard-id") {
          shard = req.headers[p];
        }
        else {
          headers[p] = req.headers[p];
        }
      }
    }

    var body;
    if (req.requestBody === undefined || typeof req.requestBody !== "string") {
      body = "";
    }
    else {
      body = req.requestBody;
    }

    var r;
    if (typeof SYS_CLUSTER_TEST === "undefined") {
      actions.resultError(req, res, actions.HTTP_NOT_FOUND,
                          "Not compiled for cluster operation");
    }
    else {
      try {
        r = SYS_CLUSTER_TEST(req, res, shard, path, transID,
                              headers, body, timeout, asyncMode);
        if (r.timeout || typeof r.errorMessage === 'string') {
          res.responseCode = actions.HTTP_OK;
          res.contentType = "application/json; charset=utf-8";
          var s = JSON.stringify(r);
          res.body = s;
        }
        else {
          res.responseCode = actions.HTTP_OK;
          res.contentType = r.headers.contentType;
          res.headers = r.headers;
          res.body = r.body;
        }
      }
      catch(err) {
        actions.resultError(req, res, actions.HTTP_FORBIDDEN, String(err));
      }
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief function to parse an authorization header
////////////////////////////////////////////////////////////////////////////////

function parseAuthorization (authorization) {
  var auth = require("internal").base64Decode(authorization.substr(6));
  var pos = auth.indexOf(":");
  if (pos === -1) {
    return {username:"root", passwd:""};
  }
  return { username: auth.substr(0, pos),
           passwd: auth.substr(pos+1) || "" };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_cluster_planner_POST
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/clusterPlanner",
  prefix: false,

  callback : function (req, res) {
    if (ArangoServerState.disableDispatcherKickstarter() === true) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    var userconfig;
    try {
      userconfig = JSON.parse(req.requestBody);
    }
    catch (err) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "Posted body was not valid JSON.");
      return;
    }
    var Planner = require("@arangodb/cluster/planner").Planner;
    try {
      var p = new Planner(userconfig);
      res.responseCode = actions.HTTP_OK;
      res.contentType = "application/json; charset=utf-8";
      res.body = JSON.stringify({"clusterPlan": p.getPlan(),
                                 "config": p.config,
                                 "error": false});
    }
    catch (error) {
      actions.resultException(req, res, error, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_cluster_dispatcher_POST
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/clusterDispatch",
  prefix: false,

  callback : function (req, res) {
    if (ArangoServerState.disableDispatcherKickstarter() === true) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    var input;
    try {
      input = JSON.parse(req.requestBody);
    }
    catch (error) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "Posted body was not valid JSON.");
      return;
    }
    if (!input.hasOwnProperty("clusterPlan")) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          'Posted body needs a "clusterPlan" property.');
      return;
    }
    if (!input.hasOwnProperty("myname")) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          'Posted body needs a "myname" property.');
      return;
    }
    var action = input.action;
    var Kickstarter, k, r;
    if (action === "launch") {
      Kickstarter = require("@arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        r = k.launch();
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error2) {
        actions.resultException(req, res, error2, undefined, false);
      }
    }
    else if (action === "relaunch") {
      Kickstarter = require("@arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        r = k.relaunch();
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error3) {
        actions.resultException(req, res, error3, undefined, false);
      }
    }
    else if (action === "upgrade") {
      Kickstarter = require("@arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        r = k.upgrade(input.username, input.password);
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error3a) {
        actions.resultException(req, res, error3a, undefined, false);
      }
    }
    else if (action === "shutdown") {
      if (!input.hasOwnProperty("runInfo")) {
        actions.resultError(req, res, actions.HTTP_BAD,
                            'Posted body needs a "runInfo" property.');
        return;
      }
      Kickstarter = require("@arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        k.runInfo = input.runInfo;
        r = k.shutdown();
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error4) {
        actions.resultException(req, res, error4, undefined, false);
      }
    }
    else if (action === "cleanup") {
      Kickstarter = require("@arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        r = k.cleanup();
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error5) {
        actions.resultException(req, res, error5, undefined, false);
      }
    }
    else if (action === "isHealthy") {
      if (!input.hasOwnProperty("runInfo")) {
        actions.resultError(req, res, actions.HTTP_BAD,
                            'Posted body needs a "runInfo" property.');
        return;
      }
      Kickstarter = require("@arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        k.runInfo = input.runInfo;
        r = k.isHealthy();
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error6) {
        actions.resultException(req, res, error6, undefined, false);
      }
    }
    else {
      actions.resultError(req, res, actions.HTTP_BAD,
                          'Action '+action+' not yet implemented.');
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_cluster_check_port_GET
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/clusterCheckPort",
  prefix: false,

  callback : function (req, res) {
    if (ArangoServerState.disableDispatcherKickstarter() === true) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    if (req.requestType !== actions.GET) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    var port;
    if (!req.parameters.hasOwnProperty("port")) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "required parameter port was not given");
      return;
    }
    try {
      port = parseInt(req.parameters.port, 10);
      if (port < 1 || port > 65535) {
        throw "banana";
      }
    }
    catch (err) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "given port was not a proper integer");
      return;
    }
    try {
      var r = internal.testPort("tcp://0.0.0.0:" + port);
      res.responseCode = actions.HTTP_OK;
      res.contentType = "application/json; charset=utf-8";
      res.body = JSON.stringify(r);
    }
    catch (err2) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "exception in port test");
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_cluster_statistics_GET
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/clusterStatistics",
  prefix: false,

  callback : function (req, res) {
    if (req.requestType !== actions.GET) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                          "only GET requests are allowed");
      return;
    }
    if (!require("@arangodb/cluster").isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                          "only allowed on coordinator");
      return;
    }
    if (!req.parameters.hasOwnProperty("DBserver")) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "required parameter DBserver was not given");
      return;
    }
    var DBserver = req.parameters.DBserver;
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout:10 };
    var op = ArangoClusterComm.asyncRequest("GET","server:"+DBserver,"_system",
                                            "/_admin/statistics","",{},options);
    var r = ArangoClusterComm.wait(op);
    res.contentType = "application/json; charset=utf-8";
    if (r.status === "RECEIVED") {
      res.responseCode = actions.HTTP_OK;
      res.body = r.body;
    }
    else if (r.status === "TIMEOUT") {
      res.responseCode = actions.HTTP_BAD;
      res.body = JSON.stringify( {"error":true,
                                  "errorMessage": "operation timed out"});
    }
    else {
      res.responseCode = actions.HTTP_BAD;
      var bodyobj;
      try {
        bodyobj = JSON.parse(r.body);
      }
      catch (err) {
      }
      res.body = JSON.stringify( {"error":true,
        "errorMessage": "error from DBserver, possibly DBserver unknown",
        "body": bodyobj} );
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief allows to query the historic statistics of a DBserver in the cluster
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/history",
  prefix: false,

  callback : function (req, res) {
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                          "only POST requests are allowed");
      return;

    }
    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
        return;
    }
    var DBserver = req.parameters.DBserver;

    //build query
    var figures = body.figures;
    var filterString = " filter u.time > @startDate";
    var bind = {
      startDate: (new Date().getTime() / 1000) - 20 * 60
    };

    if (cluster.isCoordinator() && !req.parameters.hasOwnProperty("DBserver")) {
      filterString += " filter u.clusterId == @serverId";
      bind.serverId = cluster.coordinatorId();
    }

    var returnValue = " return u";
    if (figures) {
      returnValue = " return { time : u.time, server : {uptime : u.server.uptime} ";

      var groups = {};
      figures.forEach(function(f) {
          var g = f.split(".")[0];
          if (!groups[g]) {
              groups[g] = [];
          }
          groups[g].push(f.split(".")[1] + " : u." + f);
      });
      Object.keys(groups).forEach(function(key) {
          returnValue +=  ", " + key + " : {" + groups[key]  +"}";
      });
      returnValue += "}";
    }
    // allow at most ((60 / 10) * 20) * 2 documents to prevent total chaos
    var myQueryVal = "FOR u in _statistics " + filterString + " LIMIT 240 SORT u.time" + returnValue;

    if (! req.parameters.hasOwnProperty("DBserver")) {
        // query the local statistics collection
        var cursor = AQL_EXECUTE(myQueryVal, bind);
        res.contentType = "application/json; charset=utf-8";
        if (cursor instanceof Error) {
            res.responseCode = actions.HTTP_BAD;
            res.body = JSON.stringify( {"error":true,
                "errorMessage": "an error occurred"});
        }
        res.responseCode = actions.HTTP_OK;
        res.body = JSON.stringify({result : cursor.docs});
    }
    else {
        // query a remote statistics collection
        var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
        var options = { coordTransactionID: coord.coordTransactionID, timeout:10 };
        var op = ArangoClusterComm.asyncRequest("POST","server:"+DBserver,"_system",
            "/_api/cursor",JSON.stringify({query: myQueryVal, bindVars: bind}),{},options);
        var r = ArangoClusterComm.wait(op);
        res.contentType = "application/json; charset=utf-8";
        if (r.status === "RECEIVED") {
            res.responseCode = actions.HTTP_OK;
            res.body = r.body;
        }
        else if (r.status === "TIMEOUT") {
            res.responseCode = actions.HTTP_BAD;
            res.body = JSON.stringify( {"error":true,
                "errorMessage": "operation timed out"});
        }
        else {
            res.responseCode = actions.HTTP_BAD;
            var bodyobj;
            try {
                bodyobj = JSON.parse(r.body);
            }
            catch (err) {
            }
            res.body = JSON.stringify( {"error":true,
                "errorMessage": "error from DBserver, possibly DBserver unknown",
                "body": bodyobj} );
        }
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief bootstraps the all db servers
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/bootstrapDbServers",
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                          "only POST requests are allowed");
      return;
    }
    if (!require("@arangodb/cluster").isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                          "only allowed on coordinator");
      return;
    }
    var body = actions.getJsonBody(req, res);

    try {
      var result = cluster.bootstrapDbServers(body.isRelaunch);

      if (result) {
        actions.resultOk(req, res, actions.HTTP_OK);
      }
      else {
        actions.resultBad(req, res);
      }
    }
    catch(err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief bootstraps one db server
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/bootstrapDbServer",
  prefix: false,

  callback: function (req, res) {
    var body = actions.getJsonBody(req, res);

    UPGRADE_ARGS = {
      isCluster: true,
      isDbServer: true,
      isRelaunch: body.isRelaunch
    };

    try {
      var func = internal.loadStartup("server/bootstrap/db-server.js");
      var result = func && func();

      if (result) {
        actions.resultOk(req, res, actions.HTTP_OK);
      }
      else {
        actions.resultBad(req, res);
      }
    }
    catch(err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrade cluster database
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/upgradeClusterDatabase",
  prefix: false,

  callback: function (req, res) {
    var body = actions.getJsonBody(req, res);

    UPGRADE_ARGS = {
      isCluster: true,
      isCoordinator: true,
      isRelaunch: body.isRelaunch || false,
      upgrade: body.upgrade || false
    };

    try {
      var result = internal.loadStartup("server/upgrade-database.js");

      if (result) {
        actions.resultOk(req, res, actions.HTTP_OK);
      }
      else {
        actions.resultBad(req, res);
      }
    }
    catch(err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief bootstraps the coordinator
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/bootstrapCoordinator",
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    try {
      var func = internal.loadStartup("server/bootstrap/coordinator.js");
      var result = func && func();

      if (result) {
        actions.resultOk(req, res, actions.HTTP_OK);
      }
      else {
        actions.resultBad(req, res);
      }
    }
    catch(err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block JSF_getSecondary
/// (intentionally not in manual)
/// @brief gets the secondary of a primary DBserver
///
/// @ RESTHEADER{GET /_admin/cluster/getSecondary, Get secondary of a primary DBServer}
///
/// @ RESTQUERYPARAMETERS
///
/// @ RESTDESCRIPTION Gets the configuration in the agency of the secondary
/// replicating a primary.
///
/// @ RESTQUERYPARAMETERS
///
/// @ RESTQUERYPARAM{primary,string,required}
/// is the ID of the primary whose secondary we would like to get.
///
/// @ RESTQUERYPARAM{timeout,number,optional}
/// the timeout to use in HTTP requests to the agency, default is 60.
///
/// @ RESTRETURNCODES
///
/// @ RESTRETURNCODE{200} is returned when everything went well.
///
/// @ RESTRETURNCODE{400} the primary was not given as query parameter.
///
/// @ RESTRETURNCODE{403} server is not a coordinator or method was not GET.
///
/// @ RESTRETURNCODE{404} the given primary name is not configured in Agency.
///
/// @ RESTRETURNCODE{408} there was a timeout in the Agency communication.
///
/// @ RESTRETURNCODE{500} the get operation did not work.
///
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/getSecondary",
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.GET ||
        !require("@arangodb/cluster").isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                    "only GET requests are allowed and only to coordinators");
      return;
    }
    if (! req.parameters.hasOwnProperty("primary")) {
      actions.resultError(req, res, actions.HTTP_BAD, 0,
                          '"primary" is not given as parameter');
      return;
    }
    var primary = req.parameters.primary;

    var timeout = 60.0;

    try {
      if (req.parameters.hasOwnProperty("timeout")) {
        timeout = Number(req.parameters.timeout);
      }
    }
    catch (e) {
    }

    // Now get to work, first get the write lock on the Plan in the Agency:
    var success = ArangoAgency.lockRead("Plan", timeout);
    if (! success) {
      actions.resultError(req, res, actions.HTTP_REQUEST_TIMEOUT, 0,
                          "could not get a read lock on Plan in Agency");
      return;
    }

    try {
      var oldValue;
      try {
        oldValue = ArangoAgency.get("Plan/DBServers/" + primary, false, false);
      }
      catch (e1) {
        actions.resultError(req, res, actions.HTTP_NOT_FOUND, 0,
                  "Primary with the given ID is not configured in Agency.");
        return;
      }

      oldValue = oldValue["Plan/DBServers/" + primary];

      actions.resultOk(req, res, actions.HTTP_OK, { primary: primary,
                                                    secondary: oldValue } );
    }
    finally {
      ArangoAgency.unlockRead("Plan", timeout);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block JSF_replaceSecondary
/// (intentionally not in manual)
/// @brief exchanges the secondary of a primary DBserver
///
/// @ RESTHEADER{PUT /_admin/cluster/replaceSecondary, Replace secondary of a primary DBServer}
///
/// @ RESTDESCRIPTION Replaces the configuration in the agency of the secondary
/// replicating a primary. Use with care, because the old secondary will
/// relatively quickly delete its data. For security reasons and to avoid
/// races, the ID of the old secondary must be given as well.
///
/// @ RESTBODYPARAM{primary,string,required,string}
/// is the ID of the primary whose secondary is to be changed.
///
/// @ RESTBODYPARAM{oldSecondary,string,required,string}
/// is the old ID of the secondary.
///
/// @ RESTBODYPARAM{newSecondary,string,required,string}
/// is the new ID of the secondary.
///
/// @ RESTBODYPARAM{ttl,number,optional,number}
/// the time to live in seconds for the write lock, default is 60.
///
/// @ RESTBODYPARAM{timeout,number,optional,number}
/// the timeout to use in HTTP requests to the agency, default is 60.
///
/// @ RESTRETURNCODES
///
/// @ RESTRETURNCODE{200} is returned when everything went well.
///
/// @ RESTRETURNCODE{400} either one of the required body parameters was
/// not given or no server with this ID exists.
///
/// @ RESTRETURNCODE{403} server is not a coordinator or method was not PUT.
///
/// @ RESTRETURNCODE{404} the given primary name is not configured in Agency.
///
/// @ RESTRETURNCODE{408} there was a timeout in the Agency communication.
///
/// @ RESTRETURNCODE{412} the given oldSecondary was not the current secondary
/// of the given primary.
///
/// @ RESTRETURNCODE{500} the change operation did not work.
///
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/replaceSecondary",
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.PUT ||
        !require("@arangodb/cluster").isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                    "only PUT requests are allowed and only to coordinators");
      return;
    }
    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
      return;
    }
    if (! body.hasOwnProperty("primary") ||
        typeof(body.primary) !== "string" ||
        ! body.hasOwnProperty("oldSecondary") ||
        typeof(body.oldSecondary) !== "string" ||
        ! body.hasOwnProperty("newSecondary") ||
        typeof(body.newSecondary) !== "string") {
      actions.resultError(req, res, actions.HTTP_BAD, 0,
                          'not all three of "primary", "oldSecondary" and '+
                          '"newSecondary" are given in body and are strings');
      return;
    }

    var ttl = 60.0;
    var timeout = 60.0;

    if (body.hasOwnProperty("ttl") && typeof body.ttl === "number") {
      ttl = body.ttl;
    }
    if (body.hasOwnProperty("timeout") && typeof body.timeout === "number") {
      timeout = body.timeout;
    }

    // Now get to work, first get the write lock on the Plan in the Agency:
    var success = ArangoAgency.lockWrite("Plan", ttl, timeout);
    if (! success) {
      actions.resultError(req, res, actions.HTTP_REQUEST_TIMEOUT, 0,
                          "could not get a write lock on Plan in Agency");
      return;
    }

    try {
      var oldValue;
      try {
        oldValue = ArangoAgency.get("Plan/DBServers/" + body.primary, false,
                                    false);
      }
      catch (e1) {
        actions.resultError(req, res, actions.HTTP_NOT_FOUND, 0,
                  "Primary with the given ID is not configured in Agency.");
        return;
      }
      oldValue = oldValue["Plan/DBServers/"+body.primary];
      if (oldValue !== body.oldSecondary) {
        actions.resultError(req, res, actions.HTTP_PRECONDITION_FAILED, 0,
                            "Primary does not have the given oldSecondary as "+
                            "its secondary, current value: " + oldValue);
        return;
      }
      try {
        ArangoAgency.set("Plan/DBServers/" + body.primary, body.newSecondary,
                         0);
      }
      catch (e2) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
                            "Cannot change secondary of given primary.");
        return;
      }

      try {
        ArangoAgency.increaseVersion("Plan/Version");
      }
      catch (e3) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
                            "Cannot increase Plan/Version.");
        return;
      }

      actions.resultOk(req, res, actions.HTTP_OK, body);
    }
    finally {
      ArangoAgency.unlockWrite("Plan", timeout);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief changes responsibility for all shards from oldServer to newServer.
/// This needs to be done atomically!
////////////////////////////////////////////////////////////////////////////////

function changeAllShardReponsibilities (oldServer, newServer) {
  // This is only called when we have the write lock and we "only" have to
  // make sure that either all or none of the shards are moved.
  var l = ArangoAgency.get("Plan/Collections", true, false);
  var ll = Object.keys(l);

  var i = 0;
  var c;
  var oldShards = [];
  var shards;
  var names;
  var j;
  try {
    while (i < ll.length) {
      c = l[ll[i]];   // A collection entry
      shards = c.shards;
      names = Object.keys(shards);
      // Poor man's deep copy:
      oldShards.push(JSON.parse(JSON.stringify(shards)));
      for (j = 0; j < names.length; j++) {
        if (shards[names[j]] === oldServer) {
          shards[names[j]] = newServer;
        }
      }
      ArangoAgency.set(ll[i], c, 0);
      i += 1;
    }
  }
  catch (e) {
    i -= 1;
    while (i >= 0) {
      c = l[ll[i]];
      c.shards = oldShards[i];
      try {
        ArangoAgency.set(ll[i], c, 0);
      }
      catch (e2) {
      }
      i -= 1;
    }
    throw e;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block JSF_swapPrimaryAndSecondary
/// (intentionally not in manual)
/// @brief swaps the roles of a primary and secondary pair
///
/// @ RESTHEADER{PUT /_admin/cluster/swapPrimaryAndSecondary, Swaps the roles of a primary and secondary pair.}
///
/// @RESTDESCRIPTION Swaps the roles of a primary and replicating secondary 
/// pair. This includes changing the entry for all shards for which the 
/// primary was responsible to the name of the secondary. All changes happen
/// in a single write transaction (using a write lock) and the Plan/Version 
/// is increased. Use with care, because currently replication in the cluster
/// is asynchronous and the old secondary might not yet have all the data.
/// For security reasons and to avoid races, the ID of the old secondary
/// must be given as well.
///
/// @ RESTBODYPARAM{primary,string,required,string}
/// is the ID of the primary whose secondary is to be changed.
///
/// @ RESTBODYPARAM{secondary,string,required,string}
/// is the ID of the secondary, which must be the secondary of this primay.
///
/// @ RESTBODYPARAM{ttl,number,optional,number}
/// the time to live in seconds for the write lock, default is 60.
///
/// @ RESTBODYPARAM{timeout,number,optional,number}
/// the timeout to use in HTTP requests to the agency, default is 60.
///
/// @ RESTRETURNCODES
///
/// @ RESTRETURNCODE{200} is returned when everything went well.
///
/// @ RESTRETURNCODE{400} either one of the required body parameters was
/// not given or no server with this ID exists.
///
/// @ RESTRETURNCODE{403} server is not a coordinator or method was not PUT.
///
/// @ RESTRETURNCODE{404} the given primary name is not configured in Agency.
///
/// @ RESTRETURNCODE{408} there was a timeout in the Agency communication.
///
/// @ RESTRETURNCODE{412} the given secondary was not the current secondary
/// of the given primary.
///
/// @ RESTRETURNCODE{500} the change operation did not work.
///
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/swapPrimaryAndSecondary",
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.PUT ||
        !require("@arangodb/cluster").isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                    "only PUT requests are allowed and only to coordinators");
      return;
    }
    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
      return;
    }
    require("console").log("FUXX: " + JSON.stringify(body));
    if (! body.hasOwnProperty("primary") ||
        typeof(body.primary) !== "string" ||
        ! body.hasOwnProperty("secondary") ||
        typeof(body.secondary) !== "string") {
      actions.resultError(req, res, actions.HTTP_BAD, 0,
                          'not both "primary" and "secondary" '+
                          'are given in body and are strings');
      return;
    }

    var ttl = 60.0;
    var timeout = 60.0;

    if (body.hasOwnProperty("ttl") && typeof body.ttl === "number") {
      ttl = body.ttl;
    }
    if (body.hasOwnProperty("timeout") && typeof body.timeout === "number") {
      timeout = body.timeout;
    }

    // Now get to work, first get the write lock on the Plan in the Agency:
    var success = ArangoAgency.lockWrite("Plan", ttl, timeout);
    if (! success) {
      actions.resultError(req, res, actions.HTTP_REQUEST_TIMEOUT, 0,
                          "could not get a write lock on Plan in Agency");
      return;
    }

    try {
      var oldValue;
      try {
        oldValue = ArangoAgency.get("Plan/DBServers/" + body.primary, false,
                                    false);
      }
      catch (e1) {
        actions.resultError(req, res, actions.HTTP_NOT_FOUND, 0,
                  "Primary with the given ID is not configured in Agency.");
        return;
      }
      oldValue = oldValue["Plan/DBServers/"+body.primary];
      if (oldValue !== body.secondary) {
        actions.resultError(req, res, actions.HTTP_PRECONDITION_FAILED, 0,
                            "Primary does not have the given secondary as "+
                            "its secondary, current value: " + oldValue);
        return;
      }
      try {
        ArangoAgency.remove("Plan/DBServers/" + body.primary, false);
      }
      catch (e2) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
                            "Cannot remove old primary entry.");
        return;
      }
      try {
        ArangoAgency.set("Plan/DBServers/" + body.secondary,
                         body.primary, 0);
      }
      catch (e3) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
                            "Cannot set secondary as primary.");
        // Try to reset the old primary:
        try {
          ArangoAgency.set("Plan/DBServers/" + body.primary, 
                           body.secondary, 0);
        }
        catch (e4) {
          actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
                              "Cannot set secondary as primary, could not "+
                              "even reset the old value!");
        }
        return;
      }

      try {
        // Now change all responsibilities for shards to the "new" primary
        // body.secondary:
        changeAllShardReponsibilities(body.primary, body.secondary);
      }
      catch (e5) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
                            "Could not change responsibilities for shards.");
        // Try to reset the old primary:
        try {
          ArangoAgency.set("Plan/DBServers/" + body.primary, 
                           body.secondary, 0);
          ArangoAgency.remove("Plan/DBServers/" + body.secondary);
        }
        catch (e4) {
          actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
                              "Cannot change responsibility for shards and "+
                              "could not even reset the old value!");
        }
        return;
      }
        
      try {
        ArangoAgency.increaseVersion("Plan/Version");
      }
      catch (e3) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
                            "Cannot increase Plan/Version.");
        return;
      }
      
      actions.resultOk(req, res, actions.HTTP_OK, {primary: body.secondary,
                                                   secondary: body.primary});
    }
    finally {
      ArangoAgency.unlockWrite("Plan", timeout);
    }
  }
});

    

