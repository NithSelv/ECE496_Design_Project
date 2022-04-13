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

c = threading.Lock()
p = threading.Lock()
f = threading.Lock()
ports = []
loads = []
processes = []
finished = False

def recvmsg(sock):
   global ports
   global finished
   global loads
   while True:
      try: 
         f.acquire()
         if finished:
            f.release()
            sock.close()
            return
         f.release()
         connection, _ = sock.accept()
         x = b''
         while len(x) < 4:
            x = x + connection.recv(4-len(x))
         new_port = struct.unpack("!i", x)[0]
         c.acquire()
         temp = loads[:]
         size = len(ports)+1
         c.release()
         temp.append(0)
         for i in range(size):
            x = b''
            while len(x) < 4:
               x = x + connection.recv(4-len(x))
            temp[i] = struct.unpack("!i", x)[0]
         c.acquire()
         ports.append(new_port)
         loads = temp
         c.release()
         p.acquire()
         print("Received a new port: "+str(new_port))
         p.release()
         time.sleep(20)
      except socket.timeout:
         p.acquire()
         print("Socket timed out, closing connection!")
         p.release()
         sock.close()
         f.acquire()
         finished = True
         f.release()
         return
   
   
def sendreq(java_path, load):
   global ports
   global finished
   global loads
   global processes
   original_load = load
   while True:
      c.acquire()
      ports_copy = ports[:]
      loads_copy = loads[:]
      c.release()
      maxload = max(max(loads_copy), 5)
      for i in range(len(ports_copy)):
         if loads_copy[i] >= 5:
            num_iterations = 0
         else:
            num_iterations = min(maxload-loads_copy[i], load)
         for j in range(num_iterations):
            process = sendcmd(java_path, ports_copy[i])
            processes.append(process)
         load -= num_iterations
         if (load == 0):
            f.acquire()
            finished = True
            f.release()
            return
         c.acquire()
         loads_copy[i] = 4
         c.release()
      if load < original_load:
         p.acquire()
         print("Fired a round of requests!")
         p.release()
      time.sleep(20)

def sendcmd(java_path, port):
   args = [java_path]
   args.append("-XX:+UseJITServer")
   args.append("-XX:JITServerPort="+str(port))
   args.append("-Xjit:count=1"+86*"0")
   args.append("-version")
   process = subprocess.Popen(args, shell=False, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, close_fds=True)
   return process

def main(java_path, load):
   sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
   host = socket.gethostname()
   ip = socket.gethostbyname(host)
   sock.bind((host, 2000))
   sock.settimeout(100)
   sock.listen()
   t1 = threading.Thread(target=recvmsg, args=(sock,))
   t2 = threading.Thread(target=sendreq, args=(java_path,load,))

   t1.start()
   c.acquire()
   while len(ports) <= 0:
      c.release()
      time.sleep(5)
      c.acquire()
   c.release()
   t2.start()

   t1.join()
   t2.join()

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
