# ECE496_Design_Project

## Goal

The goal of this project is to design a HTTP/HTTPS server that can act as a liasion between Prometheus and the Open-Source JITServer project.

## Description

This can be done by allowing a Prometheus instance to send HTTP requests to the server (which will be running on a port specified in the Prometheus config file) which will allow it to scrape metrics from the endpoint. The server should provide those metrics from JITServer and return them to Prometheus in a valid HTTP Response. HTTPS can be configured on Prometheus as well for added security.

## Server Diagram

![image](https://user-images.githubusercontent.com/46902009/143926505-b89aab9d-cc62-436a-91a0-b1fd354a6de1.png)

## Current Progress

As of April, 8th, 2022, the current tasks have been implemented in the HTTP/HTTPS server:

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
- Server returns the desired metrics from JITServer and metrics related to itself and the host machine
- Server can use HTTP or HTTPS to have metrics scraped
- Server responds to liveness probes
- Instructions for integration with openj9 can be found in the README.md under `/server`

## Testing

The server can now obtain metrics from JITServer. In addition, as seen in the sample image below, we are able to display metrics that change over time. The computation of the metrics is done by our server for now and it is stored in a Metrics_Database object. The metrics can be scraped using HTTP or HTTPS.

![image](https://user-images.githubusercontent.com/46902009/143985492-37beb3c9-34eb-4e86-b305-19ac1f578a78.png)

## Upcoming Features

We plan to implement the following updates fairly soon:
- (HIGH PRIORITY) Set up demo
- (HIGH PRIORITY) Enhance BASH script (e.g. # of connections within 10s, execute JITServer instead of local server code)
