#!/usr/bin/python3
import subprocess
import time 
import sys 
import socket
import requests
import json
import signal
import os

processes = []

# This code is used to setup the jitserver instances and transmit jitserver port numbers

# It will transmit port numbers over port 2000 to whoever is reading them. It's just a 4byte integer. Decode it and typecast to int to read it.

def send_new_port(port, msg):
   sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
   host = socket.gethostname()
   ip = socket.gethostbyname(host)
   sock.connect((ip, port))
   sock.send(str(msg).encode())
   sock.close()

def get_metrics():
   req = requests.get('https://localhost:9090/api/v1/query?query=numberOfClients{job="prometheus"}', verify=False)
   data = list(json.loads(req.content.decode('utf-8'))['data']['result'])
   jitserverClients = {};
   for i in range(len(data)):
      jitserverClients[int(data[i]['metric']['instance'].split(":")[1])] = int(data[i]['value'][1])
   return jitserverClients
   
def make_jitserver_instance(jitserver_path, port, metricport, httpsmetricport):
   args = [jitserver_path]
   args.append("-XX:JITServerPort="+str(port))
   args.append("-XX:MetricServerPort="+str(metricport))
   args.append("-XX:MetricHttpsPort="+str(httpsmetricport))
   process = subprocess.Popen(args, shell=False, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, close_fds=True)
   return process

def start_prometheus(prometheus_path):
   args = [prometheus_path+"/prometheus"]
   args.append("--web.config.file="+prometheus_path+"/web.yml")
   args.append("--config.file="+prometheus_path+"/prometheus.yml")
   process = subprocess.Popen(args, shell=False, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, close_fds=True)
   return process

def kill_processes(*args):
   for p in processes:
      p.terminate();
      p.wait();
      p.kill();
   print("Killed all JITServer and Prometheus instances!")
   sys.exit("Finished Running")
   

def main(jitserver_path, prometheus_path):
   jit_port = 38400
   metric_port = 7390
   https_port = 7391
   metric_ports = [7390]
   processes.append(make_jitserver_instance(jitserver_path, jit_port, metric_port, https_port))
   print("Created JITServer Instance No: "+str(len(processes)))
   print("Sending original port for listening on port 2000!")
   send_new_port(2000, jit_port)
   processes.append(start_prometheus(prometheus_path))
   print("Started Prometheus instance!")
   time.sleep(5)
   signal.signal(signal.SIGINT, kill_processes)
   while True: 
      time.sleep(5)
      load = get_metrics();
      for i in range(len(metric_ports)):
         if len(load.keys()) != 0 and load[metric_ports[i]] > 1:
            jit_port += 1
            metric_port += 2
            https_port += 2
            processes.append(make_jitserver_instance(jitserver_path, jit_port, metric_port, https_port))
            time.sleep(5)
            print("Created JITServer Instance No: "+str(len(metric_ports)))
            print("Please send some requests to port: "+str(jit_port))
            metric_ports.append(metric_port)
            send_new_port(2000, metric_port)
         if (len(load.keys()) != 0):
            print("JITServer Instance No: "+str(i+1)+" has "+str(load[metric_ports[i]])+" clients")

if __name__ == "__main__":
   if len(sys.argv) > 2 and os.path.exists(sys.argv[1]) and os.path.exists(sys.argv[2]):
      main(sys.argv[1], sys.argv[2])
   else:
      print("Please specify the path to jitserver executable as the first argument and the path to prometheus as the second argument!")
