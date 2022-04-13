#!/usr/bin/python3
import subprocess
import time 
import sys 
import socket
import requests
import json
import signal
import os
import threading
import struct

# This script is for firing compilation requests to jitserver and trying to keep the load at around 5 requests

finished = False

def recvmsg(sock, ports, loads):
   global finished
   while True:
      try: 
         if finished:
            sock.close()
            return
         sock.settimeout(50)
         connection, _ = sock.accept()
         x = b''
         while len(x) < 4:
            connection.settimeout(1)
            t = connection.recv(4-len(x))
            if len(t) == 0:
               break
            x = x + t
         x = x + (4-len(x)) * b'\x00'
         new_port = struct.unpack("!i", x)[0]
         ports.append(new_port)
         loads.append(0)
         for i in range(len(ports)):
            x = b''
            while len(x) < 4:
               connection.settimeout(1)
               t = connection.recv(4-len(x))
               if len(t) == 0:
                  break
               x = x + t
            x = x + (4 - len(x)) * b'\x00'
            loads[i] = struct.unpack("!i", x)[0]
         print("Received a new port: "+str(new_port))
         return
      except socket.timeout:
         print("Socket timed out, closing connection!")
         sock.close()
         finished = True
         return
   
   
def sendreq(java_path, load, sock, ports, loads, processes):
   original_load = load
   while True:
      if len(ports) > 0:
         maxload = max(max(loads), 5)
         for i in range(len(ports)):
            if loads[i] >= 5:
               num_iterations = 0
            else:
               num_iterations = min(maxload-loads[i], load)
            for j in range(num_iterations):
               process = sendcmd(java_path, ports[i])
               processes.append(process)
            load -= num_iterations
            if (load == 0):
               if not finished:
                  sock.close()
               return
            loads[i] = 4
         if load < original_load:
            print("Fired a round of requests!")
      if not finished:
         recvmsg(sock, ports, loads)

def sendcmd(java_path, port):
   args = [java_path]
   args.append("-XX:+UseJITServer")
   args.append("-XX:JITServerPort="+str(port))
   args.append("-Xjit:count=1"+86*"0")
   args.append("-version")
   process = subprocess.Popen(args, shell=False, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, close_fds=True)
   return process

def main(java_path, load):
   ports = []
   loads = []
   processes = []
   sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
   host = socket.gethostname()
   ip = socket.gethostbyname(host)
   sock.bind((host, 2000))
   sock.listen()
   sendreq(java_path, load, sock, ports, loads, processes)

   print("Finished sending requests!")
   for x in processes:
      x.terminate()
      x.wait()
      x.kill()
   print("Killed all leftover processes!")

if __name__ == "__main__":
   if len(sys.argv) > 2 and os.path.exists(sys.argv[1]):
      main(sys.argv[1], int(sys.argv[2]))
   else:
      print("Please specify the path to the java binary and the number of loads requests!")
