# ECE496_Design_Project

## Goal

The goal of this project is to design a HTTP server that can act as a liasion between Prometheus and the Open-Source JITServer project.

## Description

This can be done by allowing a Prometheus instance to send HTTP requests to the server (which will be running on a port specified in the Prometheus config file) which will allow it to scrape metrics from the endpoint. The server should provide those metrics from JITServer and return them to Prometheus in a valid HTTP Response. 

## Server Diagram

![image](https://user-images.githubusercontent.com/46902009/143926505-b89aab9d-cc62-436a-91a0-b1fd354a6de1.png)

Note: All arrows in blue identify code that has been completed and all arrows in red identify code that still needs to be written

## Current Progress

As of November, 29th, 2021, the current tasks have been implemented in the HTTP server:

- Server can receive HTTP Requests from Prometheus
- Server can send HTTP Responses from Prometheus
- Server can parse the HTTP Requests using the `Http_Request` class which will create a Linked-List of field, value pairs. Use `Find` method to search for the value of a particular field. Parse a given Http Request using the `Parse_Http_Request` method.
- Server can create a HTTP Response using the `Http_Response` class which will create a Linked-List of field, value pairs. Use `Add_Field` method to add Http Headers or Body content. Create the Http_Response using the `Prepare_Http_Response` method.  
- Server can retrieve/update/store metrics using the `Metrics_Database` class which will create a Linked-List of field, value pairs. Use `Find_Metric` method to find a particular metric value. To initialize and update all the metrics in the Metrics_Database use the `Initialize` and `Update` methods. To add new metrics or update a particular metric, use the `Add_Metric` and `Set_Metric` methods respectively.
- Server can send back the following Http Responses:
  - `401 Unauthorized` is sent is User-Agent is not Prometheus
  - `400 Bad Request` is sent if the request is not a GET request and the Host field is not provided 
  - `404 Resource Not Found` is sent if the client performs a GET request for an invalid endpoint
  - `303 See Other` is sent if the client requests for a port that the server is not running on/responsible for
  - `200 Ok` is sent if the above issues do not occur, the Content-Type is text/plain, and the body includes all the metrics formatted as `metric_name metric_value` where each metric is separated by a new line character 

## Future Updates

We plan to implement the following updates fairly soon:

- Server can handle multiple clients on different ports connecting
- Test scripts for testing the server (i.e this will be the "client" code and we may make use of `curl` and shell scripts to aid in the testing)
- Server can scrape metrics from JITServer and store them in the `Metrics_Database` object
- (Additional Feature) Respond to liveliness props (i.e make sure the server can respond to requests that are used to check if the server is active)
