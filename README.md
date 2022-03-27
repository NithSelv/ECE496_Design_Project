# ECE496_Design_Project

## Goal

The goal of this project is to design a HTTP server that can act as a liasion between Prometheus and the Open-Source JITServer project.

## Description

This can be done by allowing a Prometheus instance to send HTTP requests to the server (which will be running on a port specified in the Prometheus config file) which will allow it to scrape metrics from the endpoint. The server should provide those metrics from JITServer and return them to Prometheus in a valid HTTP Response. 

## Server Diagram

![image](https://user-images.githubusercontent.com/46902009/143926505-b89aab9d-cc62-436a-91a0-b1fd354a6de1.png)

## Current Progress

As of March, 27th, 2022, the current tasks have been implemented in the HTTP server:

- Server can receive HTTP Requests from Prometheus
- Server can send HTTP Responses to Prometheus
- Server can parse the HTTP Requests
- Server can create a HTTP Response  
- Server can retrieve/update/store metrics using the `Metrics_Database` class
- Server can send back the following Http Responses:
  - `400 Bad Request` is sent if the request is not a GET request and the Host field is not provided 
  - `404 Resource Not Found` is sent if the client performs a GET request for an invalid endpoint
  - `200 Ok` is sent if the above issues do not occur, the Content-Type is text/plain, and the body includes all the metrics formatted as `metric_name metric_value` where each metric is separated by a new line character 
- Server can function within JITServer open-source project  

## Testing

The server can now obtain metrics from JITServer. In addition, as seen in the sample image below, we are able to display metrics that change over time. The computation of the metrics is done by our server for now and it is stored in a Metrics_Database object. 

![image](https://user-images.githubusercontent.com/46902009/143985492-37beb3c9-34eb-4e86-b305-19ac1f578a78.png)

## Future Updates

We plan to implement the following updates fairly soon:

- (LOW PRIORITY) Add a test for memory leaks (NEED TO CHECK)
- (LOW PRIORITY) (Additional Feature) Respond to liveliness props (i.e make sure the server can respond to requests that are used to check if the server is active)

## Current To-do

- Discuss Security (localhost, check IP, encrypt, password, etc.) (NEED TO CHECK) (SSL)
- Handle case where http encoded in HEX
- Enhance BASH script (e.g. # of connections within 10s)
